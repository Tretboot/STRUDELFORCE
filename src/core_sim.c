/*
 * core_sim.c - STRUDELFORCE Gameplay Core (Meilenstein 2)
 *
 * Strikte Regeln (specs/AGENT_1_CORE_GAMEPLAY.md):
 *   - Kein float/double: 16.16 Fixed-Point (fix16_t, 1.0 == 65536)
 *   - Kein malloc in der Gameplay-Schleife: statische Ringbuffer
 *   - 50Hz PAL-Tick: core_tick() == exakt 1/50 Sekunde
 *
 * Lizenz: MIT (Clean Room)
 */
#include "core_sim.h"
#include <string.h>

/* ================= 16.16 Konstanten (Wert * 65536) ================= */
#define FIX(v)        ((fix16_t)((v) * 65536))

#define WORLD_W_UNITS    72
#define WORLD_H_UNITS    30

#define GRAVITY_FP       FIX(0.5)     /* Einheiten/Tick^2 */
#define MAX_FALL_FP      FIX(8.0)
#define JUMP_V_FP        FIX(3.0)     /* Sprunghoehe = v^2/(2g) = 9 Einheiten */
#define MOVE_ACC_FP      FIX(0.35)
#define MAX_SPEED_FP     FIX(3.0)
#define FRICTION_FP      FIX(0.75)
#define STOP_EPSILON     FIX(0.02)

#define PLAYER_HALF_W    FIX(1.0)     /* Hitbox 2x2 Einheiten */
#define PLAYER_HALF_H    FIX(1.0)
#define MORPH_HALF_W     32768        /* 0.5 Einheiten (Morph-Rad: 8x8 bei 2px/Einheit) */

#define SPREAD_COOLDOWN  12
#define LASER_COOLDOWN   8
#define BOUNCE_COOLDOWN  15
#define BEAM_DAMAGE_TICK 4
#define WAVE_INTRO_TICKS 100
#define SHIELD_REGEN_TICK 10
#define MAX_BOUNCES      4

#define PELLET_SPEED_FP  FIX(8.0)
#define BOUNCE_SPEED_FP  FIX(6.0)
#define BOUNCE_DIAG_FP   278046       /* 6 * cos(45deg) * 65536 = 278045.69 -> 278046 (Review-Fund Meeseeks #2) */

/* 8-Richtungstabelle (68k-freundlich: keine Trigonometrie, kein float)
 * Index: 0=Ost 1=NO 2=Nord 3=NW 4=West 5=SW 6=Sued 7=SO */
static const fix16_t DIR8_DX[8] = {
     FIX(6.0), BOUNCE_DIAG_FP, 0, -BOUNCE_DIAG_FP,
    -FIX(6.0), -BOUNCE_DIAG_FP, 0, BOUNCE_DIAG_FP
};
static const fix16_t DIR8_DY[8] = {
     0, BOUNCE_DIAG_FP, FIX(6.0), BOUNCE_DIAG_FP,
     0, -BOUNCE_DIAG_FP, -FIX(6.0), -BOUNCE_DIAG_FP
};

/* Spread-Fächer: dy-Offsets in Einheiten (Index 0..6) */
static const fix16_t SPREAD_FAN[7] = {
     0, FIX(2), FIX(3), FIX(6), -FIX(2), -FIX(3), -FIX(6)
};
/* Fächer-Index-Zuordnung pro Level (1..5 -> 1..7 Projektile) */
static const uint8_t FAN_IDX[5][7] = {
    {0, 0, 0, 0, 0, 0, 0},
    {1, 4, 0, 0, 0, 0, 0},
    {1, 0, 4, 0, 0, 0, 0},
    {2, 1, 0, 4, 5, 0, 0},
    {3, 2, 1, 0, 4, 5, 6}
};
static const int SPREAD_COUNTS[5] = { 1, 2, 3, 5, 7 };

/* Laser: Dicke in Voxel-Zellen (Spec: 2,4,8,14,22), Laenge in Welt-Einheiten */
static const int LASER_THICKNESS[5] = { 2, 4, 8, 14, 22 };
static const int LASER_LENGTH[5]    = { 24, 36, 48, 60, 72 };

/* ================= Fixed-Point-Helfer ================= */

fix16_t fix16_from_int(int v) {
    return (fix16_t)v << 16;
}

