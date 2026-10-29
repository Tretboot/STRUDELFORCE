/*
 * tfmx_player.c - STRUDELFORCE TFMX 1.5 / Pro / 7V Player, Rewrite v2
 *
 * Logic mirror of tools/tfmx_oracle.py. Integer only, C89 style
 * (declarations at block start, no // comments, no VLAs).
 *
 * Fixed vs. v1:
 *  - notes are routed by the voice nibble (byte2 & 0x0F), not by track index
 *  - trackstep advance is driven by pattern cmd F0, not by a step timer
 *  - trackstep words: 0x80..0x8F continue, 0xFE stop+kill, 0xFF idle, EFFE cmds
 *  - pattern/ macro waits use the correct bytes (pattern b3/F3 b1, macro 04 w1)
 *  - full macro set incl. Paula register latch semantics (SampleLoop, OneShot)
 *  - TFMX period table + finetune instead of a float pow() approximation
 *  - CIA/BPM tempo, speed counter, master fade, TFHD + TFMXPAK containers
 *  - no 330 KB fixed buffers inside the struct (stack overflow in game_main)
 *
 * Lizenz: MIT (Clean Room)
 */
#define _CRT_SECURE_NO_WARNINGS
#include "tfmx_player.h"
#include "game_audio.h" /* nur hier (Implementierung) - legacy tick braucht das Struct */
#include "amiga_crt.h"  /* M68K: sf_memset/sf_memcmp (kein string.h unter -nostdlib) */
#ifndef M68K
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#define PAULA_CLOCK   3546895UL
#define CIA_STEP      44336L          /* 709379 >> 4 */
#define CIA_DIVIDEND  0x1B51F8UL
#define MAX_MACRO_CMDS 64
#define MAX_PATT_CMDS  128
#define VMASK (TFMX_VOICES - 1)

static const uint16_t notevals[64] = {
 0x6AE,0x64E,0x5F4,0x59E,0x54D,0x501,0x4B9,0x475,0x435,0x3F9,0x3C0,0x38C,
 0x358,0x32A,0x2FC,0x2D0,0x2A8,0x282,0x25E,0x23B,0x21B,0x1FD,0x1E0,0x1C6,
 0x1AC,0x194,0x17D,0x168,0x154,0x140,0x12F,0x11E,0x10E,0x0FE,0x0F0,0x0E3,
 0x0D6,0x0CA,0x0BF,0x0B4,0x0AA,0x0A0,0x097,0x08F,0x087,0x07F,0x078,0x071,
 0x0D6,0x0CA,0x0BF,0x0B4,0x0AA,0x0A0,0x097,0x08F,0x087,0x07F,0x078,0x071,
 0x0D6,0x0CA,0x0BF,0x0B4 };

/* ------------------------------------------------------------------ */
static uint16_t rd16(const TfmxPlayer *p, uint32_t o) {
    if (o + 2 > p->mdat_size) return 0;
    return (uint16_t)((p->mdat[o] << 8) | p->mdat[o + 1]);
}
static uint32_t rd32(const TfmxPlayer *p, uint32_t o) {
    if (o + 4 > p->mdat_size) return 0;
    return ((uint32_t)p->mdat[o] << 24) | ((uint32_t)p->mdat[o + 1] << 16) |
           ((uint32_t)p->mdat[o + 2] << 8) | (uint32_t)p->mdat[o + 3];
}
static void rdcmd(TfmxPlayer *p, uint32_t o, uint8_t *b) {
    if (o + 4 <= p->mdat_size) {
        b[0] = p->mdat[o]; b[1] = p->mdat[o + 1]; b[2] = p->mdat[o + 2]; b[3] = p->mdat[o + 3];
    } else {
        p->oob_reads++; b[0] = 0xF4; b[1] = b[2] = b[3] = 0;   /* safe stop */
    }
}
static int32_t patt_addr(TfmxPlayer *p, unsigned n) {
    uint32_t a = rd32(p, p->patt_off + 4u * n);
    if (a == 0 || a >= p->mdat_size) { p->oob_reads++; return -1; }
    return (int32_t)a;
}
static int32_t macro_addr(TfmxPlayer *p, unsigned n) {
    uint32_t a = rd32(p, p->macro_off + 4u * n);
    if (a == 0 || a >= p->mdat_size) { p->oob_reads++; return -1; }
    return (int32_t)a;
}
static int16_t clamp_vol(int v) { return (int16_t)(v < 0 ? 0 : (v > 64 ? 64 : v)); }

