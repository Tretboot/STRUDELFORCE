/*
 * strudelforce.h - Master Unified Header for STRUDELFORCE
 *
 * Monolithische Schnittstelle fuer alle Subsysteme:
 *   1. 16.16 Fixed-Point Arithmetik & Core Gameplay Simulation
 *   2. Soundengine Core Hub (TFMX, MOD, Cyber-Paula Synth, Strudel Cleanroom, Game SFX)
 *   3. Engine Core (2.5D Voxel Renderer, Amiga OCS/AGA HW, Retro Visuals)
 *   4. ESC Livecoding UI Console
 *
 * Strikte Regeln:
 *   - Kein float/double im Simulations- und 50Hz-PAL-Loop
 *   - Null malloc/free im Hot-Path (statische Pools & Ringbuffer)
 *   - 50Hz PAL Timing (20ms pro Tick)
 *   - 100% C89/C99 kompatibel
 *
 * Lizenz: MIT (Clean Room)
 */

#ifndef STRUDELFORCE_H
#define STRUDELFORCE_H

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 1. NUMERIK & 16.16 FIXED-POINT ARITHMETIK
 * ============================================================================ */
typedef int32_t fix16_t;

#define FIX16_ONE 65536
#define FIX(v) ((fix16_t)((v) * 65536))
#define sf_fix(v) FIX(v)
#define sf_unfix(v) ((int32_t)((v) >> 16))

fix16_t fix16_from_int(int v);
int     fix16_to_int(fix16_t v);
fix16_t fix16_mul(fix16_t a, fix16_t b);
fix16_t fix16_lerp(fix16_t a, fix16_t b, int32_t k);

#define sf_fix_mul(a, b) fix16_mul((a), (b))
#define sf_fix_lerp(a, b, k) fix16_lerp((a), (b), (k))

/* ============================================================================
 * 2. WELT, ENTSCHEIDUNGEN & EINGABEN
 * ============================================================================ */
#define WORLD_W_UNITS 72
#define WORLD_H_UNITS 30
#define SF_WORLD_W 1280
#define SF_WORLD_H 720
#define SF_GROUND_Y FIX(650)

/* Input-Masken */
#define INPUT_LEFT      0x0001
#define INPUT_RIGHT     0x0002
#define INPUT_UP        0x0004
#define INPUT_DOWN      0x0008
#define INPUT_FIRE      0x0010
#define INPUT_SECOND    0x0020
#define INPUT_WEAPSW    0x0040
#define INPUT_MORPH     0x0080
#define INPUT_LEVELUP   0x0100
#define INPUT_ESCAPE    0x0200
#define INPUT_JUMP      0x0400

/* sf_interfaces Kompatibilitaets-Aliase */
#define SF_INPUT_LEFT      INPUT_LEFT
#define SF_INPUT_RIGHT     INPUT_RIGHT
#define SF_INPUT_UP        INPUT_UP
#define SF_INPUT_DOWN      INPUT_DOWN
#define SF_INPUT_FIRE      INPUT_FIRE
#define SF_INPUT_SECONDARY INPUT_SECOND
#define SF_INPUT_WPN_NEXT  INPUT_WEAPSW
#define SF_INPUT_MORPH     INPUT_MORPH
#define SF_INPUT_LEVELUP   INPUT_LEVELUP
#define SF_INPUT_ESCAPE    INPUT_ESCAPE
#define SF_INPUT_JUMP      INPUT_JUMP

/* Waffen-Typen */
enum {
    WEAPON_SPREAD = 0,
    WEAPON_LASER  = 1,
    WEAPON_BOUNCE = 2,
    WEAPON_COUNT  = 3
};
#define SF_WPN_MULTIPLE   0
#define SF_WPN_LASER      1
#define SF_WPN_BOUNCE     2
#define SF_WPN_SURROUND   3
#define SF_WPN_MORPH      4
#define SF_WPN_POWER_LINE 5
#define SF_WPN_COUNT      6

/* Projektil-Arten */
enum {
    PROJ_PELLET = 0,
    PROJ_BOUNCE = 1,
    PROJ_ENEMY  = 2
};

/* Gegner-Typen */
enum {
    ENEMY_WALKER  = 0,
    ENEMY_SHOOTER = 1,
    ENEMY_TANK    = 2
};