int fix16_to_int(fix16_t v) {
    return (int)((v + 0x8000) >> 16); /* Rundung auf naechste Ganzzahl */
}

fix16_t fix16_mul(fix16_t a, fix16_t b) {
    /* int64-Zwischenstufe: |a*b| <= 2^62, passt sicher in int64_t */
    return (fix16_t)(((int64_t)a * (int64_t)b) >> 16);
}

/* ================= Deterministischer PRNG (LCG) ================= */

uint32_t core_rng(GameState *s) {
    s->rng_state = s->rng_state * 1664525u + 1013904223u;
    return s->rng_state;
}

/* ================= 8-Richtungs-Geometrie (ohne Trigo) ================= */

int core_dir8_from_vec(fix16_t dx, fix16_t dy) {
    if (dx >= 0) {
        if (dy >= 0) return (dx >= dy) ? 0 : 1;
        return (dx >= -dy) ? 7 : 6;
    }
    if (dy >= 0) return ((-dx) >= dy) ? 4 : 3;
    return ((-dx) >= (-dy)) ? 5 : 6;
}

/* ================= Initialisierung ================= */

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
    s->level = 1;
    s->weapon = WEAPON_SPREAD;
    s->weapon_level = 1;
    s->state = STATE_WAVE_INTRO;
    s->state_timer = WAVE_INTRO_TICKS;
    s->rng_state = seed ? seed : 1u;
}

/* ================= Ringbuffer-Spawner ================= */

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
        e->hp = 3; e->score_val = 100; e->xp_val = 10;
    } else if (kind == ENEMY_SHOOTER) {
        e->hp = 5; e->score_val = 250; e->xp_val = 20;
    } else {
        e->hp = 12; e->score_val = 500; e->xp_val = 40;
    }
}

static void part_burst(GameState *s, fix16_t x, fix16_t y, uint8_t color, uint8_t n) {
    uint8_t i;
    for (i = 0; i < n; i++) {
        Particle *pt = &s->particles[s->part_head];
        s->part_head = (uint8_t)((s->part_head + 1) % MAX_PARTICLES);
        pt->x = x;
        pt->y = y;
        /* Zufallsimpuls -4..+4 Einheiten/Tick (deterministisch via LCG) */
        pt->vx = (fix16_t)(((int32_t)(core_rng(s) & 0xFFFF) - 32768) * 4);
        pt->vy = (fix16_t)(((int32_t)(core_rng(s) & 0xFFFF) - 32768) * 4) + FIX(1);
        pt->active = 1;
        pt->color = color;
        pt->life = 15 + (uint16_t)(core_rng(s) & 15);
    }
}

/* ================= Wellen-Management ================= */

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
}

/* ================= Schaden ================= */

static void player_hurt(GameState *s, uint16_t dmg) {
    if (s->morphing) return; /* Morph-Rad: unverwundbar gegen Standard-Schuesse */
    if (s->shield > 0) {
        if (s->shield >= dmg) {
            s->shield = (uint16_t)(s->shield - dmg);
            return;
        }
        dmg = (uint16_t)(dmg - s->shield);
        s->shield = 0;
    }
    s->hp = (s->hp >= dmg) ? (uint16_t)(s->hp - dmg) : 0;
    if (s->hp == 0) s->state = STATE_GAMEOVER;
}

