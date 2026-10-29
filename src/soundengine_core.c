/*
 * soundengine_core.c - STRUDELFORCE Universal Sound Engine Hub
 *
 * Vereint alle Audio-Subsysteme in einem monolithischen Modul:
 *   1. Clean-Room Strudel Mini-Notation Compiler & Query AST
 *   2. Chris Huelsbeck TFMX 1.5 / Pro / 7V Player (Bit-identisch zum TFMX-Oracle)
 *   3. ProTracker MOD Replay Engine (4, 6, 8 Kanaele, 31 Samples, Bit-identisch zum MOD-Oracle)
 *   4. Multi-Source Audio Mixer (SfMixer mit Gain-Rampen & Ducking)
 *   5. Cyber-Paula 8-Stimmen Synthesizer & Reaktive Gameplay-Matrix
 *   6. Game-SFX System (Feuer, Sprung, Treffer, Explosion, Pickup, Truhe)
 *   7. Soundgodizer PC HQ-Overdrive & Haas Stereo Widener (optional)
 *   8. Win32 Low-Latency Ring-Transport (Audio-Thread & WaveOut)
 *
 * Lizenz: MIT (Clean Room)
 */

#define _CRT_SECURE_NO_WARNINGS
#include "strudelforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

/* ============================================================================
 * SECTION 1: STRUDEL CLEANROOM MINI-NOTATION
 * ============================================================================ */

int16_t strudel_parse_note_token(const char *tok) {
    int base = -1;
    int idx = 1;
    int octave = 3;
    if (!tok || tok[0] == '~' || tok[0] == '-') return -1;
    switch (tolower((unsigned char)tok[0])) {
        case 'c': base = 0; break;
        case 'd': base = 2; break;
        case 'e': base = 4; break;
        case 'f': base = 5; break;
        case 'g': base = 7; break;
        case 'a': base = 9; break;
        case 'b': base = 11; break;
        default: return -1;
    }
    if (tok[idx] == '#' || tok[idx] == 's') { base += 1; idx++; }
    else if (tok[idx] == 'b') { base -= 1; idx++; }
    if (isdigit((unsigned char)tok[idx])) {
        octave = tok[idx] - '0';
    }
    return (int16_t)((octave + 1) * 12 + base);
}

static uint8_t strudel_alloc_node(StrudelPattern *pat, StrudelNodeType type) {
    uint8_t idx;
    if (pat->node_count >= STRUDEL_MAX_NODES) return 0;
    idx = (uint8_t)pat->node_count++;
    memset(&pat->nodes[idx], 0, sizeof(StrudelNode));
    pat->nodes[idx].type = type;
    pat->nodes[idx].midi_note = -1;
    pat->nodes[idx].repeat_mult = 1;
    return idx;
}

static const char* strudel_parse_expr(StrudelPattern *pat, const char *p, uint8_t parent_idx, char close_char) {
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == close_char) {
            if (*p == close_char) p++;
            break;
        }
        if (*p == '[' || *p == '<') {
            char expected_close = (*p == '[') ? ']' : '>';
            StrudelNodeType ntype = (*p == '[') ? S_NODE_SEQ : S_NODE_ALT;
            uint8_t child_idx = strudel_alloc_node(pat, ntype);
            if (pat->nodes[parent_idx].num_children < STRUDEL_MAX_CHILDREN) {
                pat->nodes[parent_idx].children[pat->nodes[parent_idx].num_children++] = child_idx;
            }
            p = strudel_parse_expr(pat, p + 1, child_idx, expected_close);
            if (*p == '*') {
                p++;
                pat->nodes[child_idx].repeat_mult = (uint8_t)atoi(p);
                while (isdigit((unsigned char)*p)) p++;
            }
        } else {
            char tok[32];
            int ti = 0;
            uint8_t atom_idx;
            char *star;
            char *paren;
            while (*p && !isspace((unsigned char)*p) && *p != ']' && *p != '>' && *p != '[' && *p != '<') {
                if (ti < 31) tok[ti++] = *p;
                p++;
            }
            tok[ti] = '\0';

            atom_idx = strudel_alloc_node(pat, S_NODE_ATOM);
            star = strchr(tok, '*');
            paren = strchr(tok, '(');
            if (star) {
                *star = '\0';
                pat->nodes[atom_idx].repeat_mult = (uint8_t)atoi(star + 1);
            }
            if (paren) {
                int k = 1, n = 1;
                *paren = '\0';
                if (sscanf(paren + 1, "%d,%d", &k, &n) == 2 && n > 0) {
                    pat->nodes[atom_idx].type = S_NODE_EUCLID;
                    pat->nodes[atom_idx].euclid_k = (uint8_t)k;
                    pat->nodes[atom_idx].euclid_n = (uint8_t)n;
                }
            }
            pat->nodes[atom_idx].midi_note = strudel_parse_note_token(tok);
            if (pat->nodes[parent_idx].num_children < STRUDEL_MAX_CHILDREN) {
                pat->nodes[parent_idx].children[pat->nodes[parent_idx].num_children++] = atom_idx;
            }
        }
    }
    return p;
}