/* Spielzustaende */
enum {
    STATE_WAVE_INTRO  = 0,
    STATE_PLAYING     = 1,
    STATE_GAMEOVER    = 2,
    STATE_LEVEL_CLEAR = 3, /* Kampagne: Level abgeschlossen Intermezzo */
    STATE_VICTORY     = 4  /* Kampagne: alle 5 Level geschafft! */
};

/* Spielmodi */
enum {
    MODE_ENDLESS  = 0, /* Endlos-Roguelike */
    MODE_CAMPAIGN = 1  /* 5 Level x 3 Wellen */
};

#define CAMPAIGN_LEVELS 5
#define CAMPAIGN_WAVES_PER_LEVEL 3
#define LEVEL_CLEAR_TICKS 150

/* Pool-Limits */
#define MAX_PROJECTILES 64
#define MAX_ENEMIES     32
#define MAX_PARTICLES   128
#define MAX_PICKUPS     8

#define SF_MAX_BULLETS     64
#define SF_MAX_ENEMIES     16
#define SF_MAX_PARTICLES   128
#define SF_MAX_CRYSTALS    32

/* Turrican 3-Leben-System */
#define START_LIVES 3
#define MAX_LIVES   9
#define RESPAWN_INVULN_TICKS 100

/* Biome */
#define BIOME_CHROME_TEMPEST 0
#define BIOME_GREAT_BATH     1
#define BIOME_MECH_HIVE      2
#define BIOME_COUNT          3

#define SF_BIOME_CHROME_TEMPEST 0
#define SF_BIOME_GREAT_BATH     1
#define SF_BIOME_MECH_HIVE      2
#define SF_BIOME_COUNT          3

typedef struct {
    fix16_t     gravity;
    fix16_t     jump_vel;
    uint8_t     palette_id;
    uint8_t     underwater;
    fix16_t     chest_x;
    fix16_t     chest_y;
    const char *music_file;
} SFBiomeDef;

extern const SFBiomeDef sf_biomedefs[SF_BIOME_COUNT];

/* ============================================================================
 * 3. ENTITAETEN & GAMESTATE
 * ============================================================================ */
typedef struct {
    fix16_t  x, y;
    fix16_t  vx, vy;
    uint8_t  active;
    uint8_t  kind;      /* PROJ_* */
    uint8_t  level;     /* Waffen-Level */
    uint8_t  bounces;
    uint16_t life;
    int16_t  damage;
} Projectile;

typedef struct {
    fix16_t  x, y;
    fix16_t  vx;
    uint8_t  active;
    uint8_t  kind;      /* ENEMY_* */
    uint16_t hp;
    uint16_t score_val;
    uint16_t xp_val;
    uint16_t hit_timer;
    uint16_t shoot_timer;
} Enemy;

typedef struct {
    fix16_t  x, y;
    fix16_t  vx, vy;
    uint8_t  active;
    uint8_t  color;
    uint16_t life;
} Particle;

typedef struct {
    fix16_t  x, y;
    uint8_t  active;
} Pickup;

typedef struct {
    fix16_t x, y, w, h;
    uint8_t kind; /* 0: solid catwalk, 1: stone pillar, 2: hazard */
} LevelPlatform;

#define MAX_LEVEL_PLATFORMS 16

typedef struct GameState {
    /* Spieler */
    fix16_t  px, py, pvx, pvy;
    uint8_t  on_ground;
    int8_t   facing;      /* -1 links, +1 rechts */
    uint8_t  morphing;    /* Morph-Ball */
    uint8_t  lives;       /* Turrican-Leben */
    uint16_t respawn_timer;
    uint16_t hp, max_hp;
    uint16_t shield;
    uint16_t xp;
    uint16_t level;
    uint8_t  weapon;
    uint8_t  weapon_level;
    uint16_t fire_cooldown;
    uint8_t  beam_active;
    uint8_t  beam_angle;
    uint8_t  chest_opened;
    int16_t  biome;

    /* Welt */
    fix16_t  world_w, world_h;
    uint32_t tick;
    uint32_t score;
    uint16_t wave;
    uint8_t  state;
    uint16_t state_timer;
    uint8_t  mode;
    uint8_t  campaign_level;

    /* Level-Plattformen */
    LevelPlatform platforms[MAX_LEVEL_PLATFORMS];
    uint8_t       platform_count;

    /* Statische Ringbuffer */
    Projectile projectiles[MAX_PROJECTILES];
    Enemy      enemies[MAX_ENEMIES];
    Particle   particles[MAX_PARTICLES];
    Pickup     pickups[MAX_PICKUPS];
    uint8_t    proj_head, enemy_head, part_head;

    /* Deterministischer LCG RNG */
    uint32_t rng_state;

    /* Statistiken */
    uint32_t shots_fired;
    uint32_t enemies_killed;
    uint32_t bounces_total;

    /* Laser Visuals */
    fix16_t  laser_len;
    int8_t   laser_dir;
    uint8_t  laser_half;
} GameState;