/* ------------------------------------------------------------------ */
void tfmx_player_init(TfmxPlayer *p) {
    memset(p, 0, sizeof(*p));
    p->rate = GAME_AUDIO_RATE;
    p->step_tempo = 5;
}

void tfmx_player_free(TfmxPlayer *p) {
#ifndef M68K
    if (p->owned_buf) free(p->owned_buf);
#endif
    /* M68K: owned_buf wird nie allokiert (Load via EMBED/INCBIN) */
    p->owned_buf = NULL;
    p->loaded = p->playing = 0;
}

static int parse_header(TfmxPlayer *p) {
    const uint8_t *m = p->mdat;
    uint32_t lim, o[3];
    int i, k;
    if (p->mdat_size < 0x200) return 0;
    if (memcmp(m, "TFMX-SONG", 9) != 0) {
        char up[8];
        for (i = 0; i < 8; i++) up[i] = (char)((m[i] >= 'a' && m[i] <= 'z') ? m[i] - 32 : m[i]);
        if (memcmp(up, "TFMXSONG", 8) != 0) return 0;
    }
    /* title: 6 lines x 40 chars at 0x10 (nur PC-Build: nutzt strncat/strlen) */
    p->title[0] = '\0';
#ifndef M68K
    for (i = 0; i < 6; i++) {
        char line[41]; int e;
        memcpy(line, m + 0x10 + i * 40, 40); line[40] = '\0';
        for (k = 0; k < 40; k++) if ((unsigned char)line[k] < 32) line[k] = ' ';
        for (e = 39; e >= 0 && line[e] == ' '; e--) line[e] = '\0';
        if (line[0]) {
            if (p->title[0]) strncat(p->title, " | ", sizeof(p->title) - strlen(p->title) - 1);
            strncat(p->title, line, sizeof(p->title) - strlen(p->title) - 1);
        }
    }
#else
    (void)m; (void)k;
#endif
    p->track_off = rd32(p, 0x1D0); p->patt_off = rd32(p, 0x1D4); p->macro_off = rd32(p, 0x1D8);
    if (!p->track_off && !p->patt_off && !p->macro_off) {   /* TFMX 1.5 fixed layout */
        p->track_off = 0x800; p->patt_off = 0x400; p->macro_off = 0x600;
    }
    if (p->track_off >= p->mdat_size || p->patt_off >= p->mdat_size || p->macro_off >= p->mdat_size)
        return 0;
    lim = p->mdat_size;
    o[0] = p->patt_off; o[1] = p->macro_off; o[2] = rd32(p, p->patt_off);
    for (i = 0; i < 3; i++) if (o[i] > p->track_off && o[i] < lim) lim = o[i];
    k = (int)((lim - p->track_off) / 16);
    p->num_steps = (uint16_t)(k > 0xFFFF ? 0xFFFF : k);
    p->num_subsongs = 0;
    for (i = 0; i < 31; i++) {
        uint16_t st = rd16(p, 0x100 + 2 * i), en = rd16(p, 0x140 + 2 * i);
        if (i > 0 && st == 0 && en == 0) continue;
        if (en >= st && en < p->num_steps) p->num_subsongs = (uint8_t)(i + 1);
    }
    return 1;
}