void strudel_compile(StrudelPattern *pat, const char *code, uint8_t default_inst) {
    uint16_t i;
    memset(pat, 0, sizeof(StrudelPattern));
    if (!code) return;
    strncpy(pat->raw_source, code, sizeof(pat->raw_source) - 1);
    pat->root_idx = strudel_alloc_node(pat, S_NODE_SEQ);
    strudel_parse_expr(pat, code, pat->root_idx, '\0');
    for (i = 0; i < pat->node_count; i++) {
        pat->nodes[i].instrument_id = default_inst;
    }
}

int16_t strudel_query_step(const StrudelPattern *pat, uint32_t cycle_num, uint32_t step_in_cycle, uint32_t total_steps) {
    uint8_t curr = pat->root_idx;
    uint32_t local_step = step_in_cycle;
    uint32_t span = total_steps;
    int depth;

    for (depth = 0; depth < 8; depth++) {
        const StrudelNode *n = &pat->nodes[curr];
        if (n->repeat_mult > 1 && span >= n->repeat_mult) {
            span /= n->repeat_mult;
            local_step %= span;
        }
        if (n->type == S_NODE_ATOM) {
            return (local_step == 0) ? n->midi_note : -2;
        } else if (n->type == S_NODE_EUCLID) {
            uint32_t sub_span;
            uint32_t e_idx;
            int hit;
            if (n->euclid_n == 0) return -1;
            sub_span = span / n->euclid_n;
            if (sub_span == 0) sub_span = 1;
            if (local_step % sub_span != 0) return -2;
            e_idx = (local_step / sub_span) % n->euclid_n;
            hit = ((e_idx * n->euclid_k) % n->euclid_n) < n->euclid_k;
            return hit ? n->midi_note : -1;
        } else if (n->type == S_NODE_ALT) {
            if (n->num_children == 0) return -1;
            curr = n->children[cycle_num % n->num_children];
        } else if (n->type == S_NODE_SEQ) {
            uint32_t child_span;
            uint8_t c_idx;
            if (n->num_children == 0) return -1;
            child_span = span / n->num_children;
            if (child_span == 0) child_span = 1;
            c_idx = (uint8_t)(local_step / child_span);
            if (c_idx >= n->num_children) c_idx = n->num_children - 1;
            local_step %= child_span;
            span = child_span;
            curr = n->children[c_idx];
        }
    }
    return -1;
}


/* ============================================================================
 * SECTION 2: CHRIS HUELSBECK TFMX REPLAY ENGINE (v2)
 * ============================================================================ */

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
#ifndef M68K
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
    p->speed = 1; set_cia(p, 0); /* Default-Tempo 1 (volles doppeltes Arcade-Tempo) */
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
        acc = (acc * 5) >> 2;
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


/* ============================================================================
 * SECTION 3: PROTRACKER MOD REPLAY ENGINE
 * ============================================================================ */


void mod_player_init(ModPlayer *m) {
    if (m) memset(m, 0, sizeof(*m));
}

void mod_player_stop(ModPlayer *m) {
    if (m) m->playing = 0;
}

/*
 * mod_player.c - STRUDELFORCE ProTracker/NoiseTracker MOD player (C89, integer only)
 * Mirror oracle: tools/mod_oracle.py  (same logic, same integer math)
 *
 * Supported: 31-sample MODs, M.K./M!K!/FLT4/4CHN, 6CHN, 8CHN, FLT8(as 8ch), xxCH (<=8)
 * Effects:   0 arp, 1/2 porta, 3 tone porta, 4 vibrato, 5, 6, 9 offset, A volslide,
 *            B jump, C volume, D break, F speed/tempo,
 *            E1/E2 fine porta, E6 loop, E9 retrig, EA/EB fine vol, EC cut, ED delay, EE pattern delay
 * Not yet:   7 tremolo, 8/E8 panning (mono mixer), finetune, 15-sample Soundtracker MODs, E3/E4/E5/EF
 * Lizenz: MIT (Clean Room)
 */

#ifndef PAULA_CLOCK
#define PAULA_CLOCK 3546895UL
#endif

static const uint8_t vib_sine[32] = {
    0, 24, 49, 74, 97,120,141,161,180,197,212,224,235,244,250,253,
  255,253,250,244,235,224,212,197,180,161,141,120, 97, 74, 49, 24 };

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

