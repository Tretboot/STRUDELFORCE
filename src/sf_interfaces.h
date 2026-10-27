#ifndef SF_INTERFACES_H
#define SF_INTERFACES_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "strudel_cleanroom.h"

/* 1. NUMERIK: 16.16 Fixed-Point (fix16_t, 1.0 == 65536) */
typedef int32_t fix16_t;
#define sf_fix(x)   ((fix16_t)((x) * 65536))
#define sf_unfix(x) ((int32_t)((x) >> 16))

static inline fix16_t sf_fix_mul(fix16_t a, fix16_t b) {
    return (fix16_t)(((int64_t)a * (int64_t)b) >> 16);
}

static inline fix16_t sf_fix_lerp(fix16_t a, fix16_t b, int32_t k) {
    return a + (fix16_t)(((int64_t)(b - a) * k) >> 16);
}

/* 2. WORLD & CONSTANTS */
#define SF_WORLD_W 1280
#define SF_WORLD_H 720
#define SF_GROUND_Y sf_fix(650)

#define SF_MAX_BULLETS     64
#define SF_MAX_ENEMIES     16
#define SF_MAX_PARTICLES   128
#define SF_MAX_CRYSTALS    32
#define SF_VOXEL_MAX_ITEMS 1024
#define SF_CP_VOICES       8
#define SF_AUDIO_VOICES    8
#define SF_CP_STEPS_PER_CYCLE 16

/* 3. INPUT MASKS */
#define SF_INPUT_LEFT      0x0001
#define SF_INPUT_RIGHT     0x0002
#define SF_INPUT_UP        0x0004
#define SF_INPUT_DOWN      0x0008
#define SF_INPUT_FIRE      0x0010
#define SF_INPUT_SECONDARY 0x0020
#define SF_INPUT_WPN_NEXT  0x0040
#define SF_INPUT_MORPH     0x0080
#define SF_INPUT_LEVELUP   0x0100
#define SF_INPUT_ESCAPE    0x0200
#define SF_INPUT_JUMP      0x0400

/* 4. WEAPONS */
#define SF_WPN_MULTIPLE   0
#define SF_WPN_LASER      1
#define SF_WPN_BOUNCE     2
#define SF_WPN_SURROUND   3
#define SF_WPN_MORPH      4
#define SF_WPN_POWER_LINE 5
#define SF_WPN_COUNT      6

/* 5. ENTITY TYPES */
#define SF_ETYPE_BULLET  1
#define SF_ETYPE_LASER   2
#define SF_ETYPE_DEBRIS  3
#define SF_ETYPE_CRYSTAL 4

/* 6. BIOMES */
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

/* 7. ENTITIES & GAMESTATE */
typedef struct {
    int16_t  hp;
    int16_t  shield;
    uint8_t  level;
    int32_t  xp;
    uint8_t  weapon;
    uint8_t  wpn_lvl;
    fix16_t  x, y;
    fix16_t  vx, vy;
    int8_t   facing;
    uint8_t  on_ground;
    uint8_t  morph;
    fix16_t  beam_angle;
    uint8_t  buff_laser_w;
    uint8_t  buff_shield_res;
    uint8_t  buff_drone;
} SFPlayer;

typedef struct {
    uint8_t  active;
    uint8_t  kind;
    fix16_t  x, y;
    fix16_t  vx, vy;
    fix16_t  w, h;
    int16_t  hp;
    uint8_t  shatter;
    fix16_t  base_y;
    uint16_t fire_cd;
} SFEnemy;

typedef struct {
    uint8_t  active;
    uint8_t  etype;
    uint8_t  wpn;
    uint8_t  lvl;
    fix16_t  x, y;
    fix16_t  vx, vy;
    fix16_t  w, h;
    int16_t  dmg;
    uint16_t ttl;
    uint8_t  bounces;
    uint8_t  pal;
} SFEntity;

