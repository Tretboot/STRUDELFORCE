/*
 * game_logic.c - STRUDELFORCE Consolidated Gameplay Logic
 *
 * Monolithisches Kernmodul:
 *   - 16.16 Fixed-Point Physik & Kollision (Null float/double)
 *   - Deterministischer PRNG (LCG) fuer bit-identische Runs
 *   - Turrican 3-Leben-System, Respawn-Unverwundbarkeit & Exploration-Pickups
 *   - 3 Waffen: Spread-Faecher, Laser-Strahl, Bounce 2.0 (mit Eck-Clamp-Fix)
 *   - 3 Gegner: Walker, Shooter, Tank (15 HP Mini-Boss)
 *   - 2 Modi: Endlos-Roguelike (Default) vs. Kampagne (5x3 Wellen)
 *   - Deterministischer Biome- & Chunk-Generator
 *   - ESC Livecode-Editor Puffer- & Tausch-Logik
 *
 * Lizenz: MIT (Clean Room)
 */

#include "strudelforce.h"
#include "level_engine.h"

/* ================= 16.16 Konstanten =================
 * Balance 2026-10-28 (v0.3.0 Feinschliff): entspannteres, weniger hektisches
 * Laufgefuehl. Acc/Speed 25% runter, Spruenge etwas flacher (v^2/2g),
 * Friction auf 0.80 = weicheres Ausrollen. verify_core_sim.py muss
 * die Werte 1:1 spiegeln (Oracle-Regel!). */
#define GRAVITY_FP       FIX(0.45)
#define MAX_FALL_FP      FIX(8.0)
#define JUMP_V_FP        FIX(2.6)
#define MOVE_ACC_FP      FIX(0.25)
#define MAX_SPEED_FP     FIX(2.4)
#define FRICTION_FP      FIX(0.80)
#define STOP_EPSILON     FIX(0.02)

#define PLAYER_HALF_W    FIX(1.0)
#define PLAYER_HALF_H    FIX(1.0)
#define MORPH_HALF_W     32768

#define SPREAD_COOLDOWN  12
#define LASER_COOLDOWN   8
#define BOUNCE_COOLDOWN  15
#define BEAM_DAMAGE_TICK 4
#define WAVE_INTRO_TICKS 100
#define WUG_FLEE_SPEED   FIX(0.75) /* v0.3.2: Wug flieht schneller als WADDLE */
#define SHIELD_REGEN_TICK 10
#define MAX_BOUNCES      4

#define PELLET_SPEED_FP  FIX(8.0)
#define BOUNCE_SPEED_FP  FIX(6.0)
#define BOUNCE_DIAG_FP   278046

static const fix16_t DIR8_DX[8] = {
     FIX(6.0), BOUNCE_DIAG_FP, 0, -BOUNCE_DIAG_FP,
    -FIX(6.0), -BOUNCE_DIAG_FP, 0, BOUNCE_DIAG_FP
};
static const fix16_t DIR8_DY[8] = {
     0, BOUNCE_DIAG_FP, FIX(6.0), BOUNCE_DIAG_FP,
     0, -BOUNCE_DIAG_FP, -FIX(6.0), -BOUNCE_DIAG_FP
};

static const fix16_t SPREAD_FAN[7] = {
     0, FIX(2), FIX(3), FIX(6), -FIX(2), -FIX(3), -FIX(6)
};

static const uint8_t FAN_IDX[5][7] = {
    {0, 0, 0, 0, 0, 0, 0},
    {1, 4, 0, 0, 0, 0, 0},
    {1, 0, 4, 0, 0, 0, 0},
    {2, 1, 0, 4, 5, 0, 0},
    {3, 2, 1, 0, 4, 5, 6}
};
static const int SPREAD_COUNTS[5] = { 1, 2, 3, 5, 7 };

const uint8_t sf_laser_widths[5]  = { 2, 4, 8, 14, 22 };
const uint8_t sf_spread_counts[5] = { 1, 2, 3, 5, 7 };

static const int LASER_THICKNESS[5] = { 2, 4, 8, 14, 22 };
static const int LASER_LENGTH[5]    = { 24, 36, 48, 60, 72 };

/* Biome-Definitionen */
const SFBiomeDef sf_biomedefs[SF_BIOME_COUNT] = {
    { FIX(180), FIX(-560), 16, 0, 0, 0, "user_music/sector1_storm.strudel" },
    { FIX(120), FIX(-700), 14, 1, FIX(1100), SF_GROUND_Y - FIX(16), "user_music/the_great_bath_chest.strudel" },
    { FIX(200), FIX(-600), 4,  0, 0, 0, "user_music/sector1_storm.strudel" }
};

/* Forward-Deklaration v0.3.2 C89 */
static void enemy_damage(GameState *s, Enemy *e, int16_t dmg);

/* ================= Fixed-Point Arithmetik ================= */

fix16_t fix16_from_int(int v) {
    return (fix16_t)v << 16;
}

int fix16_to_int(fix16_t v) {
    return (int)((v + 0x8000) >> 16);
}

fix16_t fix16_mul(fix16_t a, fix16_t b) {
    return (fix16_t)(((int64_t)a * (int64_t)b) >> 16);
}

fix16_t fix16_lerp(fix16_t a, fix16_t b, int32_t k) {
    return a + (fix16_t)(((int64_t)(b - a) * k) >> 16);
}

/* ================= Deterministischer PRNG (LCG) ================= */

uint32_t core_rng(GameState *s) {
    s->rng_state = s->rng_state * 1664525u + 1013904223u;
    return s->rng_state;
}

int32_t sf_rand_range(uint32_t *seed, int32_t lo, int32_t hi) {
    *seed = *seed * 1664525u + 1013904223u;
    return lo + (int32_t)(*seed % (uint32_t)(hi - lo + 1));
}