int mod_player_load_mem(ModPlayer *m, const uint8_t *buf, uint32_t size) {
    uint32_t off;
    int i, maxpat = 0;
    char sig[5];
    int is_15 = 0;
    memset(m, 0, sizeof(*m));
    if (!buf || size < 600) return 0;
    if (size >= 1084) {
        memcpy(sig, buf + 1080, 4); sig[4] = '\0';
        if (!memcmp(sig, "M.K.", 4) || !memcmp(sig, "M!K!", 4) || !memcmp(sig, "FLT4", 4) || !memcmp(sig, "4CHN", 4)) m->nch = 4;
        else if (!memcmp(sig, "6CHN", 4)) m->nch = 6;
        else if (!memcmp(sig, "8CHN", 4) || !memcmp(sig, "FLT8", 4) || !memcmp(sig, "OCTA", 4)) m->nch = 8;
        else if (sig[2] == 'C' && sig[3] == 'H' && sig[0] >= '0' && sig[0] <= '9' && sig[1] >= '0' && sig[1] <= '9') {
            int n = (sig[0] - '0') * 10 + (sig[1] - '0');
            if (n < 1 || n > MOD_MAX_CH) return 0;
            m->nch = (uint8_t)n;
        } else {
            is_15 = 1;
        }
    } else {
        is_15 = 1;
    }
    if (is_15) {
        /* Soundtracker 15-sample format (1987-1989): 600-byte header */
        m->songlen = buf[470];
        if (m->songlen == 0 || m->songlen > 128) return 0;
        m->nch = 4;
        memcpy(m->title, buf, 20); m->title[20] = '\0';
        m->restart = buf[471];
        m->orders = buf + 472;
        for (i = 0; i < 128; i++) if (buf[472 + i] > maxpat) maxpat = buf[472 + i];
        m->npat = (uint8_t)(maxpat + 1);
        m->patdata = buf + 600;
        off = 600u + (uint32_t)m->npat * 1024u;
        if (off > size) return 0;
        for (i = 1; i <= 15; i++) {
            const uint8_t *h = buf + 20 + (i - 1) * 30;
            ModSample *s = &m->smp[i];
            s->len = (uint32_t)be16(h + 22) * 2u;
            s->fine = 0;
            s->vol = (uint8_t)(h[25] > 64 ? 64 : h[25]);
            s->rep = (uint32_t)be16(h + 26) * 2u;
            s->replen = (uint32_t)be16(h + 28) * 2u;
            s->data = (const int8_t *)(buf + off);
            if (off + s->len > size) s->len = off < size ? size - off : 0;
            if (s->rep > s->len) { s->rep = 0; s->replen = 0; }
            if (s->rep + s->replen > s->len) s->replen = s->len - s->rep;
            off += (uint32_t)be16(h + 22) * 2u;
        }
        m->buf = buf; m->size = size; m->loaded = 1;
        return 1;
    }
    memcpy(m->title, buf, 20); m->title[20] = '\0';
    m->songlen = buf[950];
    if (m->songlen == 0 || m->songlen > 128) return 0;
    m->restart = buf[951];
    m->orders = buf + 952;
    for (i = 0; i < 128; i++) if (buf[952 + i] > maxpat) maxpat = buf[952 + i];
    m->npat = (uint8_t)(maxpat + 1);
    m->patdata = buf + 1084;
    off = 1084u + (uint32_t)m->npat * 64u * 4u * m->nch;
    if (off > size) return 0;
    for (i = 1; i <= 31; i++) {
        const uint8_t *h = buf + 20 + (i - 1) * 30;
        ModSample *s = &m->smp[i];
        s->len = (uint32_t)be16(h + 22) * 2u;
        s->fine = (int8_t)((h[24] & 0x0F) << 4) >> 4;
        s->vol = (uint8_t)(h[25] > 64 ? 64 : h[25]);
        s->rep = (uint32_t)be16(h + 26) * 2u;
        s->replen = (uint32_t)be16(h + 28) * 2u;
        s->data = (const int8_t *)(buf + off);
        if (off + s->len > size) s->len = off < size ? size - off : 0;   /* truncated file */
        if (s->rep > s->len) { s->rep = 0; s->replen = 0; }
        if (s->rep + s->replen > s->len) s->replen = s->len - s->rep;
        off += (uint32_t)be16(h + 22) * 2u;
    }
    m->buf = buf; m->size = size; m->loaded = 1;
    return 1;
}

void mod_player_start(ModPlayer *m, uint32_t rate) {
    if (!m->loaded) return;
    memset(m->ch, 0, sizeof(m->ch));
    m->rate = rate ? rate : 16574;
    m->pos = 0; m->row = 0; m->tick = 0; m->speed = 6; m->bpm = 125;
    m->pdelay = 0; m->jump = 0; m->next_pos = 0; m->next_row = 0;
    m->acc = 0; m->ticks = 0; m->loops = 0; m->playing = 1;
}

static void set_inc(ModPlayer *m, ModChan *c, int per) {
    if (per < 113) per = 113;
    if (per > 6848) per = 6848;
    c->inc_fp = (((PAULA_CLOCK << 4) / (uint32_t)per) << 12) / m->rate;
}

static void trigger(ModPlayer *m, ModChan *c) {
    const ModSample *s = &m->smp[c->sidx];
    c->period = c->note_per;
    c->pos_fp = (c->cmd == 9) ? ((uint32_t)c->offset_mem << 24) : 0;
    c->active = (c->sidx && s->len > 2) ? 1 : 0;
    if (c->vib_prm == 0 || c->cmd != 4) c->vib_pos = 0;
    if (c->cmd == 9 && (c->pos_fp >> 16) >= s->len) c->active = 0;
}

static void vol_slide(ModChan *c, uint8_t prm) {
    int v = c->vol;
    if (prm & 0xF0) v += prm >> 4; else v -= prm & 0x0F;
    c->vol = (int16_t)(v < 0 ? 0 : (v > 64 ? 64 : v));
}

static void tone_porta(ModChan *c) {
    if (!c->dest || !c->porta_spd) return;
    if (c->period < c->dest) { c->period = (int16_t)(c->period + c->porta_spd); if (c->period > c->dest) c->period = c->dest; }
    else if (c->period > c->dest) { c->period = (int16_t)(c->period - c->porta_spd); if (c->period < c->dest) c->period = c->dest; }
}

static int vibrato(ModChan *c) {
    int d = (vib_sine[c->vib_pos & 31] * (c->vib_prm & 0x0F)) >> 7;
    if (c->vib_pos & 32) d = -d;
    c->vib_pos = (uint8_t)((c->vib_pos + (c->vib_prm >> 4)) & 63);
    return d;
}

