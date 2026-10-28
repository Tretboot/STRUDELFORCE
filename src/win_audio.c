/*
 * win_audio.c - STRUDELFORCE Real-Time Low-Latency Audio Transport (v2)
 *
 * Architektur (v2, 2026-10-27 - fix fuer Knistern/Underruns):
 *   Der Spiel-Loop rendert deterministisch im 50Hz-Tick (game_audio_render /
 *   tfmx_player_render) und legt die Samples in einen statischen Ring-Puffer.
 *   Ein dedizierter, time-critical Audio-Thread speist die WaveOut-Geraete
 *   aus dem Ring. Damit ist die Audio-Glatte vom Frame-Render-Zeitpunkt
 *   (GDI/StretchDIBits) entkoppelt:
 *     - Underrun (langsame Frame)  -> letztes Sample wird gehalten (kein DC-Knackser)
 *     - Overflow (Spiel schneller) -> aelteste Samples werden verworfen
 *     - win_audio_write blockiert nie mehr (kein Spiel-Loop-Stau)
 *
 * 16574 Hz 16-Bit STEREO PCM (Soundgodizer-tauglich), 8 WaveOut-Buffers a
 * 331 Frames (~160ms Lookahead), Ring ~8s. Null externe Abhaengigkeiten, null malloc.
 *
 * Lizenz: MIT (Clean Room)
 */

#include "win_audio.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

/* --- Ring-Puffer (statisch, kein malloc) --------------------------------
 * v3 (2026-10-27): Ring speichert STEREO-Frames (L,R verflochten), damit
 * der Soundgodizer (Bass/Drive/Haas-Wide) nahtlos einspeisen kann.         */
#define RING_FRAMES (WIN_AUDIO_RATE * 8)    /* ~8 s @16574 Hz, ~530 KB */

static int16_t   s_ring[RING_FRAMES * 2];
static volatile long s_ring_head;           /* Leseposition (Audio-Thread)  */
static volatile long s_ring_tail;           /* Schreibposition (Spiel-Thread) */
static int16_t   s_hold[2];                 /* letztes Frame (Underrun-Hold) */

/* --- WaveOut-Zustand ----------------------------------------------------- */
static HWAVEOUT  s_hWave = NULL;
static WAVEHDR   s_headers[WIN_AUDIO_BUFFERS];
static int16_t   s_buffers[WIN_AUDIO_BUFFERS][WIN_AUDIO_BUF_SAMPLES * 2];
static int       s_active = 0;
static HANDLE    s_thread = NULL;
static volatile int s_thread_run = 0;

static void ring_reset(void) {
    s_ring_head = 0;
    s_ring_tail = 0;
    s_hold[0] = 0;
    s_hold[1] = 0;
}

/* Produzent (Spiel-Thread): blockiert nie. Overflow verwirft aelteste Frames.
 * Schreibt immer in den Ring (auch ohne offenes Geraet - selftest-kompatibel). */
void win_audio_write_frames(const int16_t *frames, int count) {
    long tail;
    long head;
    int i;

    if (!frames || count <= 0) return;
    tail = s_ring_tail;
    head = s_ring_head;
    for (i = 0; i < count; i++) {
        if (tail - head >= RING_FRAMES) {
            s_ring_head = tail - RING_FRAMES + 1; /* aeltestes Frame verwerfen */
            head = s_ring_head;
        }
        s_ring[(tail % RING_FRAMES) * 2]     = frames[i * 2];
        s_ring[(tail % RING_FRAMES) * 2 + 1] = frames[i * 2 + 1];
        s_hold[0] = frames[i * 2];
        s_hold[1] = frames[i * 2 + 1];
        tail++;
    }
    s_ring_tail = tail;
}