int tfmx_player_load_mem(TfmxPlayer *p, const uint8_t *mdat, uint32_t mdat_size,
                         const int8_t *smpl, uint32_t smpl_size) {
    uint8_t *keep = p->owned_buf;
    tfmx_player_init(p);
    p->owned_buf = keep;
    if (!mdat || mdat_size < 18) return 0;
    if (!smpl && memcmp(mdat, "TFHD", 4) == 0) {
        uint32_t h = ((uint32_t)mdat[4] << 24) | ((uint32_t)mdat[5] << 16) | ((uint32_t)mdat[6] << 8) | mdat[7];
        uint32_t ms = ((uint32_t)mdat[10] << 24) | ((uint32_t)mdat[11] << 16) | ((uint32_t)mdat[12] << 8) | mdat[13];
        uint32_t ss = ((uint32_t)mdat[14] << 24) | ((uint32_t)mdat[15] << 16) | ((uint32_t)mdat[16] << 8) | mdat[17];
        if (h > mdat_size || ms > mdat_size - h) return 0;
        if (ss > mdat_size - h - ms) ss = mdat_size - h - ms;
        smpl = (const int8_t *)(mdat + h + ms); smpl_size = ss;
        mdat += h; mdat_size = ms;
    } else if (!smpl && memcmp(mdat, "TFMXPAK ", 8) == 0) {
        unsigned long ms = 0, ss = 0; uint32_t i = 8;
        while (i < mdat_size && mdat[i] == ' ') i++;
        while (i < mdat_size && mdat[i] >= '0' && mdat[i] <= '9') ms = ms * 10 + (mdat[i++] - '0');
        while (i < mdat_size && mdat[i] == ' ') i++;
        while (i < mdat_size && mdat[i] >= '0' && mdat[i] <= '9') ss = ss * 10 + (mdat[i++] - '0');
        while (i + 3 <= mdat_size && memcmp(mdat + i, ">>>", 3) != 0) i++;
        i += 3;
        if (i > mdat_size || ms > mdat_size - i) return 0;
        if (ss > mdat_size - i - ms) ss = mdat_size - i - ms;
        smpl = (const int8_t *)(mdat + i + ms); smpl_size = (uint32_t)ss;
        mdat += i; mdat_size = (uint32_t)ms;
    }
    p->mdat = mdat; p->mdat_size = mdat_size;
    p->smpl = smpl; p->smpl_size = smpl ? smpl_size : 0;
    if (!parse_header(p)) return 0;
    p->loaded = 1;
    return 1;
}

#ifndef M68K
static long file_size(FILE *f) {
    long n;
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    return n;
}

int tfmx_player_load(TfmxPlayer *p, const char *tfx_path, const char *sam_path) {
    FILE *f, *g = NULL;
    long ms, ss = 0;
    char auto_sam[600];
    uint8_t *buf;
    int ok;

    tfmx_player_free(p);
    f = fopen(tfx_path, "rb");
    if (!f) return 0;
    ms = file_size(f);
    if (ms < 18 || ms > 8L * 1024 * 1024) { fclose(f); return 0; }

    if (!sam_path || !sam_path[0]) {
        const char *base = strrchr(tfx_path, '/');
        const char *b2 = strrchr(tfx_path, '\\');
        char *ext;
        if (b2 && (!base || b2 > base)) base = b2;
        base = base ? base + 1 : tfx_path;
        strncpy(auto_sam, tfx_path, sizeof(auto_sam) - 8);
        auto_sam[sizeof(auto_sam) - 8] = '\0';
        if (strncmp(base, "mdat.", 5) == 0 || strncmp(base, "MDAT.", 5) == 0) {
            size_t bo = (size_t)(base - tfx_path);
            if (bo + 5 < sizeof(auto_sam)) memcpy(auto_sam + bo, base[0] == 'm' ? "smpl." : "SMPL.", 5);
        } else {
            ext = strrchr(auto_sam, '.');
            if (ext && ext > auto_sam + (base - tfx_path)) strcpy(ext, ".sam");
            else strcat(auto_sam, ".sam");
        }
        sam_path = auto_sam;
    }
    g = fopen(sam_path, "rb");
    if (g) { ss = file_size(g); if (ss < 0 || ss > 8L * 1024 * 1024) ss = 0; }

    buf = (uint8_t *)malloc((size_t)(ms + ss + 4));    /* load time only */
    if (!buf) { fclose(f); if (g) fclose(g); return 0; }
    if (fread(buf, 1, (size_t)ms, f) != (size_t)ms) ms = 0;
    fclose(f);
    if (g) {
        if (ss && fread(buf + ms, 1, (size_t)ss, g) != (size_t)ss) ss = 0;
        fclose(g);
    }
    p->owned_buf = buf;
    if (ss > 0)
        ok = tfmx_player_load_mem(p, buf, (uint32_t)ms, (const int8_t *)(buf + ms), (uint32_t)ss);
    else
        ok = tfmx_player_load_mem(p, buf, (uint32_t)ms, NULL, 0);   /* TFHD / TFMXPAK */
    if (!ok) tfmx_player_free(p);
    return ok;
}
#endif /* !M68K — Amiga-Build laedt per tfmx_player_load_mem() aus INCBIN-Daten */