static void row_start(ModPlayer *m) {
    int k;
    const uint8_t *row = m->patdata + (((uint32_t)m->orders[m->pos] * 64u + m->row) * m->nch * 4u);
    for (k = 0; k < m->nch; k++) {
        ModChan *c = &m->ch[k];
        const uint8_t *e = row + k * 4;
        uint8_t s = (uint8_t)((e[0] & 0xF0) | (e[2] >> 4));
        uint16_t per = (uint16_t)(((e[0] & 0x0F) << 8) | e[1]);
        c->cmd = (uint8_t)(e[2] & 0x0F); c->prm = e[3];
        c->per_out = 0;
        if (s && s <= 31) { c->sidx = s; c->vol = m->smp[s].vol; }
        if (c->cmd == 9 && c->prm) c->offset_mem = c->prm;
        if (per) {
            if (c->cmd == 3 || c->cmd == 5) c->dest = (int16_t)per;
            else {
                c->note_per = (int16_t)per;
                if (!(c->cmd == 0xE && (c->prm >> 4) == 0xD)) trigger(m, c);
            }
        }
        switch (c->cmd) {
        case 0x3: if (c->prm) c->porta_spd = c->prm; break;
        case 0x4: if (c->prm & 0x0F) c->vib_prm = (uint8_t)((c->vib_prm & 0xF0) | (c->prm & 0x0F));
                  if (c->prm & 0xF0) c->vib_prm = (uint8_t)((c->vib_prm & 0x0F) | (c->prm & 0xF0)); break;
        case 0xB: m->jump = 1; m->next_pos = c->prm; m->next_row = 0; break;
        case 0xC: c->vol = (int16_t)(c->prm > 64 ? 64 : c->prm); break;
        case 0xD: if (!m->jump) m->next_pos = (int16_t)(m->pos + 1);
                  m->jump = 1; m->next_row = (int16_t)((c->prm >> 4) * 10 + (c->prm & 0x0F));
                  if (m->next_row > 63) m->next_row = 0; break;
        case 0xF: if (c->prm == 0) break;
                  if (c->prm < 32) m->speed = c->prm; else m->bpm = c->prm; break;
        case 0xE: {
            uint8_t x = (uint8_t)(c->prm >> 4), y = (uint8_t)(c->prm & 0x0F);
            if (x == 0x1) { c->period = (int16_t)(c->period - y); if (c->period < 113) c->period = 113; }
            else if (x == 0x2) { c->period = (int16_t)(c->period + y); if (c->period > 856 * 2) c->period = 856 * 2; }
            else if (x == 0x6) {
                if (y == 0) c->loop_row = m->row;
                else {
                    if (c->loop_cnt == 0) c->loop_cnt = y; else c->loop_cnt--;
                    if (c->loop_cnt) { m->jump = 1; m->next_pos = m->pos; m->next_row = c->loop_row; }
                }
            }
            else if (x == 0xA) { int v = c->vol + y; c->vol = (int16_t)(v > 64 ? 64 : v); }
            else if (x == 0xB) { int v = c->vol - y; c->vol = (int16_t)(v < 0 ? 0 : v); }
            else if (x == 0xC && y == 0) c->vol = 0;
            else if (x == 0xE) { if (!m->pdelay) m->pdelay = y; }
            break; }
        default: break;
        }
        set_inc(m, c, c->period);
    }
}

static void row_tick(ModPlayer *m) {
    int k;
    for (k = 0; k < m->nch; k++) {
        ModChan *c = &m->ch[k];
        int per = c->period;
        uint8_t x = (uint8_t)(c->prm >> 4), y = (uint8_t)(c->prm & 0x0F);
        switch (c->cmd) {
        case 0x0:
            if (c->prm) {
                int step = m->tick % 3, semis = step == 1 ? x : (step == 2 ? y : 0);
                /* 2^(-n/12) in 16.16 for n=0..15 */
                static const uint16_t arp_mul[16] = {65535,61858,58386,55109,52016,49096,46341,43740,
                                                     41285,38968,36781,34716,32768,30929,29193,27554};
                per = (int)(((uint32_t)c->period * arp_mul[semis]) >> 16);
            }
            break;
        case 0x1: c->period = (int16_t)(c->period - c->prm); if (c->period < 113) c->period = 113; per = c->period; break;
        case 0x2: c->period = (int16_t)(c->period + c->prm); if (c->period > 6848) c->period = 6848; per = c->period; break;
        case 0x3: tone_porta(c); per = c->period; break;
        case 0x4: per = c->period + vibrato(c); break;
        case 0x5: tone_porta(c); vol_slide(c, c->prm); per = c->period; break;
        case 0x6: vol_slide(c, c->prm); per = c->period + vibrato(c); break;
        case 0xA: vol_slide(c, c->prm); break;
        case 0xE:
            if (x == 0x9 && y && (m->tick % y) == 0) { c->pos_fp = 0; c->active = c->sidx ? 1 : 0; }
            else if (x == 0xC && m->tick == y) c->vol = 0;
            else if (x == 0xD && m->tick == y && c->note_per) { c->cmd = 0; trigger(m, c); c->cmd = 0xE; per = c->period; }
            break;
        default: break;
        }
        set_inc(m, c, per);
    }
}