int core_dir8_from_vec(fix16_t dx, fix16_t dy) {
    if (dx >= 0) {
        if (dy >= 0) return (dx >= dy) ? 0 : 1;
        return (dx >= -dy) ? 7 : 6;
    }
    if (dy >= 0) return ((-dx) >= dy) ? 4 : 3;
    return ((-dx) >= (-dy)) ? 5 : 6;
}

uint8_t core_campaign_biome(const GameState *s) {
    if (s->mode == MODE_CAMPAIGN && s->campaign_level >= 1 && s->campaign_level <= CAMPAIGN_LEVELS)
        return (uint8_t)(s->campaign_level - 1);
    return 0;
}

int biome_next_chunk(uint32_t *seed, int16_t *biome_out, int16_t *chest_open_out) {
    int16_t next_biome = (int16_t)sf_rand_range(seed, 0, SF_BIOME_COUNT - 1);
    int16_t chest = (next_biome == SF_BIOME_GREAT_BATH) ? 1 : 0;
    *biome_out = next_biome;
    *chest_open_out = chest;
    return 1;
}

/* ================= Initialisierung ================= */

void core_setup_platforms(GameState *s) {
    uint8_t b;
    s->platform_count = 0;
    if (s->mode != MODE_CAMPAIGN) return;
    b = core_campaign_biome(s);

    #define ADD_PLAT(px, py, pw, ph, pk) do { \
        if (s->platform_count < MAX_LEVEL_PLATFORMS) { \
            s->platforms[s->platform_count].x = FIX(px); \
            s->platforms[s->platform_count].y = FIX(py); \
            s->platforms[s->platform_count].w = FIX(pw); \
            s->platforms[s->platform_count].h = FIX(ph); \
            s->platforms[s->platform_count].kind = (uint8_t)(pk); \
            s->platform_count++; \
        } \
    } while(0)

    if (b == 0) {
        /* Chrome Tempest: Multi-Tier Industrial Outpost (Turrican 2 World 1 Benchmark) */
        /* Tier 1: Low Catwalks & Bunkers */
        ADD_PLAT(4, 4, 16, 1, 0);
        ADD_PLAT(24, 5, 14, 1, 0);
        ADD_PLAT(42, 5, 14, 1, 0);
        ADD_PLAT(58, 4, 12, 1, 0);
        /* Tier 2: Mid Fortress Girders & Conveyors */
        ADD_PLAT(8, 10, 16, 1, 0);
        ADD_PLAT(28, 11, 18, 1, 0);
        ADD_PLAT(50, 10, 16, 1, 0);
        /* Tier 3: High Skyway Bridges & Watchtowers */
        ADD_PLAT(4, 16, 14, 1, 0);
        ADD_PLAT(22, 17, 20, 1, 0);
        ADD_PLAT(46, 16, 14, 1, 0);
        ADD_PLAT(60, 20, 10, 1, 0);
        /* Tier 4: Apex Command Citadel */
        ADD_PLAT(26, 22, 20, 1, 0);
    } else if (b == 1) {
        /* The Great Bath: Ancient Stepped Aqueduct & Caverns (Turrican 2 World 2) */
        /* Tier 1: Submerged Cistern Slabs */
        ADD_PLAT(2, 3, 16, 1, 1);
        ADD_PLAT(22, 4, 14, 1, 1);
        ADD_PLAT(40, 3, 16, 1, 1);
        ADD_PLAT(58, 4, 12, 1, 1);
        /* Tier 2: Lower Colonnade & Stepped Vaults */
        ADD_PLAT(8, 8, 16, 1, 1);
        ADD_PLAT(28, 9, 20, 1, 1);
        ADD_PLAT(52, 8, 16, 1, 1);
        /* Tier 3: High Terraces & Sacred Altar with Great Bath Chest */
        ADD_PLAT(12, 14, 14, 1, 1);
        ADD_PLAT(32, 15, 18, 1, 1); /* Chest Platform */
        ADD_PLAT(54, 14, 14, 1, 1);
        /* Tier 4: Stalactite Dome Arch */
        ADD_PLAT(22, 20, 28, 1, 1);
    } else if (b == 2) {
        /* Mechanical Hive: Biomechanical Factory Spire (Turrican 3 World 1) */
        /* Tier 1: Smelter & Belt Girders */
        ADD_PLAT(6, 5, 16, 1, 0);
        ADD_PLAT(26, 6, 14, 1, 0);
        ADD_PLAT(44, 5, 16, 1, 0);
        ADD_PLAT(62, 5, 10, 1, 0);
        /* Tier 2: Biomechanical Spine & Piston Bridges */
        ADD_PLAT(10, 11, 16, 1, 0);
        ADD_PLAT(30, 12, 18, 1, 0);
        ADD_PLAT(52, 11, 16, 1, 0);
        /* Tier 3: Cooling Duct Walkways */
        ADD_PLAT(6, 17, 14, 1, 0);
        ADD_PLAT(24, 18, 22, 1, 0);
        ADD_PLAT(50, 17, 14, 1, 0);
        /* Tier 4: Hive Mind Core Reactor */
        ADD_PLAT(26, 23, 20, 1, 0);
    } else if (b == 3) {
        /* Volcanic / Air Combat: Floating Magma Basalts (Turrican 2 World 3) */
        /* Tier 1: Low Magma Stepping Basalts */
        ADD_PLAT(10, 4, 16, 1, 2);
        ADD_PLAT(32, 5, 16, 1, 2);
        ADD_PLAT(54, 4, 16, 1, 2);
        /* Tier 2: Mid Floating Magma Islands */
        ADD_PLAT(6, 10, 18, 1, 2);
        ADD_PLAT(28, 11, 20, 1, 2);
        ADD_PLAT(52, 10, 18, 1, 2);
        /* Tier 3: High Ash Cloud Columns */
        ADD_PLAT(14, 16, 16, 1, 2);
        ADD_PLAT(36, 17, 18, 1, 2);
        ADD_PLAT(56, 16, 14, 1, 2);
        /* Tier 4: Caldera Rim Skyway */
        ADD_PLAT(24, 22, 24, 1, 2);
    } else {
        /* Cyber Fortress: Multi-Tier Battlements (Alien Citadel World 5) */
        /* Tier 1: Outer Wall Defenses */
        ADD_PLAT(6, 4, 16, 1, 0);
        ADD_PLAT(26, 5, 16, 1, 0);
        ADD_PLAT(50, 4, 18, 1, 0);
        /* Tier 2: Citadel Bastion Ramparts */
        ADD_PLAT(10, 10, 18, 1, 0);
        ADD_PLAT(32, 11, 20, 1, 0);
        ADD_PLAT(54, 10, 16, 1, 0);
        /* Tier 3: Forcefield Walkways & Turret Perches */
        ADD_PLAT(8, 16, 16, 1, 0);
        ADD_PLAT(28, 17, 22, 1, 0);
        ADD_PLAT(52, 16, 16, 1, 0);
        /* Tier 4: Mother Alien Command Throne */
        ADD_PLAT(22, 22, 28, 1, 0);
    }
    #undef ADD_PLAT
}

