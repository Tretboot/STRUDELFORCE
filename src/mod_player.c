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
#include "mod_player.h"
#include <string.h>

#define PAULA_CLOCK 3546895UL

static const uint8_t vib_sine[32] = {
    0, 24, 49, 74, 97,120,141,161,180,197,212,224,235,244,250,253,
  255,253,250,244,235,224,212,197,180,161,141,120, 97, 74, 49, 24 };

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

int mod_player_load_mem(ModPlayer *m, const uint8_t *buf, uint32_t size) {
    uint32_t off;
    int i, maxpat = 0;
    char sig[5];
    memset(m, 0, sizeof(*m));
    if (!buf || size < 1084) return 0;
    memcpy(sig, buf + 1080, 4); sig[4] = '\0';
    if (!memcmp(sig, "M.K.", 4) || !memcmp(sig, "M!K!", 4) || !memcmp(sig, "FLT4", 4) || !memcmp(sig, "4CHN", 4)) m->nch = 4;
    else if (!memcmp(sig, "6CHN", 4)) m->nch = 6;
    else if (!memcmp(sig, "8CHN", 4) || !memcmp(sig, "FLT8", 4) || !memcmp(sig, "OCTA", 4)) m->nch = 8;
    else if (sig[2] == 'C' && sig[3] == 'H' && sig[0] >= '0' && sig[0] <= '9' && sig[1] >= '0' && sig[1] <= '9') {
        int n = (sig[0] - '0') * 10 + (sig[1] - '0');
        if (n < 1 || n > MOD_MAX_CH) return 0;
        m->nch = (uint8_t)n;
    } else return 0;                       /* 15-sample / unknown */
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