void mod_player_tick(ModPlayer *m) {
    if (!m->playing) return;
    if (m->tick == 0) row_start(m); else row_tick(m);
    m->ticks++;
    m->tick++;
    if (m->tick >= (uint16_t)m->speed * (uint16_t)(m->pdelay + 1)) {
        m->tick = 0; m->pdelay = 0;
        if (m->jump) {
            m->jump = 0;
            if (m->next_pos <= m->pos && !(m->next_pos == m->pos && m->next_row > m->row)) m->loops++;
            m->pos = (uint8_t)m->next_pos; m->row = (uint8_t)m->next_row;
        } else if (++m->row >= 64) { m->row = 0; m->pos++; }
        if (m->pos >= m->songlen) { m->pos = (uint8_t)(m->restart < m->songlen ? m->restart : 0); m->loops++; }
    }
}

void mod_player_render(ModPlayer *m, int16_t *out, int n) {
    int i, k;
    for (i = 0; i < n; i++) {
        int32_t acc = 0;
        if (!m->playing) { out[i] = 0; continue; }
        if (m->acc <= 0) { mod_player_tick(m); m->acc += (int32_t)m->rate * 5; }
        m->acc -= 2 * (int32_t)m->bpm;
        for (k = 0; k < m->nch; k++) {
            ModChan *c = &m->ch[k];
            const ModSample *s = &m->smp[c->sidx];
            uint32_t idx;
            if (!c->active) continue;
            idx = c->pos_fp >> 16;
            if (idx >= s->len) { c->active = 0; continue; }
            acc += (int32_t)s->data[idx] * c->vol;
            c->pos_fp += c->inc_fp;
            idx = c->pos_fp >> 16;
            if (s->replen > 2) {
                while (idx >= s->rep + s->replen) { c->pos_fp -= s->replen << 16; idx = c->pos_fp >> 16; }
            } else if (idx >= s->len) c->active = 0;
        }
        acc = (acc * 3) >> 2;
        if (m->nch > 4) acc = (acc * 4) / m->nch;
        out[i] = (int16_t)(acc > 32767 ? 32767 : (acc < -32768 ? -32768 : acc));
    }
}


/* ============================================================================
 * SECTION 4: MULTI-SOURCE MIXER (SfMixer)
 * ============================================================================ */

/*
 * sf_mixer.c - STRUDELFORCE multi-source mixer (TFMX + MOD + Strudel synth [+ SID later])
 * C89, integer only, no malloc. Each source renders mono s16 into a scratch block,
 * mixer sums with per-source gain (0..256 = 0..1.0) and optional ducking.
 * Lizenz: MIT (Clean Room)
 */

void sf_mixer_init(SfMixer *mx) { memset(mx, 0, sizeof(*mx)); mx->master = 256; }

int sf_mixer_add(SfMixer *mx, SfRenderFn fn, void *ctx, int16_t gain, const char *name) {
    SfSource *s;
    if (mx->count >= SF_MIX_MAX_SOURCES || !fn) return -1;
    s = &mx->src[mx->count];
    s->fn = fn; s->ctx = ctx; s->gain = gain; s->target = gain; s->muted = 0; s->duck = 0;
    strncpy(s->name, name ? name : "src", sizeof(s->name) - 1); s->name[sizeof(s->name) - 1] = '\0';
    return mx->count++;
}

void sf_mixer_set_gain(SfMixer *mx, int idx, int16_t gain) {
    if (idx >= 0 && idx < mx->count) mx->src[idx].target = gain;   /* ramps, no clicks */
}

void sf_mixer_render(SfMixer *mx, int16_t *out, int n) {
    int done = 0, i, k;
    while (done < n) {
        int blk = n - done;
        int32_t sum[SF_MIX_BLOCK];
        if (blk > SF_MIX_BLOCK) blk = SF_MIX_BLOCK;
        memset(sum, 0, sizeof(sum));
        for (k = 0; k < mx->count; k++) {
            SfSource *s = &mx->src[k];
            int16_t g0, g1;
            s->fn(s->ctx, mx->scratch, blk);       /* always render: keeps sources in time */
            g0 = s->gain;
            if (s->gain < s->target) { s->gain = (int16_t)(s->gain + 8); if (s->gain > s->target) s->gain = s->target; }
            if (s->gain > s->target) { s->gain = (int16_t)(s->gain - 8); if (s->gain < s->target) s->gain = s->target; }
            g1 = s->gain;
            if (s->muted) continue;
            for (i = 0; i < blk; i++) {
                int32_t g = g0 + (((int32_t)(g1 - g0) * i) / blk);
                if (s->duck && mx->duck_level) g = (g * (256 - mx->duck_level)) >> 8;
                sum[i] += ((int32_t)mx->scratch[i] * g) >> 8;
            }
        }
        for (i = 0; i < blk; i++) {
            int32_t v = (sum[i] * mx->master) >> 8;
            if (v > 32767) { v = 32767; mx->clips++; } else if (v < -32768) { v = -32768; mx->clips++; }
            out[done + i] = (int16_t)v;
        }
        done += blk;
    }
}

/* Adapter for GameAudio.ext_render (game_audio.c v2 hook) */
void sf_mixer_ext_render(void *ctx, int16_t *out, int n) { sf_mixer_render((SfMixer *)ctx, out, n); }


/* ============================================================================
 * SECTION 5: CYBER-PAULA REPLAY & GAMEPLAY REACTIVE MATRIX
 * ============================================================================ */