void core_init(GameState *s, uint32_t seed) {
    memset(s, 0, sizeof(GameState));
    s->world_w = fix16_from_int(WORLD_W_UNITS);
    s->world_h = fix16_from_int(WORLD_H_UNITS);
    s->px = fix16_from_int(WORLD_W_UNITS / 2);
    s->py = 0;
    s->on_ground = 1;
    s->facing = 1;
    s->hp = 100;
    s->max_hp = 100;
    s->shield = 50;
    s->lives = START_LIVES;
    s->respawn_timer = 0;
    s->level = 1;
    s->weapon = WEAPON_SPREAD;
    s->weapon_level = 1;
    s->state = STATE_WAVE_INTRO;
    s->state_timer = WAVE_INTRO_TICKS;
    s->mode = MODE_ENDLESS;
    s->campaign_level = 1;
    s->rng_state = seed ? seed : 1u;
    core_setup_platforms(s);
}

/* ================= Spawner ================= */

static void proj_spawn(GameState *s, fix16_t x, fix16_t y, fix16_t vx, fix16_t vy,
                       uint8_t kind, uint8_t level, int16_t damage, uint16_t life) {
    Projectile *p = &s->projectiles[s->proj_head];
    s->proj_head = (uint8_t)((s->proj_head + 1) % MAX_PROJECTILES);
    p->x = x; p->y = y; p->vx = vx; p->vy = vy;
    p->active = 1; p->kind = kind; p->level = level;
    p->bounces = 0; p->life = life; p->damage = damage;
}

static void enemy_spawn(GameState *s, uint8_t kind, fix16_t x) {
    Enemy *e = &s->enemies[s->enemy_head];
    s->enemy_head = (uint8_t)((s->enemy_head + 1) % MAX_ENEMIES);
    e->x = x; e->y = 0; e->vx = 0;
    e->active = 1; e->kind = kind;
    e->hit_timer = 0; e->shoot_timer = 40;
    if (kind == ENEMY_WALKER) {
        e->hp = 1; e->score_val = 100; e->xp_val = 10;
    } else if (kind == ENEMY_SHOOTER) {
        e->hp = 1; e->score_val = 250; e->xp_val = 20;
    } else if (kind == ENEMY_WUG) {
        e->hp = 12; e->score_val = 150; e->xp_val = 20; /* v0.3.2: Wug (Balancing-Iter 2a: 4->12, 4x Laser-L1) */
        e->state = WUG_ST_WADDLE;
        e->state_timer = 120 + (uint16_t)(core_rng(s) & 119); /* 120-240 Ticks (RNG) */
        e->facing = (e->x > s->px) ? -1 : 1;
    } else {
        e->hp = 15; e->score_val = 500; e->xp_val = 40; /* Mini-Boss */
    }
}

static void part_burst(GameState *s, fix16_t x, fix16_t y, uint8_t color, uint8_t n) {
    uint8_t i;
    for (i = 0; i < n; i++) {
        Particle *pt = &s->particles[s->part_head];
        s->part_head = (uint8_t)((s->part_head + 1) % MAX_PARTICLES);
        pt->x = x;
        pt->y = y;
        pt->vx = (fix16_t)(((int32_t)(core_rng(s) & 0xFFFF) - 32768) * 4);
        pt->vy = (fix16_t)(((int32_t)(core_rng(s) & 0xFFFF) - 32768) * 4) + FIX(1);
        pt->active = 1;
        pt->color = color;
        pt->life = 15 + (uint16_t)(core_rng(s) & 15);
    }
}

static void wave_pickup_spawn(GameState *s) {
    int i;
    for (i = 0; i < MAX_PICKUPS; i++) s->pickups[i].active = 0;
    s->pickups[0].x = fix16_from_int(3);
    s->pickups[0].y = 0;
    s->pickups[0].active = 1;
    s->pickups[1].x = s->world_w - fix16_from_int(3);
    s->pickups[1].y = 0;
    s->pickups[1].active = 1;
}

static void pickup_update(GameState *s) {
    int i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &s->pickups[i];
        if (!p->active) continue;
        {
            fix16_t dx = s->px - p->x; if (dx < 0) dx = -dx;
            fix16_t dy = s->py - p->y; if (dy < 0) dy = -dy;
            if (dx < FIX(2) && dy < FIX(3)) {
                p->active = 0;
                if (s->lives < MAX_LIVES) s->lives++;
                s->score += 250;
                part_burst(s, p->x, p->y + FIX(1), 6, 8);
            }
        }
    }
}