/* ------------------------------------------------------------------ */
void tfmx_player_set_rate(TfmxPlayer *p, uint32_t rate) {
    if (rate < 4000) rate = 4000;
    if (rate > 96000) rate = 96000;
    p->rate = rate;
}

static void set_cia(TfmxPlayer *p, uint16_t bpm) {
    uint32_t timer = bpm ? (uint32_t)(CIA_DIVIDEND / bpm) : 14188UL;
    p->spj_fp = timer * (p->rate >> 4) + ((timer * (p->rate & 15)) >> 4);
    p->bpm = bpm;
}

static void start_fade(TfmxPlayer *p, uint8_t speed, uint8_t dest) {
    p->fade_dest = (int16_t)(dest > 64 ? 64 : dest);
    if (speed == 0 || p->fade_dest == p->master) {
        p->master = p->fade_dest; p->fade_time = 0; return;
    }
    p->fade_reset = p->fade_time = speed;
    p->fade_step = (int16_t)(p->fade_dest > p->master ? 1 : -1);
}

static void voice_reset(TfmxVoice *v) {
    memset(v, 0, sizeof(*v));
    v->mloop = -1; v->dma_wait = -1; v->reg_len = 1; v->lat_bytes = 2;
}

static void get_trackstep(TfmxPlayer *p) {
    int guard = 0, i;
    for (;;) {
        uint16_t w[8];
        uint32_t o;
        if (++guard > 64 || p->pos >= p->num_steps) { p->playing = 0; return; }
        o = p->track_off + 16u * p->pos;
        for (i = 0; i < 8; i++) w[i] = rd16(p, o + 2u * i);
        if (w[0] == 0xEFFE) {
            switch (w[1]) {
            case 0: p->playing = 0; return;
            case 1:
                if (p->tloop == 0) { p->tloop = -1; p->pos++; continue; }
                else if (p->tloop < 0) p->tloop = (int16_t)(w[3] - 1);
                else p->tloop--;
                p->pos = w[2];
                continue;
            case 2:
                p->speed = p->step_tempo = w[2]; p->speed_cnt = 0;
                if ((w[3] & 0x1FF) >= 0x10 && !(w[3] & 0xF000)) set_cia(p, (uint16_t)(w[3] & 0x1FF));
                p->pos++; continue;
            case 4:
                start_fade(p, (uint8_t)(w[2] & 0xFF), (uint8_t)(w[3] & 0xFF));
                p->pos++; continue;
            default:   /* 3 = 7V timeshare: we have 8 real voices, ignore */
                p->pos++; continue;
            }
        }
        for (i = 0; i < TFMX_TRACKS; i++) {
            TfmxTrack *tr = &p->t[i];
            uint8_t pn = (uint8_t)(w[i] >> 8);
            int8_t xp = (int8_t)(w[i] & 0xFF);
            if (pn < 0x80) {
                int32_t a = patt_addr(p, pn);
                if (a < 0) { tr->pnum = 0xFF; continue; }
                tr->pnum = pn; tr->paddr = (uint32_t)a; tr->pstep = 0; tr->pwait = 0;
                tr->ploop = -1; tr->xpose = xp;
            } else if (pn == 0xFE) {
                TfmxVoice *v = &p->v[(uint8_t)xp & VMASK];
                tr->pnum = 0xFF; v->mrun = 0; v->dma = 0;
            } else if (pn == 0xFF) {
                tr->pnum = 0xFF;
            } else {
                tr->xpose = xp;          /* continue running pattern */
            }
        }
        return;
    }
}

void tfmx_player_start(TfmxPlayer *p, uint8_t sub) {
    int i;
    uint16_t tp;
    if (!p->loaded) return;
    if (sub >= (p->num_subsongs ? p->num_subsongs : 1)) sub = 0;
    p->current_subsong = sub;
    for (i = 0; i < TFMX_VOICES; i++) voice_reset(&p->v[i]);
    for (i = 0; i < TFMX_TRACKS; i++) { memset(&p->t[i], 0, sizeof(TfmxTrack)); p->t[i].pnum = 0xFF; p->t[i].ploop = -1; }
    p->first = rd16(p, 0x100 + 2u * sub); p->last = rd16(p, 0x140 + 2u * sub); p->pos = p->first;
    p->speed = 1; set_cia(p, 0); /* Default speed 1 (Turrican 3 / TFMX Pro double speed arcade tempo) */
    tp = rd16(p, 0x180 + 2u * sub);
    if (tp >= 0x10) { set_cia(p, tp); p->speed = 0; }
    else if (tp) p->speed = tp;
    p->step_tempo = p->speed;
    p->speed_cnt = 0; p->tloop = -1; p->master = 64;
    p->fade_dest = 64; p->fade_time = 0; p->fade_reset = 0; p->fade_step = 0;
    p->jiffy = 0; p->acc_fp = 0; p->oob_reads = 0;
    p->playing = 1;
    get_trackstep(p);
}