/*
 * game_audio.c - STRUDELFORCE Cyber-Paula-Verbindung (Meilenstein 3)
 *
 * Reaktive Gameplay->Audio-Matrix (AGENT_2-Spec: cyber_paula_update_from_game)
 * + 8-Stimmen-Mixer mit 1-Pole-Low-Pass und optionalem 4-Bit-Quantisizer.
 *
 * PC-Target: float/double NUR fuer die Note->Frequenz-Berechnung (konsistent
 * mit tfmx_macro_synth.h). Der Gameplay-Core bleibt strikt 16.16 Fixed-Point.
 * Amiga-Backport (Agent 5) wandelt diese Engine auf Fixed-Point um.
 * Kein malloc im Hot-Path. Lizenz: MIT (Clean Room)
 */

/* 1-Pole-Low-Pass-Koeffizienten 16.16: 1 - exp(-2*pi*fc/fs), fs = 16574 */
#define CUTOFF_8000_FP 62377
#define CUTOFF_550_FP  12327

/* 50Hz Bassline: C-Moll (Biom 1 "Chrome Tempest") */
static const int16_t BASSLINE[8] = { 36, 36, 43, 36, 39, 36, 41, 39 };

typedef struct {
    uint16_t off;
    uint16_t len;
    int8_t   arp[3];
    uint8_t  decay;
} MacDef;

/* 0: PWM-Saw (Laser-Bass)  1: FM-Pluck (Bounce, Arp)  2: Zap  3: Boom  4: Tick */
static const MacDef MACROS[5] = {
    { 0,   128, { 0,  0,  0}, 1  },
    { 256, 256, { 0, 12,  7}, 2  },
    { 256, 256, { 0,  0,  0}, 8  },
    { 0,   128, { 0,  0,  0}, 12 },
    { 256, 256, { 0,  0,  0}, 16 }
};

static void voice_set(GameAudio *a, int v, uint8_t macro, int16_t note, int16_t vol) {
    GameVoice *vc = &a->voices[v];
    const MacDef *m = &MACROS[macro % 5];
    double freq;
    vc->active = 1;
    vc->macro_id = macro;
    vc->base_note = note;
    vc->tick_50hz = 0;
    vc->pos_fp = 0;
    vc->volume = vol;
    vc->loop_start = m->off;
    vc->loop_len = m->len ? m->len : 128;
    freq = 440.0 * pow(2.0, (double)(note - 69) / 12.0);
    vc->inc_fp = (uint32_t)((freq * (double)vc->loop_len) / (double)GAME_AUDIO_RATE * 65536.0);
}

void soundengine_init(GameAudio *a) {
    int i;
    memset(a, 0, sizeof(GameAudio));
    a->cutoff_fp = CUTOFF_8000_FP;
    a->pwm_speed = 2;
    a->bass_override_active = 0;
    a->bass_override_note = -1;
    a->ext_sample_ram = NULL;
    a->ext_sample_size = 0;
    a->ext_render = NULL;
    a->ext_ctx = NULL;
    /* Bank 0 (0..255): Saegezahn + Puls-Hybrid fuer TFMX PWM-Modulation */
    for (i = 0; i < 256; i++) {
        a->sample_ram[i] = (int8_t)((i < 128 ? i * 2 : (i - 256) * 2) - 64);
    }
    /* Bank 1 (256..511): FM-Slap/Pluck (DX21-Stil) */
    for (i = 0; i < 256; i++) {
        double p = (2.0 * 3.14159265358979 * i) / 256.0;
        a->sample_ram[256 + i] = (int8_t)(sin(p + 2.2 * sin(3.0 * p)) * 120.0);
    }
    /* Bank 2 (512..1023): D-50 AnalogString ("The Great Bath") */
    for (i = 0; i < 512; i++) {
        double p = (2.0 * 3.14159265358979 * i) / 512.0;
        a->sample_ram[512 + i] = (int8_t)((0.5 * sin(p) + 0.3 * sin(2.01 * p) + 0.2 * sin(4.0 * p)) * 115.0);
    }
}