static void wave_spawn(GameState *s) {
    int n = 3 + 2 * (int)s->wave;
    int i;
    for (i = 0; i < n; i++) {
        uint8_t kind = ENEMY_WALKER;
        if (s->wave >= 2 && i % 3 == 0) kind = ENEMY_SHOOTER;
        if (s->wave >= 3 && i == n - 1) kind = ENEMY_TANK;
        enemy_spawn(s, kind, (i % 2 == 0) ? fix16_from_int(1)
                                           : s->world_w - fix16_from_int(1));
    }
    /* v0.3.2: Wug-Wellen-Spawn (max 2, ab Welle 2, Biome-Gate: kein Great Bath) */
    if (s->wave >= 2) {
        int wug_active = 0;
        int wi;
        for (wi = 0; wi < MAX_ENEMIES; wi++)
            if (s->enemies[wi].active && s->enemies[wi].kind == ENEMY_WUG) wug_active++;
        if (s->mode == MODE_CAMPAIGN) {
            int b = core_campaign_biome(s);
            if (b != 1) { /* Great Bath (unterwasser) = kein Wug */
                while (wug_active < 2) {
                    enemy_spawn(s, ENEMY_WUG, (s->px < s->world_w / 2) ? s->world_w - fix16_from_int(1) : fix16_from_int(1));
                    wug_active++;
                }
            }
        } else {
            while (wug_active < 2) {
                enemy_spawn(s, ENEMY_WUG, (s->px < s->world_w / 2) ? s->world_w - fix16_from_int(1) : fix16_from_int(1));
                wug_active++;
            }
        }
    }
    wave_pickup_spawn(s);
}

static void player_respawn(GameState *s) {
    int i;
    s->px = fix16_from_int(WORLD_W_UNITS / 2);
    s->py = 0;
    s->pvx = 0;
    s->pvy = 0;
    s->on_ground = 1;
    s->hp = s->max_hp;
    s->shield = 100;
    s->respawn_timer = RESPAWN_INVULN_TICKS;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (s->projectiles[i].active && s->projectiles[i].kind == PROJ_ENEMY) {
            s->projectiles[i].active = 0;
        }
    }
}

static void player_hurt(GameState *s, uint16_t dmg) {
    if (s->morphing) return;
    if (s->respawn_timer > 0) return;
    if (s->shield > 0) {
        if (s->shield >= dmg) {
            s->shield = (uint16_t)(s->shield - dmg);
            return;
        }
        dmg = (uint16_t)(dmg - s->shield);
        s->shield = 0;
    }
    s->hp = (s->hp >= dmg) ? (uint16_t)(s->hp - dmg) : 0;
    if (s->hp == 0) {
        if (s->lives > 1) {
            s->lives--;
            player_respawn(s);
        } else {
            s->state = STATE_GAMEOVER;
        }
    }
}

static void player_fire(GameState *s) {
    if (s->weapon == WEAPON_SPREAD) {
        int lvl = (int)s->weapon_level - 1;
        int n = SPREAD_COUNTS[lvl];
        int i;
        s->fire_cooldown = SPREAD_COOLDOWN;
        s->shots_fired += (uint32_t)n;
        for (i = 0; i < n; i++) {
            fix16_t dy = SPREAD_FAN[FAN_IDX[lvl][i]];
            proj_spawn(s, s->px + ((s->facing > 0) ? PLAYER_HALF_W : -PLAYER_HALF_W),
                       s->py + FIX(1),
                       (s->facing > 0) ? PELLET_SPEED_FP : -PELLET_SPEED_FP, dy,
                       PROJ_PELLET, s->weapon_level, 1, 120);
        }
    } else if (s->weapon == WEAPON_LASER) {
        int lvl = (int)s->weapon_level - 1;
        int len = LASER_LENGTH[lvl];
        int half_band = LASER_THICKNESS[lvl] / 2;
        int dmg = 2 + s->weapon_level;
        int pierce = (s->weapon_level >= 3);
        int hit_list[MAX_ENEMIES];
        int hit_count = 0;
        int nearest = -1;
        int nearest_d = 0x7FFFFFFF;
        int i;
        s->fire_cooldown = LASER_COOLDOWN;
        s->shots_fired++;
        s->laser_len = fix16_from_int(len);
        s->laser_dir = s->facing;
        s->laser_half = (uint8_t)half_band;
        for (i = 0; i < MAX_ENEMIES; i++) {
            Enemy *e = &s->enemies[i];
            fix16_t dx, dy;
            if (!e->active) continue;
            dx = e->x - s->px;
            if (s->facing > 0 ? dx < 0 : dx > 0) continue;
            if (dx < 0) dx = -dx;
            if (dx > fix16_from_int(len)) continue;
            dy = e->y - s->py;
            if (dy < 0) dy = -dy;
            if (dy > fix16_from_int(half_band + 1)) continue;
            hit_list[hit_count++] = i;
            if ((int)dx < nearest_d) { nearest_d = (int)dx; nearest = i; }
        }
        if (pierce) {
            for (i = 0; i < hit_count; i++) {
                Enemy *e = &s->enemies[hit_list[i]];
                enemy_damage(s, e, (int16_t)dmg);
                part_burst(s, e->x, e->y + FIX(1), 3, 4);
            }
        } else if (nearest >= 0) {
            Enemy *e = &s->enemies[nearest];
            enemy_damage(s, e, (int16_t)dmg);
            part_burst(s, e->x, e->y + FIX(1), 3, 4);
        }
    } else {
        int lvl = (int)s->weapon_level;
        fix16_t fx = (s->facing > 0) ? BOUNCE_SPEED_FP : -BOUNCE_SPEED_FP;
        fix16_t x0 = s->px + ((s->facing > 0) ? PLAYER_HALF_W : -PLAYER_HALF_W);
        fix16_t y0 = s->py + FIX(1);
        s->fire_cooldown = BOUNCE_COOLDOWN;
        s->shots_fired++;
        proj_spawn(s, x0, y0,
                   (s->facing > 0) ? BOUNCE_DIAG_FP : -BOUNCE_DIAG_FP, BOUNCE_DIAG_FP,
                   PROJ_BOUNCE, s->weapon_level, 2, 200);
        if (lvl >= 2) {
            proj_spawn(s, x0, y0, fx, 0, PROJ_BOUNCE, s->weapon_level, 2, 200);
        }
        if (lvl >= 3) {
            proj_spawn(s, x0, y0, 0, BOUNCE_SPEED_FP, PROJ_BOUNCE, s->weapon_level, 2, 200);
            proj_spawn(s, x0, y0, 0, -BOUNCE_SPEED_FP, PROJ_BOUNCE, s->weapon_level, 2, 200);
        }
    }
}