/* Core Simulation API */
void     core_init(GameState *s, uint32_t seed);
void     core_tick(GameState *s, uint16_t input_mask);
void     core_setup_platforms(GameState *s);
uint32_t core_rng(GameState *s);
int      core_dir8_from_vec(fix16_t dx, fix16_t dy);
uint8_t  core_campaign_biome(const GameState *s);
int      biome_next_chunk(uint32_t *seed, int16_t *biome_out, int16_t *chest_open_out);

/* Tabellen */
extern const uint8_t sf_laser_widths[5];
extern const uint8_t sf_spread_counts[5];

/* ============================================================================
 * 4. STRUDEL MINI-NOTATION & CLEANROOM AST
 * ============================================================================ */
#define STRUDEL_MAX_NODES 128
#define STRUDEL_MAX_CHILDREN 16
#define STRUDEL_MAX_EVENTS 64

typedef struct {
    int32_t num;
    int32_t den;
} StrudelFrac;

typedef enum {
    S_NODE_ATOM,
    S_NODE_SEQ,
    S_NODE_ALT,
    S_NODE_EUCLID
} StrudelNodeType;

typedef struct {
    StrudelNodeType type;
    int16_t midi_note;
    uint8_t instrument_id;
    uint8_t repeat_mult;
    uint8_t euclid_k;
    uint8_t euclid_n;
    uint8_t num_children;
    uint8_t children[STRUDEL_MAX_CHILDREN];
} StrudelNode;

typedef struct {
    StrudelNode nodes[STRUDEL_MAX_NODES];
    uint16_t node_count;
    uint8_t root_idx;
    char raw_source[512];
} StrudelPattern;

typedef struct {
    StrudelFrac start;
    StrudelFrac end;
    int16_t midi_note;
    uint8_t instrument_id;
} StrudelEvent;

int16_t strudel_parse_note_token(const char *tok);
void    strudel_compile(StrudelPattern *p, const char *source, uint8_t default_instr);
int16_t strudel_query_step(const StrudelPattern *p, uint32_t cycle, uint32_t step, uint32_t steps_per_cycle);

/* ============================================================================
 * 5. LIVE-CODING UI CONSOLE
 * ============================================================================ */
#define LIVECODE_MAX_LEN 512
#define LIVECODE_STEPS_PER_CYCLE 16
#define LIVECODE_TICKS_PER_STEP 3

typedef struct {
    char           buffer[LIVECODE_MAX_LEN + 1];
    int            cursor;
    StrudelPattern active;
    StrudelPattern pending;
    uint8_t        have_pending;
    uint32_t       tick;
    uint32_t       cycle_num;
    uint8_t        step_in_cycle;
    uint8_t        editor_open;
    uint8_t        chest_opened;
    uint8_t        biome;
} LiveCodeUI;

void        livecode_init(LiveCodeUI *ui, const char *default_pattern);
void        livecode_tick(LiveCodeUI *ui);
int16_t     livecode_current_note(const LiveCodeUI *ui);
void        livecode_edit_char(LiveCodeUI *ui, char c);
void        livecode_edit_backspace(LiveCodeUI *ui);
void        livecode_edit_cursor(LiveCodeUI *ui, int dir);
void        livecode_compile_pending(LiveCodeUI *ui);
int         livecode_save(LiveCodeUI *ui, const char *path);
int         livecode_load(LiveCodeUI *ui, const char *path);
void        livecode_open_chest(LiveCodeUI *ui);
const char *livecode_biome_name(int biome);