void soundengine_update(GameAudio *a, const GameState *s) {
    int v;
    a->tick++;

    /* 1. Waffe -> Bass-Macro + Arpeggio (AGENT_2-Spec) */
    if (s->weapon == WEAPON_LASER) {
        a->bass_macro = 0;
        a->arp_active = 0;
    } else if (s->weapon == WEAPON_BOUNCE) {
        a->bass_macro = 1;
        a->arp_active = 1;
    } else {
        a->bass_macro = 0;
        a->arp_active = 0;
    }

    /* 2. Schild (0..100) -> PWM-Modulationsgeschwindigkeit (Spec: Q 0.7..8.5) */
    a->pwm_speed = (uint8_t)(2 + s->shield / 25);

    /* 3. HP < 25% -> Cutoff 550Hz + 4-Bit-Quantisierung */
    if (s->max_hp > 0 && (uint32_t)s->hp * 100u / (uint32_t)s->max_hp < 25u) {
        a->cutoff_fp = CUTOFF_550_FP;
        a->quantize_4bit = 1;
    } else {
        a->cutoff_fp = CUTOFF_8000_FP;
        a->quantize_4bit = 0;
    }

    /* 4. SFX-Transitions (read-only Diff der Core-Zaehler) */
    if (s->shots_fired > a->last_shots) {
        a->last_shots = s->shots_fired;
        voice_set(a, 2, 2,
                  (s->weapon == WEAPON_LASER) ? 60 : (s->weapon == WEAPON_BOUNCE ? 64 : 57), 48);
    }
    if (s->enemies_killed > a->last_kills) {
        a->last_kills = s->enemies_killed;
        voice_set(a, 3, 3, 24, 56);
    }
    if (s->bounces_total > a->last_bounces) {
        a->last_bounces = s->bounces_total;
        voice_set(a, 4, 4, 72, 40);
    }

    /* 5. Bass- + Arp-Sequencer (Schritt alle 5 Ticks = 10 Hz) */
    {
        int16_t bass_note;
        GameVoice *bass = &a->voices[0];
        GameVoice *arp = &a->voices[1];
        if (a->bass_override_active) {
            bass_note = a->bass_override_note;
        } else {
            bass_note = BASSLINE[(a->tick / 5) % 8];
        }
        if (bass_note < 0) {
            bass->active = 0;
            arp->active = 0;
        } else {
            if (!bass->active) {
                voice_set(a, 0, a->bass_macro, bass_note, 32);
            } else {
                bass->macro_id = a->bass_macro;
                bass->base_note = bass_note;
                if (bass->volume < 32) bass->volume = 32;
            }
            if (a->arp_active) {
                if (!arp->active) {
                    voice_set(a, 1, 1, bass_note + 12, 24);
                } else {
                    arp->base_note = bass_note + 12;
                    if (arp->volume < 24) arp->volume = 24;
                }
            } else {
                arp->active = 0;
            }
        }
    }

    /* 6. 50Hz-Macro-Tick (PWM, Arp, Decay, Frequenz) */
    for (v = 0; v < GAME_AUDIO_VOICES; v++) {
        GameVoice *vc = &a->voices[v];
        const MacDef *m;
        int16_t note;
        double freq;
        if (!vc->active) continue;
        m = &MACROS[vc->macro_id & 4];
        if (vc->tick_50hz > 0 && vc->volume > m->decay) {
            vc->volume -= m->decay;
        } else if (vc->volume <= m->decay) {
            vc->active = 0;
            continue;
        }
        vc->tick_50hz++;
        note = vc->base_note;
        if (vc->macro_id == 1) note += m->arp[vc->tick_50hz % 3];
        vc->loop_start = (uint16_t)((m->off + (vc->tick_50hz * a->pwm_speed) % 64) & 0xFFFF);
        vc->loop_len = m->len;
        freq = 440.0 * pow(2.0, (double)(note - 69) / 12.0);
        vc->inc_fp = (uint32_t)((freq * (double)m->len) / (double)GAME_AUDIO_RATE * 65536.0);
    }
}

void soundengine_render_synth(GameAudio *a, int16_t *out, int n) {
    int i, v;
    for (i = 0; i < n; i++) {
        int32_t sum = 0;
        for (v = 0; v < GAME_AUDIO_VOICES; v++) {
            GameVoice *vc = &a->voices[v];
            uint32_t idx;
            int8_t smp;
            if (!vc->active || vc->volume <= 0 || vc->loop_len == 0) continue;
            idx = (uint32_t)vc->loop_start + ((vc->pos_fp >> 16) % (uint32_t)vc->loop_len);
            if (a->ext_sample_ram && a->ext_sample_size > 0) {
                smp = a->ext_sample_ram[idx % a->ext_sample_size];
            } else {
                smp = a->sample_ram[idx & 0xFFFF];
            }
            sum += ((int32_t)smp * vc->volume) >> 6;
            vc->pos_fp += vc->inc_fp;
            if (vc->pos_fp >= (uint32_t)vc->loop_len << 16) {
                vc->pos_fp -= (uint32_t)vc->loop_len << 16;
            }
        }
        /* 1-Pole-Low-Pass (16.16) */
        a->lp_state += (int32_t)(((int64_t)a->cutoff_fp * (sum - a->lp_state)) >> 16);
        {
            int32_t y = a->lp_state;
            if (a->quantize_4bit) y = (y + 8) & ~15;
            if (y > 127) y = 127;
            else if (y < -128) y = -128;
            out[i] = (int16_t)(y << 8);
        }
    }
}

/* v2 wrapper: external source (TFMX player / SfMixer) replaces the synth when attached.
   To hear both, add game_audio_render_synth as an SfMixer source. */
void soundengine_render(GameAudio *a, int16_t *out, int n) {
    if (a->ext_render) { a->ext_render(a->ext_ctx, out, n); return; }
    soundengine_render_synth(a, out, n);
}


/* ============================================================================
 * SECTION 6: GAME-SFX SYSTEM (Feuer, Jump, Hit, Explosion, Crystal, Pickup, Chest)
 * ============================================================================ */


void sound_sfx_fire(GameAudio *a, uint8_t weapon) {
    if (weapon == WEAPON_LASER) {
        voice_set(a, 4, 2, 60, 50);
    } else if (weapon == WEAPON_BOUNCE) {
        voice_set(a, 4, 1, 64, 45);
    } else {
        voice_set(a, 4, 0, 57, 48);
    }
}

void sound_sfx_jump(GameAudio *a) {
    voice_set(a, 5, 1, 45, 52);
}

void sound_sfx_hit(GameAudio *a, uint8_t is_tank) {
    voice_set(a, 6, 2, is_tank ? 28 : 34, is_tank ? 58 : 46);
}

