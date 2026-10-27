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
 * 16574 Hz 16-Bit Mono PCM, 8 WaveOut-Buffers a 331 Samples (~160ms Lookahead),
 * Ring ~8s. Null externe Abhaengigkeiten, null malloc.
 *
 * Lizenz: MIT (Clean Room)
 */

#include "win_audio.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

/* --- Ring-Puffer (statisch, kein malloc) -------------------------------- */
#define RING_SAMPLES (WIN_AUDIO_RATE * 8)   /* ~8 s @16574 Hz, ~265 KB */

static int16_t   s_ring[RING_SAMPLES];
static volatile long s_ring_head;           /* Leseposition (Audio-Thread) */
static volatile long s_ring_tail;           /* Schreibposition (Spiel-Thread) */
static int16_t   s_hold;                    /* letztes Sample (Underrun-Hold) */

/* --- WaveOut-Zustand ----------------------------------------------------- */
static HWAVEOUT  s_hWave = NULL;
static WAVEHDR   s_headers[WIN_AUDIO_BUFFERS];
static int16_t   s_buffers[WIN_AUDIO_BUFFERS][WIN_AUDIO_BUF_SAMPLES];
static int       s_active = 0;
static HANDLE    s_thread = NULL;
static volatile int s_thread_run = 0;

static void ring_reset(void) {
    s_ring_head = 0;
    s_ring_tail = 0;
    s_hold = 0;
}

/* Produzent (Spiel-Thread): blockiert nie. Overflow verwirft aelteste Samples.
 * Schreibt immer in den Ring (auch ohne offenes Geraet - selftest-kompatibel). */
void win_audio_write(const int16_t *samples, int count) {
    long tail;
    long head;
    int i;

    if (!samples || count <= 0) return;
    tail = s_ring_tail;
    head = s_ring_head;
    for (i = 0; i < count; i++) {
        if (tail - head >= RING_SAMPLES) {
            s_ring_head = tail - RING_SAMPLES + 1; /* aeltestes Sample verwerfen */
            head = s_ring_head;
        }
        s_ring[tail % RING_SAMPLES] = samples[i];
        s_hold = samples[i];
        tail++;
    }
    s_ring_tail = tail;
}

/* Konsument (Audio-Thread): liest n Samples; bei Underrun: letztes Sample halten. */
static void ring_read(int16_t *out, int n) {
    long head = s_ring_head;
    long tail = s_ring_tail;
    int i;

    for (i = 0; i < n; i++) {
        if (head < tail) {
            out[i] = s_ring[head % RING_SAMPLES];
            head++;
        } else {
            out[i] = s_hold; /* Underrun: halten (kein Stille-Knackser) */
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
                hdr->dwBufferLength = (DWORD)(WIN_AUDIO_BUF_SAMPLES * (int)sizeof(int16_t));
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
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = sample_rate > 0 ? (DWORD)sample_rate : (DWORD)WIN_AUDIO_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * 2;

    res = waveOutOpen(&s_hWave, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (res != MMSYSERR_NOERROR) {
        return 0;
    }

    ring_reset();

    memset(s_headers, 0, sizeof(s_headers));
    for (i = 0; i < WIN_AUDIO_BUFFERS; i++) {
        memset(s_buffers[i], 0, sizeof(s_buffers[i]));
        s_headers[i].lpData = (LPSTR)s_buffers[i];
        s_headers[i].dwBufferLength = (DWORD)(WIN_AUDIO_BUF_SAMPLES * (int)sizeof(int16_t));
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
 * Verifiziert: Wrap-around, Overflow (aelteste werden verworfen) und
 * Underrun-Hold. Laeuft auch headless (ohne Soundkarte). 0 = OK.            */
int win_audio_selftest(void) {
    static int16_t got[RING_SAMPLES];
    long n = RING_SAMPLES + 1000;
    int i;
    int fails = 0;

    ring_reset();

    /* 1) Wrap + Overflow: n Muster-Samples schreiben (1 pro Aufruf).
     *    Erwartet: genau die letzten RING_SAMPLES Samples sind lesbar. */
    for (i = 0; i < (int)n; i++) {
        int16_t v = (int16_t)(((long)i * 7) % 200 - 100);
        int16_t tmp[1];
        tmp[0] = v;
        win_audio_write(tmp, 1);
    }
    ring_read(got, RING_SAMPLES);
    for (i = 0; i < RING_SAMPLES; i++) {
        int16_t want = (int16_t)(((long)(n - RING_SAMPLES + i) * 7) % 200 - 100);
        if (got[i] != want) {
            fails++;
            if (fails > 100) return fails;
        }
    }

    /* 2) Underrun: mehr lesen als im Ring -> Rest muss s_hold sein
     *    (letztes geschriebenes Sample = Muster-Wert bei i = n-1). */
    {
        int16_t hold_expected = (int16_t)(((long)(n - 1) * 7) % 200 - 100);
        int k;
        for (k = 0; k < 4096; k++) {
            int16_t tmp[1];
            ring_read(tmp, 1);
            if (tmp[0] != hold_expected) {
                fails++;
                if (fails > 200) return fails;
            }
        }
    }

    /* 3) Weiterbetrieb nach Underun: 100 neue Samples, dann lesen. */
    {
        int16_t burst[100];
        for (i = 0; i < 100; i++) burst[i] = (int16_t)(50 - i);
        win_audio_write(burst, 100);
        ring_read(got, 100);
        for (i = 0; i < 100; i++) {
            if (got[i] != (int16_t)(50 - i)) {
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
void win_audio_close(void) {}
int  win_audio_is_active(void) { return 0; }
int  win_audio_selftest(void) { return 0; }
#endif