static void bounce_hit_wall(GameState *s, Projectile *p) {
    p->bounces++;
    s->bounces_total++;
    part_burst(s, p->x, p->y, 5, 3);
    if (p->bounces > MAX_BOUNCES) {
        p->active = 0;
        return;
    }
    if (p->level >= 5) {
        int best = -1;
        int32_t best_d = 0x7FFFFFFF;
        int i;
        for (i = 0; i < MAX_ENEMIES; i++) {
            const Enemy *e = &s->enemies[i];
            fix16_t dx, dy;
            if (!e->active) continue;
            dx = e->x - p->x; if (dx < 0) dx = -dx;
            dy = e->y - p->y; if (dy < 0) dy = -dy;
            if ((int32_t)(dx + dy) < best_d) { best_d = (int32_t)(dx + dy); best = i; }
        }
        if (best >= 0) {
            fix16_t dx = s->enemies[best].x - p->x;
            fix16_t dy = s->enemies[best].y - p->y;
            int d8 = core_dir8_from_vec(dx, dy);
            p->vx = DIR8_DX[d8];
            p->vy = DIR8_DY[d8];
        }
    } else if (p->level >= 4) {
        fix16_t svx = p->vx;
        fix16_t svy = p->vy;
        p->vx = svy;
        p->vy = svx;
        proj_spawn(s, p->x, p->y, svx, svy, PROJ_BOUNCE, p->level, p->damage, p->life);
    } else if (p->level == 1) {
        p->vx = (p->vx > 0) ? BOUNCE_DIAG_FP : ((p->vx < 0) ? -BOUNCE_DIAG_FP : p->vx);
        p->vy = (p->vy > 0) ? BOUNCE_DIAG_FP : ((p->vy < 0) ? -BOUNCE_DIAG_FP : p->vy);
    }
}

static void proj_update(GameState *s) {
    int i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &s->projectiles[i];
        if (!p->active) continue;
        p->x += p->vx;
        p->y += p->vy;
        {
            int wall = 0;
            if (p->x <= 0) {
                p->x = 0; p->vx = -p->vx; wall = 1;
            } else if (p->x >= s->world_w) {
                p->x = s->world_w; p->vx = -p->vx; wall = 1;
            }
            if (p->y <= 0) {
                p->y = 0; p->vy = -p->vy; wall |= 2;
            } else if (p->y >= s->world_h) {
                p->y = s->world_h; p->vy = -p->vy; wall |= 2;
            }
            if (wall && p->kind == PROJ_BOUNCE) bounce_hit_wall(s, p);
        }
        if (!p->active) continue;
        if (--p->life == 0) {
            p->active = 0;
            part_burst(s, p->x, p->y, 2, 3);
            continue;
        }
        if (p->kind == PROJ_ENEMY) {
            fix16_t dx = p->x - s->px; if (dx < 0) dx = -dx;
            fix16_t dy = p->y - s->py; if (dy < 0) dy = -dy;
            if (dx < PLAYER_HALF_W + FIX(1) && dy < PLAYER_HALF_H + FIX(1)) {
                p->active = 0;
                player_hurt(s, (uint16_t)p->damage);
                if (p->damage >= 20) { /* v0.3.2: Wug-Laser (25 Dmg) => Knockback (Schwelle 20) */
                    s->pvy = FIX(1.5);
                    s->on_ground = 0;
                }
            }
        } else {
            int j;
            for (j = 0; j < MAX_ENEMIES; j++) {
                Enemy *e = &s->enemies[j];
                fix16_t dx, dy;
                if (!e->active) continue;
                dx = p->x - e->x; if (dx < 0) dx = -dx;
                dy = p->y - e->y; if (dy < 0) dy = -dy;
                if (dx < FIX(2) && dy < FIX(2)) {
                    enemy_damage(s, e, p->damage);
                    p->active = 0;
                    part_burst(s, p->x, p->y, 3, 4);
                    break;
                }
            }
        }
    }
}

/* v0.3.2: generischer Feind-Schaden mit Wug-Sonderregeln (COOLDOWN=2x, FLEE bei Verwundung). */
static void enemy_damage(GameState *s, Enemy *e, int16_t dmg) {
    if (!e->active) return;
    if (e->kind == ENEMY_WUG && e->state == WUG_ST_COOLDOWN) dmg *= 2; /* erschopft: 2x Schaden */
    e->hp = (e->hp > (uint16_t)dmg) ? (uint16_t)(e->hp - (uint16_t)dmg) : 0;
    if (e->kind == ENEMY_WUG && e->hp > 0 &&
        e->state != WUG_ST_FLEE && e->state != WUG_ST_CHARGE && e->state != WUG_ST_FIRE) {
        e->state = WUG_ST_FLEE;   /* verletzt -> laeuft weg (Turrican-2-Walker) */
    }
}

