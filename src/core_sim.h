/*
 * core_sim.h - STRUDELFORCE Gameplay Core (Meilenstein 2)
 *
 * Strikte Regeln (specs/AGENT_1_CORE_GAMEPLAY.md):
 *   - Kein float/double: 16.16 Fixed-Point (fix16_t, 1.0 == 65536)
 *   - Kein malloc in der Gameplay-Schleife: statische Ringbuffer
 *   - 50Hz PAL-Tick: core_tick() berechnet exakt 1/50 Sekunde
 *
 * Lizenz: MIT (Clean Room)
 */
#ifndef CORE_SIM_H
#define CORE_SIM_H

#include <stdint.h>

typedef int32_t fix16_t; /* 16.16 Fixed-Point: 1.0 == 65536 */

/* Input-Masken (INPUT_WEAPSW ist edge-getriggert, Verantwortung des Callers) */
#define INPUT_LEFT   0x0001
#define INPUT_RIGHT  0x0002
#define INPUT_UP     0x0004
#define INPUT_DOWN   0x0008
#define INPUT_FIRE   0x0010
#define INPUT_SECOND 0x0020
#define INPUT_WEAPSW 0x0040

enum {
    WEAPON_SPREAD = 0,
    WEAPON_LASER  = 1,
    WEAPON_BOUNCE = 2,
    WEAPON_COUNT  = 3
};

enum {
    STATE_WAVE_INTRO = 0,
    STATE_PLAYING    = 1,
    STATE_GAMEOVER   = 2
};

enum {
    PROJ_PELLET = 0, /* Spread-Pellets */
    PROJ_BOUNCE = 1, /* Bounce 2.0 Prismen */
    PROJ_ENEMY  = 2  /* Gegner-Schuesse */
};

enum {
    ENEMY_WALKER  = 0,
    ENEMY_SHOOTER = 1,
    ENEMY_TANK    = 2
};

#define MAX_PROJECTILES 64
#define MAX_ENEMIES     32
#define MAX_PARTICLES   128

typedef struct {
    fix16_t  x, y;
    fix16_t  vx, vy;
    uint8_t  active;
    uint8_t  kind;      /* PROJ_* */
    uint8_t  level;     /* Waffen-Level (Bounce-Logik) */
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
    uint16_t hit_timer;   /* Kontakt-Schaden Cooldown */
    uint16_t shoot_timer; /* Schuesser-Feuer-Timer */
} Enemy;

typedef struct {
    fix16_t  x, y;
    fix16_t  vx, vy;
    uint8_t  active;
    uint8_t  color;      /* 0..7 (Display) */
    uint16_t life;
} Particle;

typedef struct {
    /* Spieler */
    fix16_t  px, py, pvx, pvy;
    uint8_t  on_ground;
    int8_t   facing;      /* -1 links, +1 rechts */
    uint8_t  morphing;    /* DOWN halten: 1x1 Hitbox, unverwundbar gegen Standard-Schuesse */
    uint16_t hp, max_hp;
    uint16_t shield;      /* 0..100 */
    uint16_t xp;
    uint16_t level;
    uint8_t  weapon;      /* WEAPON_* */
    uint8_t  weapon_level;/* 1..5 */
    uint16_t fire_cooldown;
    uint8_t  beam_active;
    uint8_t  beam_angle;  /* 0..255 == 360 Grad */
    /* Welt */
    fix16_t  world_w, world_h;
    uint32_t tick;
    uint32_t score;
    uint16_t wave;
    uint8_t  state;       /* STATE_* */
    uint16_t state_timer;
    /* Statische Entity-Buffer (kein malloc in der Schleife) */
    Projectile projectiles[MAX_PROJECTILES];
    Enemy      enemies[MAX_ENEMIES];
    Particle   particles[MAX_PARTICLES];
    uint8_t    proj_head, enemy_head, part_head; /* Ringbuffer-Heads */
    /* Deterministischer PRNG (LCG) fuer reproduzierbare Headless-Runs */
    uint32_t rng_state;
    /* Zhaehler (Tests & HUD) */
    uint32_t shots_fired;
    uint32_t enemies_killed;
    uint32_t bounces_total;
    /* Laser-Rendering (letzter Tick) */
    fix16_t  laser_len;
    int8_t   laser_dir;
    uint8_t  laser_half;
} GameState;

/* --- Fixed-Point-Helfer --- */
fix16_t fix16_from_int(int v);
int     fix16_to_int(fix16_t v);
fix16_t fix16_mul(fix16_t a, fix16_t b);

/* --- Core-API --- */
void     core_init(GameState *s, uint32_t seed);
void     core_tick(GameState *s, uint16_t input_mask);
uint32_t core_rng(GameState *s);                     /* deterministischer LCG */
int      core_dir8_from_vec(fix16_t dx, fix16_t dy); /* 0..7 (0=Ost, 2=Nord, 4=West, 6=Sued) */

#endif /* CORE_SIM_H */