/* ============================================================================
 * 6. SOUNDENGINE CORE & UNIVERSAL AUDIO HUB
 * ============================================================================ */
#define GAME_AUDIO_RATE 16574
#define GAME_AUDIO_VOICES 8
#define GAME_AUDIO_SAMPLES_PER_TICK 331  /* 16574 / 50 */
#define SF_AUDIO_VOICES 8

typedef struct GameVoice {
    uint8_t  active;
    uint8_t  macro_id;
    int16_t  base_note;
    uint16_t tick_50hz;
    uint32_t pos_fp;
    uint32_t inc_fp;
    uint32_t loop_start;
    uint32_t loop_len;
    int16_t  volume;
} GameVoice;

typedef struct GameAudio {
    int8_t   sample_ram[65536];
    GameVoice voices[GAME_AUDIO_VOICES];
    uint32_t tick;
    uint8_t  bass_macro;
    uint8_t  arp_active;
    uint8_t  pwm_speed;
    uint16_t cutoff_fp;
    uint8_t  quantize_4bit;
    uint32_t last_shots;
    uint32_t last_kills;
    uint32_t last_bounces;
    uint8_t  bass_override_active;
    int16_t  bass_override_note;
    int32_t  lp_state;
    const int8_t *ext_sample_ram;
    uint32_t ext_sample_size;
    void (*ext_render)(void *ctx, int16_t *out, int n);
    void *ext_ctx;
} GameAudio;

/* Game SFX Enumeration */
enum {
    SOUND_SFX_FIRE_SPREAD = 0,
    SOUND_SFX_FIRE_LASER  = 1,
    SOUND_SFX_FIRE_BOUNCE = 2,
    SOUND_SFX_JUMP        = 3,
    SOUND_SFX_HIT         = 4,
    SOUND_SFX_HIT_TANK    = 5,
    SOUND_SFX_EXPLOSION   = 6,
    SOUND_SFX_CRYSTAL     = 7,
    SOUND_SFX_PICKUP      = 8,
    SOUND_SFX_CHEST       = 9
};

/* Soundengine Hub API */
void soundengine_init(GameAudio *a);
void soundengine_update(GameAudio *a, const GameState *s);
void soundengine_render(GameAudio *a, int16_t *out, int n);
void soundengine_render_synth(GameAudio *a, int16_t *out, int n);

/* Explizite Game SFX Trigger */
void sound_sfx_fire(GameAudio *a, uint8_t weapon);
void sound_sfx_jump(GameAudio *a);
void sound_sfx_hit(GameAudio *a, uint8_t is_tank);
void sound_sfx_explosion(GameAudio *a);
void sound_sfx_crystal(GameAudio *a);
void sound_sfx_pickup(GameAudio *a);
void sound_sfx_chest(GameAudio *a);

/* Rueckwaertskompatible Aliase */
#define game_audio_init(a) soundengine_init(a)
#define game_audio_update(a, s) soundengine_update(a, s)
#define game_audio_render(a, out, n) soundengine_render(a, out, n)
#define game_audio_render_synth(a, out, n) soundengine_render_synth(a, out, n)

/* ============================================================================
 * 7. TFMX PLAYER SUBSYSTEM (Chris Huelsbeck 1.5 / Pro / 7V)
 * ============================================================================ */
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