static void enemy_update(GameState *s) {
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &s->enemies[i];
        if (!e->active) continue;
        if (e->hp == 0) {
            e->active = 0;
            s->score += e->score_val;
            s->xp += e->xp_val;
            s->enemies_killed++;
            part_burst(s, e->x, e->y + FIX(1), 4, 8);
            continue;
        }
        if (e->hit_timer > 0) e->hit_timer--;
        /* v0.3.2: Wug-Zustandsautomat (eigene Bewegung, ueberspringt Walker-Logik) */
        if (e->kind == ENEMY_WUG) {
            fix16_t wdx = s->px - e->x;
            int8_t  wdir = (wdx > 0) ? 1 : ((wdx < 0) ? -1 : 0);
            e->facing = (e->state == WUG_ST_FLEE) ? (int8_t)(-wdir) : wdir;
            if (e->state == WUG_ST_FLEE) {
                fix16_t fs = WUG_FLEE_SPEED;
                e->x += (e->facing > 0) ? fs : -fs;
                if (e->x < fix16_from_int(1)) { e->x = fix16_from_int(1); e->state = WUG_ST_WADDLE; e->state_timer = 120 + (uint16_t)(core_rng(s) & 119); }
                else if (e->x > s->world_w - fix16_from_int(1)) { e->x = s->world_w - fix16_from_int(1); e->state = WUG_ST_WADDLE; e->state_timer = 120 + (uint16_t)(core_rng(s) & 119); }
            } else {
                e->state_timer--;
                switch (e->state) {
                case WUG_ST_WADDLE:
                    e->x += (e->facing > 0) ? FIX(0.5) : -FIX(0.5);
                    if (e->x < fix16_from_int(1)) e->x = fix16_from_int(1);
                    if (e->x > s->world_w - fix16_from_int(1)) e->x = s->world_w - fix16_from_int(1);
                    if (e->state_timer == 0) { e->state = WUG_ST_STOP; e->state_timer = 30; }
                    break;
                case WUG_ST_STOP:
                    if (e->state_timer == 0) { e->state = WUG_ST_CHARGE; e->state_timer = 60; }
                    break;
                case WUG_ST_CHARGE:
                    if (e->state_timer == 0) { e->state = WUG_ST_FIRE; e->state_timer = 40; }
                    break;
                case WUG_ST_FIRE:
                    if (e->state_timer == 39) {
                        proj_spawn(s, e->x, e->y + FIX(1),
                                   (e->facing > 0) ? FIX(8) : -FIX(8), 0,
                                   PROJ_ENEMY, 0, 25, 150); /* riesiger Wug-Laser: 25 Schaden */
                    }
                    if (e->state_timer == 0) { e->state = WUG_ST_COOLDOWN; e->state_timer = 50; }
                    break;
                case WUG_ST_COOLDOWN:
                    if (e->state_timer == 0) { e->state = WUG_ST_WADDLE; e->state_timer = 120 + (uint16_t)(core_rng(s) & 119); }
                    break;
                default:
                    e->state = WUG_ST_WADDLE; e->state_timer = 120;
                    break;
                }
            }
            {
                fix16_t cdx = s->px - e->x; if (cdx < 0) cdx = -cdx;
                fix16_t cdy = s->py - e->y; if (cdy < 0) cdy = -cdy;
                if (cdx < PLAYER_HALF_W + FIX(1) && cdy < PLAYER_HALF_H + FIX(1)
                        && e->hit_timer == 0) {
                    e->hit_timer = 30;
                    player_hurt(s, 10);
                }
            }
            continue;
        }
        {
            fix16_t dx = s->px - e->x;
            int8_t dir = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
            fix16_t spd = (e->kind == ENEMY_TANK) ? FIX(0.4) : FIX(0.8);
            if (e->kind == ENEMY_SHOOTER) {
                fix16_t dist = (dx > 0) ? dx : -dx;
                if (dist < fix16_from_int(8)) {
                    dir = (int8_t)-dir;
                } else if (dist > fix16_from_int(14)) {
                } else {
                    dir = 0;
                }
                e->shoot_timer--;
                if (e->shoot_timer == 0) {
                    int d8;
                    e->shoot_timer = 40;
                    d8 = core_dir8_from_vec(dx, s->py - e->y);
                    proj_spawn(s, e->x, e->y + FIX(1),
                               DIR8_DX[d8] / 2, DIR8_DY[d8] / 2,
                               PROJ_ENEMY, 0, 5, 150);
                }
            }
            e->vx = (dir > 0) ? spd : ((dir < 0) ? -spd : 0);
            e->x += e->vx;
            if (e->x < fix16_from_int(1)) e->x = fix16_from_int(1);
            if (e->x > s->world_w - fix16_from_int(1)) e->x = s->world_w - fix16_from_int(1);
            {
                fix16_t cdx = s->px - e->x; if (cdx < 0) cdx = -cdx;
                fix16_t cdy = s->py - e->y; if (cdy < 0) cdy = -cdy;
                if (cdx < PLAYER_HALF_W + FIX(1) && cdy < PLAYER_HALF_H + FIX(1)
                        && e->hit_timer == 0) {
                    e->hit_timer = 30;
                    player_hurt(s, (e->kind == ENEMY_TANK) ? 20 : 10);
                }
            }
        }
    }
}

static void part_update(GameState *s) {
    int i;
    for (i = 0; i < MAX_PARTICLES; i++) {
        Particle *pt = &s->particles[i];
        if (!pt->active) continue;
        pt->x += pt->vx;
        pt->y += pt->vy;
        pt->vy -= FIX(0.2);
        if (pt->y < 0) { pt->y = 0; pt->vy = 0; }
        if (--pt->life == 0) pt->active = 0;
    }
}