/* ================= Waffen ================= */

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
        int half_band = LASER_THICKNESS[lvl] / 2; /* Zellen -> Einheiten */
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
            if (dy > fix16_from_int(half_band + 1)) continue; /* +1: Gegner-Halbhoehe */
            hit_list[hit_count++] = i;
            if ((int)dx < nearest_d) { nearest_d = (int)dx; nearest = i; }
        }
        if (pierce) {
            for (i = 0; i < hit_count; i++) {
                Enemy *e = &s->enemies[hit_list[i]];
                e->hp = (e->hp > (uint16_t)dmg) ? (uint16_t)(e->hp - (uint16_t)dmg) : 0;
                part_burst(s, e->x, e->y + FIX(1), 3, 4);
            }
        } else if (nearest >= 0) {
            Enemy *e = &s->enemies[nearest];
            e->hp = (e->hp > (uint16_t)dmg) ? (uint16_t)(e->hp - (uint16_t)dmg) : 0;
            part_burst(s, e->x, e->y + FIX(1), 3, 4);
        }
    } else { /* WEAPON_BOUNCE (Geometric Prism) */
        int lvl = (int)s->weapon_level;
        fix16_t fx = (s->facing > 0) ? BOUNCE_SPEED_FP : -BOUNCE_SPEED_FP;
        fix16_t x0 = s->px + ((s->facing > 0) ? PLAYER_HALF_W : -PLAYER_HALF_W);
        fix16_t y0 = s->py + FIX(1);
        s->fire_cooldown = BOUNCE_COOLDOWN;
        s->shots_fired++;
        /* L1: 45 Grad schraeg nach vorne-oben */
        proj_spawn(s, x0, y0,
                   (s->facing > 0) ? BOUNCE_DIAG_FP : -BOUNCE_DIAG_FP, BOUNCE_DIAG_FP,
                   PROJ_BOUNCE, s->weapon_level, 2, 200);
        if (lvl >= 2) {
            /* L2: + 0 Grad (geradeaus) */
            proj_spawn(s, x0, y0, fx, 0, PROJ_BOUNCE, s->weapon_level, 2, 200);
        }
        if (lvl >= 3) {
            /* L3: + 90 Grad (senkrecht hoch und runter) */
            proj_spawn(s, x0, y0, 0, BOUNCE_SPEED_FP, PROJ_BOUNCE, s->weapon_level, 2, 200);
            proj_spawn(s, x0, y0, 0, -BOUNCE_SPEED_FP, PROJ_BOUNCE, s->weapon_level, 2, 200);
        }
        /* L4/L5: Wand-Split / Smart-Ricochet in bounce_hit_wall() */
    }
}

/* Bounce 2.0: Wandkontakt-Logik pro Level */
static void bounce_hit_wall(GameState *s, Projectile *p) {
    p->bounces++;
    s->bounces_total++;
    part_burst(s, p->x, p->y, 5, 3);
    if (p->bounces > MAX_BOUNCES) {
        p->active = 0;
        return;
    }
    if (p->level >= 5) {
        /* L5: Smart-Ricochet — Winkel zeigt Richtung naechster aktiver Gegner */
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
        /* L4: Wandtreffer spaltet das Projektil in 2 Sekundaerwinkel (90-Grad-Rotation) */
        fix16_t svx = p->vx;
        fix16_t svy = p->vy;
        p->vx = svy;
        p->vy = svx;
        proj_spawn(s, p->x, p->y, svx, svy, PROJ_BOUNCE, p->level, p->damage, p->life);
    } else if (p->level == 1) {
        /* L1: 45-Grad-Abprallwinkel — Geschwindigkeit auf 45-Grad-Diagonale zwingen */
        p->vx = (p->vx > 0) ? BOUNCE_DIAG_FP : ((p->vx < 0) ? -BOUNCE_DIAG_FP : p->vx);
        p->vy = (p->vy > 0) ? BOUNCE_DIAG_FP : ((p->vy < 0) ? -BOUNCE_DIAG_FP : p->vy);
    }
}

/* ================= Projektile ================= */

static void proj_update(GameState *s) {
    int i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &s->projectiles[i];
        if (!p->active) continue;
        p->x += p->vx;
        p->y += p->vy;
        /* Wandabprall (nur Bounce-Projektile nutzen die Bounce-Logik) */
        if (p->x <= 0) {
            p->x = 0; p->vx = -p->vx;
            if (p->kind == PROJ_BOUNCE) bounce_hit_wall(s, p);
        } else if (p->x >= s->world_w) {
            p->x = s->world_w; p->vx = -p->vx;
            if (p->kind == PROJ_BOUNCE) bounce_hit_wall(s, p);
        }
        if (p->y <= 0) {
            p->y = 0; p->vy = -p->vy;
            if (p->kind == PROJ_BOUNCE) bounce_hit_wall(s, p);
        } else if (p->y >= s->world_h) {
            p->y = s->world_h; p->vy = -p->vy;
            if (p->kind == PROJ_BOUNCE) bounce_hit_wall(s, p);
        }
        if (!p->active) continue;
        if (--p->life == 0) {
            p->active = 0;
            part_burst(s, p->x, p->y, 2, 3);
            continue;
        }
        /* Kollisionen */
        if (p->kind == PROJ_ENEMY) {
            fix16_t dx = p->x - s->px; if (dx < 0) dx = -dx;
            fix16_t dy = p->y - s->py; if (dy < 0) dy = -dy;
            if (dx < PLAYER_HALF_W + FIX(1) && dy < PLAYER_HALF_H + FIX(1)) {
                p->active = 0;
                player_hurt(s, (uint16_t)p->damage);
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
                    e->hp = (e->hp > (uint16_t)p->damage) ? (uint16_t)(e->hp - (uint16_t)p->damage) : 0;
                    p->active = 0;
                    part_burst(s, p->x, p->y, 3, 4);
                    break;
                }
            }
        }
    }
}