void tfmx_player_stop(TfmxPlayer *p) {
    int i;
    p->playing = 0;
    for (i = 0; i < TFMX_VOICES; i++) { p->v[i].dma = 0; p->v[i].mrun = 0; p->v[i].vol = 0; }
}

/* ------------------------------------------------------------------ */
static void note_port(TfmxPlayer *p, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    TfmxVoice *v = &p->v[b2 & VMASK];
    if (b0 < 0xC0) {
        int32_t a;
        v->prevnote = v->note; v->note = (uint8_t)(b0 & 0x3F);
        v->velo = (uint8_t)((b2 >> 4) & 0xF); v->finetune = (int8_t)b3;
        v->keyup = 0; v->mloop = -1; v->dma_wait = -1;
        a = macro_addr(p, b1);
        if (a < 0) { v->mrun = 0; return; }
        v->mptr = (uint32_t)a; v->mstep = 0; v->mwait = 0; v->mrun = 1;
    } else if (b0 < 0xF0) {
        v->porta_reset = b1; v->porta_time = 1;
        if (!v->porta_rate) v->porta_per = v->dest;
        v->porta_rate = b3;
        v->prevnote = v->note; v->note = (uint8_t)(b0 & 0x3F);
        v->dest = notevals[v->note];
    } else if (b0 == 0xF5) {
        v->keyup = 1;
    } else if (b0 == 0xF6) {
        v->vib_reset = (uint8_t)(b1 & 0xFE); v->vib_time = (uint8_t)(v->vib_reset >> 1);
        v->vib_width = (int8_t)b3; v->vib_off = 0;
    } else if (b0 == 0xF7) {
        v->env_rate = 1; v->env_reset = v->env_time = (uint8_t)(b1 ? b1 : 1);
        v->env_end = (uint8_t)(b3 > 64 ? 64 : b3);
    }
}

/* returns 1 when F0 (end of trackstep) was hit */
static int do_track(TfmxPlayer *p, int i) {
    TfmxTrack *tr = &p->t[i];
    int n;
    uint8_t b[4];
    if (tr->pnum >= 0x90) return 0;
    if (tr->pwait) { tr->pwait--; return 0; }
    for (n = 0; n < MAX_PATT_CMDS; n++) {
        rdcmd(p, tr->paddr + 4u * tr->pstep, b);
        tr->pstep++;
        if (b[0] < 0xF0) {
            int wait = (b[0] & 0xC0) == 0x80;
            uint8_t note, det = b[3];
            if (wait) { tr->pwait = b[3]; det = 0; }
            note = (uint8_t)((b[0] + tr->xpose) & 0x3F);
            if ((b[0] & 0xC0) == 0xC0) note |= 0xC0;
            note_port(p, note, b[1], b[2], det);
            if (wait) return 0;
            continue;
        }
        switch (b[0]) {
        case 0xF0: tr->pnum = 0xFF; return 1;
        case 0xF1:
            if (tr->ploop == 0) { tr->ploop = -1; break; }
            else if (tr->ploop < 0) tr->ploop = (int16_t)(b[1] - 1);
            else tr->ploop--;
            tr->pstep = (uint16_t)((b[2] << 8) | b[3]);
            break;
        case 0xF2: {
            int32_t a = patt_addr(p, b[1]);
            if (a < 0) { tr->pnum = 0xFF; return 0; }
            tr->pnum = b[1]; tr->paddr = (uint32_t)a;
            tr->pstep = (uint16_t)((b[2] << 8) | b[3]); tr->ploop = -1;
            break; }
        case 0xF3: tr->pwait = b[1]; return 0;
        case 0xF4: tr->pnum = 0xFF; return 0;
        case 0xF5: case 0xF6: case 0xF7:
            note_port(p, b[0], b[1], b[2], b[3]);
            break;
        case 0xF8: {
            int32_t a = patt_addr(p, b[1]);
            if (a < 0) break;
            tr->raddr = tr->paddr; tr->rstep = tr->pstep;
            tr->paddr = (uint32_t)a; tr->pstep = (uint16_t)((b[2] << 8) | b[3]);
            break; }
        case 0xF9: tr->paddr = tr->raddr; tr->pstep = tr->rstep; break;
        case 0xFA: start_fade(p, b[1], b[3]); break;
        case 0xFB: {
            TfmxTrack *t2 = &p->t[b[2] & 7];
            int32_t a = patt_addr(p, b[1]);
            if (a >= 0) {
                t2->pnum = b[1]; t2->paddr = (uint32_t)a; t2->pstep = 0; t2->pwait = 0;
                t2->ploop = -1; t2->xpose = (int8_t)b[3];
            }
            break; }
        case 0xFE: p->t[b[2] & 7].pnum = 0xFF; break;
        default: break;   /* FC lock, FD cue, FF nop */
        }
    }
    tr->pnum = 0xFF;
    return 0;
}