typedef struct {
    uint32_t tick;
    int32_t  score;
    int16_t  biome;
    uint32_t seed;
    uint8_t  game_over;
    uint8_t  chest_opened;
    uint8_t  levelup_pending;
    uint8_t  levelup_idx;
    SFPlayer player;
    SFEnemy  enemies[SF_MAX_ENEMIES];
    SFEntity bullets[SF_MAX_BULLETS];
    SFEntity particles[SF_MAX_PARTICLES];
    SFEntity crystals[SF_MAX_CRYSTALS];
} GameState;

/* 8. VOXEL RENDER ITEM */
typedef struct {
    fix16_t x, y, w, h, z;
    uint8_t kind;
    uint8_t pal;
    uint8_t layer;
    uint8_t flags;
} VoxelRenderItem;

/* 9. AUDIO SNAPSHOT VOICE (Agent 5 Amiga Backport) */
typedef struct {
    uint8_t  active;
    uint8_t  macro_id;
    int16_t  base_note;
    int16_t  volume;
    uint16_t tick_50hz;
    uint32_t sample_inc_fp;
    uint32_t loop_start;
    uint32_t loop_len;
} SF_AudioVoice;

/* 10. CYBER-PAULA DIAGNOSTICS */
typedef struct {
    fix16_t cutoff_fp;
    fix16_t q_fp;
    uint8_t macro_bass;
    uint8_t sub_bass_up;
    uint8_t low_hp_mode;
    uint8_t great_bath;
    uint8_t quant_drums;
    int     active_voices;
} CyberPaulaDiagnostics;

/* 11. ESC LIVE-CODING UI */
#define LIVECODE_SOURCE_LEN 512
typedef struct {
    uint8_t        open;
    uint8_t        tab;
    char           source[LIVECODE_SOURCE_LEN];
    int16_t        cursor;
    StrudelPattern compiled;
    uint8_t        compile_ok;
    uint8_t        swap_pending;
    char           error[64];
} LiveCodeUI;

/* 12. EXPORTED CONSTANTS & TABLES */
extern const uint8_t sf_laser_widths[5];
extern const uint8_t sf_spread_counts[5];
extern const SFBiomeDef sf_biomedefs[SF_BIOME_COUNT];

/* 13. FUNCTION DECLARATIONS */
int32_t sf_rand_range(uint32_t *seed, int32_t lo, int32_t hi);
void    core_init(GameState *s, int16_t biome, uint32_t seed);
void    core_tick(GameState *s, uint16_t input_mask);
int     biome_next_chunk(uint32_t *seed, int16_t *biome_out, int16_t *chest_open_out);

void    cyber_paula_init(void);
void    cyber_paula_update_from_game(const GameState *s);
const CyberPaulaDiagnostics *cyber_paula_diag(void);
void    cyber_paula_voices(SF_AudioVoice *out, int max);
int     cyber_paula_load_strudel_file(const char *path);
void    cyber_paula_tick_50hz(GameState *s, uint32_t sample_rate);

void     voxel_init(void);
uint16_t voxel_build_render_plan(const GameState *s, VoxelRenderItem *out, uint16_t max_items);

void        livecode_init(LiveCodeUI *ui, const char *initial_source);
void        livecode_on_input(LiveCodeUI *ui, uint16_t input_mask, uint16_t ascii_or_0);
int         livecode_compile_swap(LiveCodeUI *ui);
int         livecode_save(const LiveCodeUI *ui, const char *path);
int         livecode_load(LiveCodeUI *ui, const char *path);
int         livecode_list_user_music(char (*names)[64], int max, int *count);
const char *livecode_get_source(const LiveCodeUI *ui);

void    amiga_hw_init(void);
void    amiga_hw_vblank(void);
void    amiga_hw_set_underwater(uint8_t on);
void    amiga_hw_play_sample(const int8_t *data, uint16_t len, uint8_t voice, uint8_t loop);
uint8_t amiga_hw_hardware_present(void);
int     amiga_hw_render(const VoxelRenderItem *items, uint16_t n, const GameState *s);

#endif /* SF_INTERFACES_H */
