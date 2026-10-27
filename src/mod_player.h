/* mod_player.h - ProTracker MOD player (C89, integer). Mirror: tools/mod_oracle.py */
#ifndef MOD_PLAYER_H
#define MOD_PLAYER_H
#include <stdint.h>
#define MOD_MAX_CH 8
typedef struct { const int8_t *data; uint32_t len, rep, replen; int8_t fine; uint8_t vol; } ModSample;
typedef struct {
    uint8_t  sidx, active, cmd, prm, porta_spd, vib_pos, vib_prm, loop_row, loop_cnt, offset_mem;
    int16_t  period, dest, vol, note_per, per_out;
    uint32_t pos_fp, inc_fp;
} ModChan;
typedef struct {
    uint8_t  loaded, playing;
    char     title[21];
    uint8_t  nch, songlen, restart, npat;
    const uint8_t *orders, *patdata, *buf;
    uint32_t size;
    ModSample smp[32];
    uint8_t  pos, row, speed, bpm, pdelay, jump;
    uint16_t tick;
    int16_t  next_pos, next_row;
    uint32_t rate, ticks, loops;
    int32_t  acc;
    ModChan  ch[MOD_MAX_CH];
} ModPlayer;
int  mod_player_load_mem(ModPlayer *m, const uint8_t *buf, uint32_t size);  /* buf must outlive player */
void mod_player_start(ModPlayer *m, uint32_t rate);
void mod_player_tick(ModPlayer *m);                    /* one 50Hz-ish tick (CIA bpm) */
void mod_player_render(ModPlayer *m, int16_t *out, int n);
#endif