static void do_tracks(TfmxPlayer *p) {
    uint8_t done[TFMX_TRACKS];
    int i = 0, k, restarts = 0;
    memset(done, 0, sizeof(done));
    while (i < TFMX_TRACKS) {
        if (!done[i]) {
            done[i] = 1;
            if (do_track(p, i)) {
                p->pos = (uint16_t)(p->pos >= p->last ? p->first : p->pos + 1);
                get_trackstep(p);
                if (!p->playing) return;
                if (++restarts > 4) return;
                for (k = 0; k < TFMX_TRACKS; k++)
                    if (p->t[k].pnum < 0x80 && p->t[k].pstep == 0) done[k] = 0;
                i = 0;
                continue;
            }
        }
        i++;
    }
}

/* ------------------------------------------------------------------ */
static void set_period_from(TfmxVoice *v, int note, int fine) {
    int32_t b = notevals[note & 0x3F];
    b = (b * (fine + v->finetune + 0x100)) >> 8;
    if (b < 0) b = 0;
    if (b > 0xFFFF) b = 0xFFFF;
    v->dest = (uint16_t)b;
    if (!v->porta_rate) v->cur = (uint16_t)b;
}

static void paula_on(TfmxVoice *v) {
    v->lat_start = v->reg_start & 0xFFFFFEUL;
    v->lat_bytes = (uint32_t)(v->reg_len ? v->reg_len : 0x10000UL) * 2u;
    v->pos_fp = 0; v->dma = 1;
}

