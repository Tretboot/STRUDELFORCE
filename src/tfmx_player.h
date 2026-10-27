/*
 * tfmx_player.h - STRUDELFORCE TFMX 1.5 / Pro / 7V player v2 (Chris Huelsbeck format)
 * Mirror oracle: tools/tfmx_oracle.py. Integer only, C89. NOT Jochen Hippel TFMX/COSO.
 * Containers: .tfx+.sam, mdat.X+smpl.X, TFHD, TFMXPAK.   Lizenz: MIT (Clean Room)
 */
#ifndef TFMX_PLAYER_H
#define TFMX_PLAYER_H
#include <stdint.h>
/* KEIN game_audio.h-Include hier! Der Player ist kern-neutral (Smith- und
 * sf-Targets linken ihn beide). Legacy tick bekommt GameAudio per
 * Forward-Declaration; tfmx_player.c includiert game_audio.h selbst. */
struct GameAudio;

#define TFMX_VOICES        8
#define TFMX_TRACKS        8
#define TFMX_MAX_SUBSONGS 32

typedef struct {
    uint8_t  mrun, keyup;
    uint32_t mptr, rptr;
    uint16_t mstep, rstep, mwait;
    int16_t  mloop;
    int32_t  dma_wait;
    uint8_t  note, prevnote, velo;
    int8_t   finetune;
    uint16_t dest, cur;
    uint16_t porta_rate, porta_per;
    uint8_t  porta_reset, porta_time;
    uint8_t  vib_reset, vib_time;
    int8_t   vib_width;
    int16_t  vib_off;
    int16_t  vol;
    uint8_t  env_rate, env_reset, env_time, env_end;
    uint8_t  ab_time, ab_reset;
    int16_t  ab_delta;
    uint32_t reg_start;
    uint16_t reg_len;
    uint32_t lat_start, lat_bytes;
    uint32_t pos_fp, inc_fp;
    uint8_t  dma;
} TfmxVoice;

typedef struct {
    uint8_t  pnum;
    uint32_t paddr, raddr;
    uint16_t pstep, rstep, pwait;
    int16_t  ploop;
    int8_t   xpose;
} TfmxTrack;

typedef struct {
    uint8_t  loaded, playing;
    char     title[244];
    const uint8_t *mdat;  uint32_t mdat_size;
    const int8_t  *smpl;  uint32_t smpl_size;
    uint8_t *owned_buf;
    uint32_t track_off, patt_off, macro_off;
    uint16_t num_steps;
    uint8_t  num_subsongs, current_subsong;
    uint16_t first, last, pos, speed, speed_cnt, step_tempo, bpm;
    int16_t  tloop, master, fade_dest, fade_step;
    uint8_t  fade_time, fade_reset;
    uint32_t rate, spj_fp;
    int32_t  acc_fp;
    uint32_t jiffy, oob_reads;
    TfmxVoice v[TFMX_VOICES];
    TfmxTrack t[TFMX_TRACKS];
} TfmxPlayer;

void tfmx_player_init(TfmxPlayer *p);
int  tfmx_player_load(TfmxPlayer *p, const char *tfx_path, const char *sam_path);
int  tfmx_player_load_mem(TfmxPlayer *p, const uint8_t *mdat, uint32_t mdat_size,
                          const int8_t *smpl, uint32_t smpl_size);
void tfmx_player_free(TfmxPlayer *p);
void tfmx_player_start(TfmxPlayer *p, uint8_t subsong);
void tfmx_player_stop(TfmxPlayer *p);
void tfmx_player_set_rate(TfmxPlayer *p, uint32_t rate);
void tfmx_player_jiffy(TfmxPlayer *p);                 /* Amiga HW: call from VBlank/CIA IRQ */
void tfmx_player_render(TfmxPlayer *p, int16_t *out, int n);
void tfmx_player_tick(TfmxPlayer *p, struct GameAudio *audio); /* legacy: hooks render into GameAudio */
#endif