static void xp_update(GameState *s) {
    while (s->xp >= 50u * (uint16_t)s->level) {
        s->xp -= 50u * (uint16_t)s->level;
        s->level++;
        if (s->level % 2 == 1) {
            if (s->weapon_level < 5) s->weapon_level++;
        } else {
            s->max_hp += 10;
            s->hp += 10;
        }
    }
}

/* ================= 50Hz Haupt-Tick ================= */

void core_tick(GameState *s, uint16_t input) {
    if (s->state == STATE_GAMEOVER || s->state == STATE_VICTORY) return;
    s->tick++;
    if (s->respawn_timer > 0) s->respawn_timer--;

    if (s->state == STATE_WAVE_INTRO) {
        if (s->mode == MODE_CAMPAIGN && s->platform_count == 0)
            level_load_campaign(s);
        if (--s->state_timer == 0) {
            s->wave++;
            wave_spawn(s);
            s->state = STATE_PLAYING;
        }
    } else if (s->state == STATE_PLAYING) {
        int any = 0;
        int i;
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (s->enemies[i].active) { any = 1; break; }
        }
        if (!any) {
            if (s->mode == MODE_CAMPAIGN && (s->wave % CAMPAIGN_WAVES_PER_LEVEL) == 0) {
                s->state = (s->wave >= CAMPAIGN_LEVELS * CAMPAIGN_WAVES_PER_LEVEL)
                               ? STATE_VICTORY : STATE_LEVEL_CLEAR;
                s->state_timer = LEVEL_CLEAR_TICKS;
                if (s->state == STATE_LEVEL_CLEAR)
                    s->score += 1000u * (uint32_t)s->campaign_level;
            } else {
                s->state = STATE_WAVE_INTRO;
                s->state_timer = WAVE_INTRO_TICKS;
            }
        }
    } else if (s->state == STATE_LEVEL_CLEAR) {
        if (--s->state_timer == 0) {
            if (s->campaign_level < CAMPAIGN_LEVELS) {
                s->campaign_level++;
                level_load_campaign(s);
            }
            s->state = STATE_WAVE_INTRO;
            s->state_timer = WAVE_INTRO_TICKS;
        }
    }

    /* Spieler: Bewegung & Physik */
    {
        int dir = 0;
        if (input & INPUT_LEFT) dir -= 1;
        if (input & INPUT_RIGHT) dir += 1;
        s->morphing = (input & INPUT_DOWN) ? 1 : 0;
        if (dir != 0) {
            s->pvx += (dir > 0) ? MOVE_ACC_FP : -MOVE_ACC_FP;
            if (s->pvx > MAX_SPEED_FP) s->pvx = MAX_SPEED_FP;
            if (s->pvx < -MAX_SPEED_FP) s->pvx = -MAX_SPEED_FP;
            s->facing = (int8_t)dir;
        } else {
            s->pvx = fix16_mul(s->pvx, FRICTION_FP);
            if (s->pvx > 0 && s->pvx < STOP_EPSILON) s->pvx = 0;
            if (s->pvx < 0 && s->pvx > -STOP_EPSILON) s->pvx = 0;
        }
        if ((input & (INPUT_UP | INPUT_JUMP)) && s->on_ground) {
            s->pvy = JUMP_V_FP;
            s->on_ground = 0;
        }
        s->pvy -= GRAVITY_FP;
        if (s->pvy < -MAX_FALL_FP) s->pvy = -MAX_FALL_FP;
        s->px += s->pvx;
        s->py += s->pvy;

        /* v0.3.2: Wug-Headbutt (von oben springen: pvy<0, Fuessel am Wug-Kopf -> 5 Schaden = 1-Hit-Kill + Bounce) */
        if (s->pvy < 0) {
            int hb;
            for (hb = 0; hb < MAX_ENEMIES; hb++) {
                Enemy *e = &s->enemies[hb];
                if (!e->active || e->kind != ENEMY_WUG) continue;
                {
                    fix16_t hdx = s->px - e->x; if (hdx < 0) hdx = -hdx;
                    fix16_t head = e->y + FIX(1);
                    if (hdx < FIX(1.5) && s->py <= head + FIX(0.5)) {
                        enemy_damage(s, e, 5);
                        s->pvy = JUMP_V_FP / 2; /* Bounce nach oben */
                        s->on_ground = 0;
                        s->wug_headbutts++;
                    }
                }
            }
        }

        if (s->py <= 0) {
            s->py = 0;
            if (s->pvy < 0) s->pvy = 0;
            s->on_ground = 1;
        } else {
            s->on_ground = 0;
            /* Plattform-Kollision von oben (Drop-Through bei Morphing/Down) */
            if (s->pvy <= 0 && !s->morphing) {
                fix16_t feet = s->py;
                fix16_t prev_feet = s->py - s->pvy;
                uint8_t pi;
                for (pi = 0; pi < s->platform_count; pi++) {
                    const LevelPlatform *pl = &s->platforms[pi];
                    if (s->px >= pl->x && s->px <= pl->x + pl->w) {
                        if (prev_feet >= pl->y && feet <= pl->y + FIX(0.45)) {
                            s->py = pl->y;
                            if (s->pvy < 0) s->pvy = 0;
                            s->on_ground = 1;
                            break;
                        }
                    }
                }
            }
        }
        {
            fix16_t hw = s->morphing ? MORPH_HALF_W : PLAYER_HALF_W;
            if (s->px < hw) { s->px = hw; if (s->pvx < 0) s->pvx = 0; }
            if (s->px > s->world_w - hw) { s->px = s->world_w - hw; if (s->pvx > 0) s->pvx = 0; }
        }

        if (input & INPUT_WEAPSW) {
            s->weapon = (uint8_t)((s->weapon + 1) % WEAPON_COUNT);
            s->fire_cooldown = 4;
        }
        if (s->fire_cooldown > 0) s->fire_cooldown--;

        s->beam_active = (input & INPUT_SECOND) ? 1 : 0;
        if (s->beam_active) {
            int d8;
            int i;
            s->pvx = 0;
            s->beam_angle = (uint8_t)((s->beam_angle + 4) & 255);
            d8 = (s->beam_angle / 32) & 7;
            if (s->tick % BEAM_DAMAGE_TICK == 0) {
                for (i = 0; i < MAX_ENEMIES; i++) {
                    Enemy *e = &s->enemies[i];
                    fix16_t dx, dy, adx, ady;
                    if (!e->active) continue;
                    dx = e->x - s->px;
                    dy = e->y - s->py;
                    adx = (dx > 0) ? dx : -dx;
                    ady = (dy > 0) ? dy : -dy;
                    if (adx > fix16_from_int(5) || ady > fix16_from_int(5)) continue;
                    if (core_dir8_from_vec(dx, dy) == d8) {
                        enemy_damage(s, e, 1); /* v0.3.2: Beam 1 Schaden/Tick (Wug: COOLDOWN 2x) */
                    }
                }
            }
        }

        if ((input & INPUT_FIRE) && s->fire_cooldown == 0) {
            player_fire(s);
        }
    }

    if (s->tick % SHIELD_REGEN_TICK == 0 && s->shield < 100) s->shield++;

    /* v0.3.2: "Always two there are" - 1x-Dialog wenn 2 Wug gleichzeitig */
    if (!s->wug_two_dialog) {
        int wc = 0, di;
        for (di = 0; di < MAX_ENEMIES; di++)
            if (s->enemies[di].active && s->enemies[di].kind == ENEMY_WUG) wc++;
        if (wc >= 2) s->wug_two_dialog = 1;
    }

    proj_update(s);
    enemy_update(s);
    pickup_update(s);
    part_update(s);
    xp_update(s);
}