/* ================= Gegner ================= */

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
        {
            fix16_t dx = s->px - e->x;
            int8_t dir = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
            fix16_t spd = (e->kind == ENEMY_TANK) ? FIX(0.4) : FIX(0.8);
            if (e->kind == ENEMY_SHOOTER) {
                fix16_t dist = (dx > 0) ? dx : -dx;
                if (dist < fix16_from_int(8)) {
                    dir = (int8_t)-dir; /* Distanz halten */
                } else if (dist > fix16_from_int(14)) {
                    /* anmauern */
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
            /* Kontakt-Schaden */
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

/* ================= Partikel ================= */

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

/* ================= XP / Level-Up (Survivors-Layer) ================= */

static void xp_update(GameState *s) {
    while (s->xp >= 50u * (uint16_t)s->level) {
        s->xp -= 50u * (uint16_t)s->level;
        s->level++;
        if (s->level % 2 == 1) {
            if (s->weapon_level < 5) s->weapon_level++; /* Waffen-Upgrade */
        } else {
            s->max_hp += 10;
            s->hp += 10;                                 /* Schild-Resonanz */
        }
    }
}

/* ================= 50Hz Haupt-Tick ================= */

void core_tick(GameState *s, uint16_t input) {
    if (s->state == STATE_GAMEOVER) return;
    s->tick++;

    /* --- Wellen-Management --- */
    if (s->state == STATE_WAVE_INTRO) {
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
            s->state = STATE_WAVE_INTRO;
            s->state_timer = WAVE_INTRO_TICKS;
        }
    }

    /* --- Spieler: Bewegung & Physik --- */
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
        if ((input & INPUT_UP) && s->on_ground) {
            s->pvy = JUMP_V_FP;
            s->on_ground = 0;
        }
        s->pvy -= GRAVITY_FP;
        if (s->pvy < -MAX_FALL_FP) s->pvy = -MAX_FALL_FP;
        s->px += s->pvx;
        s->py += s->pvy;
        if (s->py <= 0) {
            s->py = 0;
            if (s->pvy < 0) s->pvy = 0;
            s->on_ground = 1;
        } else {
            s->on_ground = 0;
        }
        {
            fix16_t hw = s->morphing ? MORPH_HALF_W : PLAYER_HALF_W;
            if (s->px < hw) { s->px = hw; if (s->pvx < 0) s->pvx = 0; }
            if (s->px > s->world_w - hw) { s->px = s->world_w - hw; if (s->pvx > 0) s->pvx = 0; }
        }

        /* Waffenwechsel (Edge vom Caller) */
        if (input & INPUT_WEAPSW) {
            s->weapon = (uint8_t)((s->weapon + 1) % WEAPON_COUNT);
            s->fire_cooldown = 4;
        }
        if (s->fire_cooldown > 0) s->fire_cooldown--;

        /* Surround-Beam (Sekundaer) */
        s->beam_active = (input & INPUT_SECOND) ? 1 : 0;
        if (s->beam_active) {
            int d8;
            int i;
            s->pvx = 0; /* Haltestrahl friert vx ein */
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
                        e->hp = (e->hp > 1) ? (uint16_t)(e->hp - 1) : 0;
                    }
                }
            }
        }

        /* Primr-feuer */
        if ((input & INPUT_FIRE) && s->fire_cooldown == 0) {
            player_fire(s);
        }
    }

    /* --- Schild-Regeneration --- */
    if (s->tick % SHIELD_REGEN_TICK == 0 && s->shield < 100) s->shield++;

    /* --- Entities --- */
    proj_update(s);
    enemy_update(s);
    part_update(s);
    xp_update(s);
}