static void do_macro(TfmxPlayer *p, TfmxVoice *v) {
    int n;
    uint8_t b[4];
    if (!v->mrun) return;
    if (v->mwait) { v->mwait--; return; }
    if (v->dma_wait >= 0) return;
    for (n = 0; n < MAX_MACRO_CMDS; n++) {
        uint16_t w1;
        rdcmd(p, v->mptr + 4u * v->mstep, b);
        v->mstep++;
        w1 = (uint16_t)((b[2] << 8) | b[3]);
        switch (b[0]) {
        case 0x00:
            v->env_rate = 0; v->vib_reset = 0; v->porta_rate = 0; v->ab_time = 0;
            if (b[1]) v->vol = clamp_vol(b[3]);
            v->dma = 0; break;
        case 0x13: v->dma = 0; break;
        case 0x01: paula_on(v); break;
        case 0x02: v->ab_time = 0; v->reg_start = ((uint32_t)b[1] << 16) | w1; break;
        case 0x03: v->reg_len = w1; break;
        case 0x04: v->mwait = w1; return;
        case 0x10:
            if (v->keyup) break;           /* released: fall through to next cmd */
            /* else: behave like Loop */
        case 0x05:
            if (v->mloop == 0) { v->mloop = -1; break; }
            else if (v->mloop < 0) v->mloop = (int16_t)(b[1] - 1);
            else v->mloop--;
            v->mstep = w1;
            break;
        case 0x06: {
            int32_t a = macro_addr(p, b[1]);
            if (a < 0) { v->mrun = 0; return; }
            v->mptr = (uint32_t)a; v->mstep = w1; v->mloop = -1;
            break; }
        case 0x07: v->mrun = 0; return;
        case 0x08: set_period_from(v, v->note + (int8_t)b[1], (int16_t)w1); break;
        case 0x09: set_period_from(v, b[1], (int16_t)w1); break;
        case 0x1F: set_period_from(v, v->prevnote + (int8_t)b[1], (int16_t)w1); break;
        case 0x0A: v->env_rate = 0; v->vib_reset = 0; v->porta_rate = 0; v->ab_time = 0; break;
        case 0x0B:
            v->porta_reset = b[1]; v->porta_time = 1;
            if (!v->porta_rate) v->porta_per = v->dest;
            v->porta_rate = w1;
            break;
        case 0x0C:
            v->vib_reset = b[1]; v->vib_time = (uint8_t)(b[1] >> 1); v->vib_width = (int8_t)b[3];
            if (!v->porta_rate) { v->cur = v->dest; v->vib_off = 0; }
            break;
        case 0x0D: v->vol = clamp_vol(v->velo * 3 + (int8_t)b[3]); break;
        case 0x0E: v->vol = clamp_vol(b[3]); break;
        case 0x0F:
            v->env_rate = b[1]; v->env_reset = v->env_time = b[2];
            v->env_end = (uint8_t)(b[3] > 64 ? 64 : b[3]);
            break;
        case 0x11:
            v->ab_time = v->ab_reset = b[1]; v->ab_delta = (int16_t)w1;
            v->reg_start = (uint32_t)((int32_t)v->reg_start + v->ab_delta) & 0xFFFFFFUL;
            break;
        case 0x12: v->reg_len = (uint16_t)(v->reg_len + (int16_t)w1); break;
        case 0x14:
            if (v->keyup) { v->mloop = -1; break; }
            if (v->mloop == 0) { v->mloop = -1; break; }
            else if (v->mloop < 0) v->mloop = (int16_t)(b[3] - 1);
            else v->mloop--;
            v->mstep--; return;
        case 0x15: {
            int32_t a = macro_addr(p, b[1]);
            if (a < 0) break;
            v->rptr = v->mptr; v->rstep = v->mstep;
            v->mptr = (uint32_t)a; v->mstep = w1; v->mloop = -1;
            break; }
        case 0x16: v->mptr = v->rptr; v->mstep = v->rstep; break;
        case 0x17: v->dest = w1; if (!v->porta_rate) v->cur = w1; break;
        case 0x18: {
            uint32_t off = (((uint32_t)b[1] << 16) | w1) & 0xFFFFFEUL;
            int32_t nl = (int32_t)v->reg_len - (int32_t)(off >> 1);
            v->reg_start = (v->reg_start + off) & 0xFFFFFFUL;
            v->reg_len = (uint16_t)(nl < 1 ? 1 : nl);
            break; }
        case 0x19: v->ab_time = 0; v->reg_start = 0; v->reg_len = 1; break;
        case 0x1A: v->dma_wait = w1; return;
        case 0x1C: if (v->note > b[1]) v->mstep = w1; break;
        case 0x1D: if (v->vol > b[1]) v->mstep = w1; break;
        case 0x21: note_port(p, v->note, b[1], b[2], (uint8_t)v->finetune); break;
        default: break;   /* 1B random, 1E, 20 signal, 22..29 SID: not implemented */
        }
    }
    v->mrun = 0;
}