#ifndef M68K /* Livecode-Editor: Dateizugriff/libc - nicht im Amiga-Build */
/* ================= Livecode-UI Logik ================= */

void livecode_init(LiveCodeUI *ui, const char *default_pattern) {
    memset(ui, 0, sizeof(LiveCodeUI));
    strncpy(ui->buffer, default_pattern ? default_pattern : "", LIVECODE_MAX_LEN);
    ui->buffer[LIVECODE_MAX_LEN] = 0;
    ui->cursor = (int)strlen(ui->buffer);
    strudel_compile(&ui->active, ui->buffer, 0);
}

void livecode_tick(LiveCodeUI *ui) {
    ui->tick++;
    if (ui->tick % LIVECODE_TICKS_PER_STEP == 0) {
        ui->step_in_cycle++;
        if (ui->step_in_cycle >= LIVECODE_STEPS_PER_CYCLE) {
            ui->step_in_cycle = 0;
            ui->cycle_num++;
            if (ui->have_pending) {
                ui->active = ui->pending;
                ui->have_pending = 0;
            }
        }
    }
}

int16_t livecode_current_note(const LiveCodeUI *ui) {
    return strudel_query_step(&ui->active, ui->cycle_num, ui->step_in_cycle,
                              LIVECODE_STEPS_PER_CYCLE);
}

void livecode_edit_char(LiveCodeUI *ui, char c) {
    int n = (int)strlen(ui->buffer);
    if (n >= LIVECODE_MAX_LEN - 1) return;
    memmove(&ui->buffer[ui->cursor + 1], &ui->buffer[ui->cursor],
            (size_t)(n - ui->cursor) + 1);
    ui->buffer[ui->cursor] = c;
    ui->cursor++;
}

void livecode_edit_backspace(LiveCodeUI *ui) {
    if (ui->cursor <= 0) return;
    ui->cursor--;
    memmove(&ui->buffer[ui->cursor], &ui->buffer[ui->cursor + 1],
            strlen(ui->buffer) - ui->cursor);
}

void livecode_edit_cursor(LiveCodeUI *ui, int dir) {
    int n = (int)strlen(ui->buffer);
    ui->cursor += dir;
    if (ui->cursor < 0) ui->cursor = 0;
    if (ui->cursor > n) ui->cursor = n;
}

void livecode_compile_pending(LiveCodeUI *ui) {
    strudel_compile(&ui->pending, ui->buffer, 0);
    ui->have_pending = 1;
}

int livecode_save(LiveCodeUI *ui, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(ui->buffer, f);
    fputc('\n', f);
    fclose(f);
    return 0;
}

int livecode_load(LiveCodeUI *ui, const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    if (!f) return -1;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    line[strcspn(line, "\r\n")] = 0;
    strncpy(ui->buffer, line, LIVECODE_MAX_LEN);
    ui->buffer[LIVECODE_MAX_LEN] = 0;
    ui->cursor = (int)strlen(ui->buffer);
    strudel_compile(&ui->active, ui->buffer, 0);
    ui->cycle_num = 0;
    ui->step_in_cycle = 0;
    ui->have_pending = 0;
    return 0;
}

#define BATH_PATTERN "<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"

void livecode_open_chest(LiveCodeUI *ui) {
    if (ui->chest_opened) return;
    ui->chest_opened = 1;
    ui->biome = 1;
    strncpy(ui->buffer, BATH_PATTERN, LIVECODE_MAX_LEN);
    ui->buffer[LIVECODE_MAX_LEN] = 0;
    ui->cursor = (int)strlen(ui->buffer);
    strudel_compile(&ui->active, ui->buffer, 0);
    ui->cycle_num = 0;
    ui->step_in_cycle = 0;
    ui->have_pending = 0;
}
#endif /* !M68K */

const char *livecode_biome_name(int biome) {
    switch (biome) {
        case 1: return "The Great Bath";
        case 2: return "Mechanical Hive";
        default: return "Chrome Tempest";
    }
}