typedef struct TfmxPlayer {
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
void tfmx_player_jiffy(TfmxPlayer *p);
void tfmx_player_render(TfmxPlayer *p, int16_t *out, int n);
void tfmx_player_tick(TfmxPlayer *p, GameAudio *audio);

/* ============================================================================
 * 8. PROTRACKER MOD PLAYER SUBSYSTEM
 * ============================================================================ */
#define MOD_MAX_CH 8

typedef struct {
    const int8_t *data;
    uint32_t len, rep, replen;
    int8_t   fine;
    uint8_t  vol;
} ModSample;

typedef struct {
    uint8_t  sidx, active, cmd, prm, porta_spd, vib_pos, vib_prm, loop_row, loop_cnt, offset_mem;
    int16_t  period, dest, vol, note_per, per_out;
    uint32_t pos_fp, inc_fp;
} ModChan;

typedef struct ModPlayer {
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

void mod_player_init(ModPlayer *m);
int  mod_player_load_mem(ModPlayer *m, const uint8_t *buf, uint32_t size);
void mod_player_start(ModPlayer *m, uint32_t rate);
void mod_player_stop(ModPlayer *m);
void mod_player_tick(ModPlayer *m);
void mod_player_render(ModPlayer *m, int16_t *out, int n);

/* ============================================================================
 * 9. MULTI-SOURCE MIXER (SfMixer)
 * ============================================================================ */
#define SF_MIX_MAX_SOURCES 4
#define SF_MIX_BLOCK 256

typedef void (*SfRenderFn)(void *ctx, int16_t *out, int n);

typedef struct {
    SfRenderFn  fn;
    void       *ctx;
    int16_t     gain;
    int16_t     target;
    uint8_t     muted;
    uint8_t     duck;
    char        name[16];
} SfSource;

typedef struct {
    SfSource src[SF_MIX_MAX_SOURCES];
    int      count;
    int16_t  master;
    int16_t  duck_level;
    uint32_t clips;
    int16_t  scratch[SF_MIX_BLOCK];
} SfMixer;

void sf_mixer_init(SfMixer *mx);
int  sf_mixer_add(SfMixer *mx, SfRenderFn fn, void *ctx, int16_t gain, const char *name);
void sf_mixer_set_gain(SfMixer *mx, int idx, int16_t gain);
void sf_mixer_render(SfMixer *mx, int16_t *out, int n);
void sf_mixer_ext_render(void *ctx, int16_t *out, int n);

/* ============================================================================
 * 10. SOUNDGODIZER POST-FX & WIN32 AUDIO TRANSPORT
 * ============================================================================ */
#define GOD_DELAY_SAMPLES 128

typedef struct {
    int     enabled;
    int     bass;
    int     warm;
    int     drive;
    int     width;
    float   lp_bass;
    float   lp_warm;
    int16_t dly[GOD_DELAY_SAMPLES];
    int     dly_i;
} Soundgodizer;

void god_init(Soundgodizer *g);
void god_set_params(Soundgodizer *g, int bass, int warm, int drive, int width);
void god_reset_state(Soundgodizer *g);
void god_process(Soundgodizer *g, const int16_t *in, int n, int16_t *out_lr);

#define soundgodizer_init(g) god_init(g)
#define soundgodizer_process(g, in, n, out) god_process(g, in, n, out)

/* Win32 waveOut Transport */
#define WIN_AUDIO_RATE 16574
#define WIN_AUDIO_BUFFERS 8
#define WIN_AUDIO_BUF_SAMPLES 331

int  win_audio_open(int sample_rate);
void win_audio_write(const int16_t *samples, int count);
void win_audio_write_frames(const int16_t *frames, int n);
void win_audio_close(void);
int  win_audio_is_active(void);
int  win_audio_selftest(void);

/* ============================================================================
 * 11. ENGINE CORE (2.5D Voxel Render, Amiga HW & Retro Visuals)
 * ============================================================================ */
#define SF_VOXEL_MAX_ITEMS 1024

typedef struct {
    fix16_t x, y, w, h, z;
    uint8_t kind;
    uint8_t pal;
    uint8_t layer;
    uint8_t flags;
} VoxelRenderItem;

typedef struct {
    const char   *name;
    int           w;
    int           h;
    const uint8_t *colors;
    const uint8_t *depth;
} SpriteDef;

void     voxel_init(void);
uint16_t voxel_build_render_plan(const GameState *s, VoxelRenderItem *out, uint16_t max_items);

/* Amiga Hardware Shims */
void    amiga_hw_init(void);
void    amiga_hw_vblank(void);
void    amiga_hw_set_underwater(uint8_t on);
void    amiga_hw_play_sample(const int8_t *data, uint16_t len, uint8_t voice, uint8_t loop);
uint8_t amiga_hw_hardware_present(void);
int     amiga_hw_render(const VoxelRenderItem *items, uint16_t n, const GameState *s);

/* Amiga Intro */
int amiga_intro_run(int interactive, int max_frames);

#ifdef __cplusplus
}
#endif

#endif /* STRUDELFORCE_H */