void sound_sfx_explosion(GameAudio *a) {
    voice_set(a, 6, 3, 24, 60);
}

void sound_sfx_crystal(GameAudio *a) {
    voice_set(a, 7, 1, 72, 48);
}

void sound_sfx_pickup(GameAudio *a) {
    voice_set(a, 7, 0, 67, 56);
}

void sound_sfx_chest(GameAudio *a) {
    voice_set(a, 7, 2, 63, 62);
}



/* ============================================================================
 * SECTION 7: SOUNDGODIZER POST-PROCESSING (PC HQ Overdrive & Haas Widener)
 * ============================================================================ */

/*
 * soundgodizer.c - STRUDELFORCE "Soundgodizer" Post-Processing-Kette
 *
 * Rechenlast: ~20 Ops/Sample @ 16574 Hz -> < 0.5 % einer Kern-CPU.
 * Alle Parameter 0..100, Presets werden in gui_main.c (Taste F5) gesetzt.
 *
 * Lizenz: MIT (Clean Room)
 */

void god_init(Soundgodizer *g) {
    god_set_params(g, 0, 0, 0, 0);
    god_reset_state(g);
}

void god_set_params(Soundgodizer *g, int bass, int warm, int drive, int width) {
    if (bass  < 0) bass = 0;  if (bass  > 100) bass  = 100;
    if (warm  < 0) warm = 0;  if (warm  > 100) warm  = 100;
    if (drive < 0) drive = 0; if (drive > 100) drive = 100;
    if (width < 0) width = 0; if (width > 100) width = 100;
    g->bass = bass;
    g->warm = warm;
    g->drive = drive;
    g->width = width;
}

void god_reset_state(Soundgodizer *g) {
    g->lp_bass = 0.0f;
    g->lp_warm = 0.0f;
    g->dly_i = 0;
    memset(g->dly, 0, sizeof(g->dly));
}

/* --- Kern-Kette: Bass-Boost -> Warm-LP -> Soft-Drive -> Haas-Wide -------- */
void god_process(Soundgodizer *g, const int16_t *in, int n, int16_t *out_lr) {
    int i;
    float f_bass, f_warm, f_drv, f_wid;
    float lp_bass, lp_warm;

    if (!g || !in || !out_lr || n <= 0) return;

    if (!g->enabled) {
        /* Passthrough: Mono duplizieren */
        for (i = 0; i < n; i++) {
            out_lr[i * 2]     = in[i];
            out_lr[i * 2 + 1] = in[i];
        }
        return;
    }

    /* Koeffizienten (16574 Hz Basisrate):
     * Bass-LP:  fc ~ 170 Hz  -> a = 1-exp(-2*pi*170/16574) ~= 0.064
     * Warm-LP:  fc von 12kHz (warm=0) bis 3kHz (warm=100)
     * Drive:    A = 9000 (Sweetspot), f = 1 + drive*0.03 (1.0..4.0)
     * Wide:     R = y + w*(delay - y), w max 0.7 */
    f_bass = (float)g->bass  * 0.012f;             /* 0 .. 1.2x LP-Anteil  */
    f_warm = 1.0f - (float)g->warm * 0.0065f;      /* 1.0 .. 0.35 (LP-Alpha) */
    f_drv  = 1.0f + (float)g->drive * 0.03f;       /* 1.0 .. 4.0 Drive    */
    f_wid  = (float)g->width * 0.007f;             /* 0 .. 0.7 Haas-Mix   */

    lp_bass = g->lp_bass;
    lp_warm = g->lp_warm;

    for (i = 0; i < n; i++) {
        float x = (float)in[i];
        float y;
        int16_t delayed;

        /* 1) Bass-Boost: Anteil des 170-Hz-Lowpasses dazu */
        lp_bass += 0.064f * (x - lp_bass);
        y = x + f_bass * lp_bass;

        /* 2) Warm-LP: zaehmt Quadrat-Obertoene (Analog-Paula-Gefuehl) */
        lp_warm += f_warm * (y - lp_warm);
        y = lp_warm;

        /* 3) Soft-Drive: tanh-Saturation, Dry/Wet-Blend (unity bei Low-Level) */
        {
            float sat = 9000.0f * tanhf(y * f_drv * (1.0f / 9000.0f));
            y = (y * (100.0f - (float)g->drive) + sat * (float)g->drive) * 0.01f;
        }

        /* 4) Clamp + Haas-Delay-Line */
        if (y > 32767.0f) y = 32767.0f;
        if (y < -32767.0f) y = -32767.0f;
        {
            int16_t yc = (int16_t)y;
            delayed = g->dly[g->dly_i];
            g->dly[g->dly_i] = yc;
            g->dly_i = (g->dly_i + 1) & (GOD_DELAY_SAMPLES - 1);
            {
                int16_t L = yc;
                int16_t R = (int16_t)(yc + (int)(f_wid * ((int)delayed - (int)yc)));
                out_lr[i * 2]     = L;
                out_lr[i * 2 + 1] = R;
            }
        }
    }

    g->lp_bass = lp_bass;
    g->lp_warm = lp_warm;
}


/* ============================================================================
 * SECTION 8: WIN32 AUDIO TRANSPORT (Low-Latency Ring & Dedicated Thread)
 * ============================================================================ */

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


#ifdef _WIN32
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