/* Mono-Quellpfad (z.B. Terminal/ohne Soundgodizer): dupliziert L=R. */
void win_audio_write(const int16_t *samples, int count) {
    int i;
    int16_t pair[WIN_AUDIO_BUF_SAMPLES * 2];

    if (!samples || count <= 0) return;
    if (count > WIN_AUDIO_BUF_SAMPLES) count = WIN_AUDIO_BUF_SAMPLES;
    for (i = 0; i < count; i++) {
        pair[i * 2]     = samples[i];
        pair[i * 2 + 1] = samples[i];
    }
    win_audio_write_frames(pair, count);
}

/* Konsument (Audio-Thread): liest n Frames; bei Underrun: letztes Frame halten. */
static void ring_read(int16_t *out, int n) {
    long head = s_ring_head;
    long tail = s_ring_tail;
    int i;

    for (i = 0; i < n; i++) {
        if (head < tail) {
            out[i * 2]     = s_ring[(head % RING_FRAMES) * 2];
            out[i * 2 + 1] = s_ring[(head % RING_FRAMES) * 2 + 1];
            head++;
        } else {
            out[i * 2]     = s_hold[0]; /* Underrun: halten (kein Stille-Knackser) */
            out[i * 2 + 1] = s_hold[1];
        }
    }
    s_ring_head = head;
}

/* Dedizierter Transport-Thread: fuellt die WaveOut-Buffers nach, wenn sie fertig sind. */
static DWORD WINAPI s_audio_thread(LPVOID param) {
    int cur = 0;

    (void)param;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while (s_thread_run) {
        int i;
        for (i = 0; i < WIN_AUDIO_BUFFERS; i++) {
            WAVEHDR *hdr = &s_headers[cur];
            if ((hdr->dwFlags & WHDR_DONE) && (hdr->dwFlags & WHDR_PREPARED)) {
                waveOutUnprepareHeader(s_hWave, hdr, sizeof(WAVEHDR));
                ring_read(s_buffers[cur], WIN_AUDIO_BUF_SAMPLES);
                hdr->dwBufferLength = (DWORD)(WIN_AUDIO_BUF_SAMPLES * 2 * (int)sizeof(int16_t));
                waveOutPrepareHeader(s_hWave, hdr, sizeof(WAVEHDR));
                waveOutWrite(s_hWave, hdr, sizeof(WAVEHDR));
            }
            cur = (cur + 1) % WIN_AUDIO_BUFFERS;
        }
        Sleep(1);
    }
    return 0;
}