static void do_effects(TfmxVoice *v) {
    if (v->ab_time) {
        v->reg_start = (uint32_t)((int32_t)v->reg_start + v->ab_delta) & 0xFFFFFFUL;
        if (--v->ab_time == 0) { v->ab_time = v->ab_reset; v->ab_delta = (int16_t)-v->ab_delta; }
    }
    if (v->vib_reset) {
        int32_t a;
        v->vib_off = (int16_t)(v->vib_off + v->vib_width);
        a = ((int32_t)v->dest * (0x800 + v->vib_off)) >> 11;
        if (!v->porta_rate) v->cur = (uint16_t)a;
        if (v->vib_time) v->vib_time--;
        if (v->vib_time == 0) { v->vib_time = v->vib_reset; v->vib_width = (int8_t)-v->vib_width; }
    }
    if (v->porta_rate) {
        if (v->porta_time) v->porta_time--;
        if (v->porta_time == 0) {
            int32_t a = v->porta_per;
            v->porta_time = v->porta_reset;
            if (v->porta_per > v->dest) {
                a = ((int32_t)v->porta_per * (256 - (int32_t)v->porta_rate) - 128) >> 8;
                if (a <= v->dest) v->porta_rate = 0;
            } else if (v->porta_per < v->dest) {
                a = ((int32_t)v->porta_per * (256 + (int32_t)v->porta_rate)) >> 8;
                if (a >= v->dest) v->porta_rate = 0;
            } else v->porta_rate = 0;
            if (!v->porta_rate) a = v->dest;
            v->porta_per = v->cur = (uint16_t)a;
        }
    }
    if (v->env_rate) {
        if (v->env_time) v->env_time--;
        if (v->env_time == 0) {
            v->env_time = v->env_reset;
            if (v->vol > v->env_end) {
                v->vol = (int16_t)(v->vol - v->env_rate);
                if (v->vol <= v->env_end) { v->vol = v->env_end; v->env_rate = 0; }
            } else if (v->vol < v->env_end) {
                v->vol = (int16_t)(v->vol + v->env_rate);
                if (v->vol >= v->env_end) { v->vol = v->env_end; v->env_rate = 0; }
            } else v->env_rate = 0;
        }
    }
}

void tfmx_player_jiffy(TfmxPlayer *p) {
    int i;
    for (i = 0; i < TFMX_VOICES; i++) { do_macro(p, &p->v[i]); do_effects(&p->v[i]); }
    if (p->fade_time) {
        if (--p->fade_time == 0) {
            p->fade_time = p->fade_reset;
            p->master = (int16_t)(p->master + p->fade_step);
            if (p->master == p->fade_dest) p->fade_time = 0;
        }
    }
    if (p->playing) {
        if (p->speed_cnt == 0) { p->speed_cnt = p->speed; do_tracks(p); }
        else p->speed_cnt--;
    }
    p->jiffy++;
}

void tfmx_player_render(TfmxPlayer *p, int16_t *out, int n) {
    int i, k;
    if (!p->loaded) { memset(out, 0, (size_t)n * sizeof(int16_t)); return; }
    for (i = 0; i < n; i++) {
        int32_t acc = 0;
        if (p->acc_fp <= 0) {
            tfmx_player_jiffy(p);
            p->acc_fp += (int32_t)p->spj_fp;
            for (k = 0; k < TFMX_VOICES; k++) {
                TfmxVoice *v = &p->v[k];
                uint32_t per = v->cur >= 113 ? v->cur : 113;
                v->inc_fp = v->cur ? ((((PAULA_CLOCK << 4) / per) << 12) / p->rate) : 0;
            }
        }
        p->acc_fp -= CIA_STEP;
        for (k = 0; k < TFMX_VOICES; k++) {
            TfmxVoice *v = &p->v[k];
            uint32_t idx;
            if (!v->dma || !v->inc_fp) continue;
            idx = v->lat_start + (v->pos_fp >> 16);
            if (idx < p->smpl_size) acc += (int32_t)p->smpl[idx] * v->vol;
            else p->oob_reads++;
            v->pos_fp += v->inc_fp;
            if ((v->pos_fp >> 16) >= v->lat_bytes) {         /* Paula reload */
                v->pos_fp -= v->lat_bytes << 16;
                v->lat_start = v->reg_start & 0xFFFFFEUL;
                v->lat_bytes = (uint32_t)(v->reg_len ? v->reg_len : 0x10000UL) * 2u;
                if ((v->pos_fp >> 16) >= v->lat_bytes) v->pos_fp = 0;
                if (v->dma_wait >= 0) {
                    if (v->dma_wait == 0) v->dma_wait = -1; else v->dma_wait--;
                }
            }
        }
        acc = (acc * p->master) >> 6;
        acc = (acc * 3) >> 2;
        out[i] = (int16_t)(acc > 32767 ? 32767 : (acc < -32768 ? -32768 : acc));
    }
}

static void tfmx_ext_render(void *ctx, int16_t *out, int n) {
    tfmx_player_render((TfmxPlayer *)ctx, out, n);
}

void tfmx_player_tick(TfmxPlayer *p, GameAudio *audio) {
    if (!audio) return;
    if (p->rate != GAME_AUDIO_RATE) { p->rate = GAME_AUDIO_RATE; set_cia(p, p->bpm); }
    audio->ext_render = tfmx_ext_render;
    audio->ext_ctx = p;
}