int win_audio_open(int sample_rate) {
    WAVEFORMATEX wfx;
    MMRESULT res;
    int i;

    if (s_active) return 1;

    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = sample_rate > 0 ? (DWORD)sample_rate : (DWORD)WIN_AUDIO_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 4;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * 4;

    res = waveOutOpen(&s_hWave, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (res != MMSYSERR_NOERROR) {
        return 0;
    }

    ring_reset();

    memset(s_headers, 0, sizeof(s_headers));
    for (i = 0; i < WIN_AUDIO_BUFFERS; i++) {
        memset(s_buffers[i], 0, sizeof(s_buffers[i]));
        s_headers[i].lpData = (LPSTR)s_buffers[i];
        s_headers[i].dwBufferLength = (DWORD)(WIN_AUDIO_BUF_SAMPLES * 2 * (int)sizeof(int16_t));
        s_headers[i].dwFlags = 0;
        waveOutPrepareHeader(s_hWave, &s_headers[i], sizeof(WAVEHDR));
        waveOutWrite(s_hWave, &s_headers[i], sizeof(WAVEHDR));
    }

    s_thread_run = 1;
    s_thread = CreateThread(NULL, 0, s_audio_thread, NULL, 0, NULL);
    if (!s_thread) {
        s_thread_run = 0;
        waveOutReset(s_hWave);
        for (i = 0; i < WIN_AUDIO_BUFFERS; i++) {
            if (s_headers[i].dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(s_hWave, &s_headers[i], sizeof(WAVEHDR));
            }
        }
        waveOutClose(s_hWave);
        s_hWave = NULL;
        return 0;
    }

    s_active = 1;
    return 1;
}

void win_audio_close(void) {
    int i;

    if (!s_active) return;
    s_active = 0;
    s_thread_run = 0;
    if (s_thread) {
        WaitForSingleObject(s_thread, 300);
        CloseHandle(s_thread);
        s_thread = NULL;
    }
    if (s_hWave) {
        waveOutReset(s_hWave);
        for (i = 0; i < WIN_AUDIO_BUFFERS; i++) {
            if (s_headers[i].dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(s_hWave, &s_headers[i], sizeof(WAVEHDR));
            }
        }
        waveOutClose(s_hWave);
        s_hWave = NULL;
    }
}

int win_audio_is_active(void) {
    return s_active;
}

/* --- Selftest (nur Ring-Logik, kein Geraet) ------------------------------
 * Verifiziert: Wrap-around, Overflow (aelteste werden verworfen),
 * Underrun-Hold und den Mono-Wrapper. Laeuft auch headless. 0 = OK.       */
int win_audio_selftest(void) {
    static int16_t got[RING_FRAMES * 2];
    long n = RING_FRAMES + 1000;
    int i;
    int fails = 0;

    ring_reset();

    /* 1) Wrap + Overflow: n Muster-Frames schreiben (1 pro Aufruf).
     *    Erwartet: genau die letzten RING_FRAMES Frames sind lesbar. */
    for (i = 0; i < (int)n; i++) {
        int16_t v = (int16_t)(((long)i * 7) % 200 - 100);
        int16_t frame[2];
        frame[0] = v;
        frame[1] = (int16_t)(v * 2);
        win_audio_write_frames(frame, 1);
    }
    ring_read(got, RING_FRAMES);
    for (i = 0; i < (int)RING_FRAMES; i++) {
        long k = n - RING_FRAMES + i;
        int16_t wantL = (int16_t)((k * 7) % 200 - 100);
        int16_t wantR = (int16_t)(wantL * 2);
        if (got[i * 2] != wantL || got[i * 2 + 1] != wantR) {
            fails++;
            if (fails > 100) return fails;
        }
    }

    /* 2) Underrun: mehr lesen als im Ring -> Rest muss das letztes Frame sein. */
    {
        long k = n - 1;
        int16_t holdL = (int16_t)((k * 7) % 200 - 100);
        int16_t holdR = (int16_t)(holdL * 2);
        int kk;
        for (kk = 0; kk < 4096; kk++) {
            int16_t f[2];
            ring_read(f, 1);
            if (f[0] != holdL || f[1] != holdR) {
                fails++;
                if (fails > 200) return fails;
            }
        }
    }

    /* 3) Weiterbetrieb nach Underrun: 100 neue Frames, dann lesen. */
    {
        int16_t burst[100 * 2];
        for (i = 0; i < 100; i++) {
            burst[i * 2]     = (int16_t)(50 - i);
            burst[i * 2 + 1] = (int16_t)(60 - i);
        }
        win_audio_write_frames(burst, 100);
        ring_read(got, 100);
        for (i = 0; i < 100; i++) {
            if (got[i * 2] != (int16_t)(50 - i) || got[i * 2 + 1] != (int16_t)(60 - i)) {
                fails++;
            }
        }
    }

    /* 4) Mono-Wrapper: L=R-Duplizierung. */
    {
        int16_t mono[4];
        mono[0] = 1000; mono[1] = -2000; mono[2] = 3000; mono[3] = -4000;
        win_audio_write(mono, 4);
        ring_read(got, 4);
        for (i = 0; i < 4; i++) {
            if (got[i * 2] != mono[i] || got[i * 2 + 1] != mono[i]) {
                fails++;
            }
        }
    }

    return fails;
}

#else
/* Non-Windows Stub (identische API) */
int  win_audio_open(int sample_rate) { (void)sample_rate; return 0; }
void win_audio_write(const int16_t *samples, int count) { (void)samples; (void)count; }
void win_audio_write_frames(const int16_t *frames, int n) { (void)frames; (void)n; }
void win_audio_close(void) {}
int  win_audio_is_active(void) { return 0; }
int  win_audio_selftest(void) { return 0; }
#endif
