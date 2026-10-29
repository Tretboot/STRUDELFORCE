/*
 * ============================================================================
 *  STRUDELFORCE // src/core_sim.c  (AGENT 1 — Mathe & C89/C99 68k-System)
 * ----------------------------------------------------------------------------
 *  Deterministische 50Hz-Gameplay-Simulation: Spieler-Physik, 6-Waffen-System
 *  (Turrican-2-Laser, Bounce 2.0, Surround-Beam, Morph, Power-Line), 16-Slot
 *  Gegner-Pool mit Voxel-Shatter, AABB-Kollision, XP/Level-Up-Buffs, Score,
 *  Great-Bath-Schatztruhe. Reines Logikmodul: kein I/O, kein Rendering, keine
 *  Allozierung — passt in 512 KB Chip-RAM (statische Pools in GameState).
 *
 *  NUMERIK: 16.16 fix16, 1.0 == 1<<16. Positionen px, Geschwindigkeiten px/s;
 *  ein Tick integriert mit SF_TICK_FRAC = round(65536/50) = 1311.
 *  Player/Enemy y = FUSS-Linie; Bullet/Particle y = Top-Left.
 *  Winkel: Oktant 0..7 (0=+x, clockwise, y-down):
 *      vx = speed * COS8[oct]  (KOS8[0]=1.0: rein +x)
 *      vy = speed * SIN8[oct]  (SIN8[2]=1.0: oct 2 = waerecht nach UNTEN)
 *  DETERMINISMUS: alles ueber sf_rand_range(xorshift32).
 *
 *  Phase-1.1 (Code-Review-Runde, dokumentiert):
 *   - SIN8/COS8 im Velocity-Mapping vertauscht BEHOBEN (Zielrichtung war
 *     an Diagonalen gespiegelt).
 *   - Bounce Stufe 3: {0,45,90,270} = Idx {0,1,2,6} (war {0,1,2,4}).
 *   - wpn_lvl folgt dem Spieler-Level (max 5) — Stufenaufstieg bei Level-Up.
 *   - Score/XP sind int32 im Vertrag (kein int16-Wrap nach ~219 Kills).
 * ========================================================================== */
#include "sf_interfaces.h"
#include "amiga_crt.h"  /* M68K: sf_memset (kein string.h unter -nostdlib) */
#ifndef M68K
#include <string.h>
#endif

/* 1. TUNING */
#define SF_TICK_FRAC      1311      /* 16.16: 1/50 s */
#define SF_PLAYER_W       16
#define SF_PLAYER_H       16
#define SF_PLAYER_MORPH_W 8
#define SF_PLAYER_MORPH_H 8
#define SF_WALK_SPEED     sf_fix(160)
#define SF_WALK_LERP_K    8000
#define SF_GRAVITY_PER_TK sf_fix(180)
#define SF_JUMP_VEL       sf_fix(-560)
#define SF_X_MIN          sf_fix(32)
#define SF_X_MAX          sf_fix(1248)
#define SF_INVULN_TICKS   30
#define SF_MAX_LEVEL      8
#define SF_MAX_WPN_LVL    5
#define SF_SPAWN_CD_INIT  150
#define SF_SPAWN_CD_BASE  90
#define SF_SPAWN_CD_SPAN  90
#define SF_SPEED_MULTIPLE sf_fix(400)
#define SF_SPEED_LASER    sf_fix(480)
#define SF_SPEED_BOUNCE   sf_fix(320)
#define SF_SPEED_SURROUND sf_fix(240)
#define SF_SPEED_DRONE    sf_fix(400)
#define SF_SPEED_ENEMY    sf_fix(200)
#define SF_CD_MULTIPLE    8
#define SF_CD_LASER       14
#define SF_CD_BOUNCE      10
#define SF_CD_SURROUND    2
#define SF_CD_POWER_LINE  30
#define SF_WPN_ENEMY      255

/* 2. WAFFEN-TABELLEN (extern in sf_interfaces.h) */
const uint8_t sf_laser_widths[5]  = { 2, 4, 8, 14, 22 };
const uint8_t sf_spread_counts[5] = { 1, 2, 3, 5, 7 };

/* 3. LOOKUP-TABLES (int32: +-65536 ueberlaeuft int16)
 *    Oktant 0..7 = 0,45,90,...,315 Grad clockwise, y-down.
 *    SIN8 = Sinus (y-Komponente), COS8 = Kosinus (x-Komponente). */
static const int32_t SF_SIN8[8] = {      0,  46341,  65536,  46341,     0, -46341, -65536, -46341 };
static const int32_t SF_COS8[8] = {  65536,  46341,      0, -46341, -65536, -46341,      0,  46341 };

static const int32_t SF_SINE128[128] = {
      0,   3216,   6424,   9616,  12785,  15924,  19024,  22078,
  25080,  28020,  30893,  33692,  36410,  39040,  41576,  44011,
  46341,  48559,  50660,  52639,  54491,  56212,  57798,  59244,
  60547,  61705,  62714,  63572,  64277,  64827,  65220,  65457,
  65536,  65457,  65220,  64827,  64277,  63572,  62714,  61705,
  60547,  59244,  57798,  56212,  54491,  52639,  50660,  48559,
  46341,  44011,  41576,  39040,  36410,  33692,  30893,  28020,
  25080,  22078,  19024,  15924,  12785,   9616,   6424,   3216,
      0,  -3216,  -6424,  -9616, -12785, -15924, -19024, -22078,
 -25080, -28020, -30893, -33692, -36410, -39040, -41576, -44011,
 -46341, -48559, -50660, -52639, -54491, -56212, -57798, -59244,
 -60547, -61705, -62714, -63572, -64277, -64827, -65220, -65457,
 -65536, -65457, -65220, -64827, -64277, -63572, -62714, -61705,
 -60547, -59244, -57798, -56212, -54491, -52639, -50660, -48559,
 -46341, -44011, -41576, -39040, -36410, -33692, -30893, -28020,
 -25080, -22078, -19024, -15924, -12785,  -9616,  -6424,  -3216
};

/* Bounce 2.0 (Geometric Prism): Richtungs-Offsets je Stufe (Oktant-Index).
 * Stufe 1: 0° | Stufe 2: 0°+45° | Stufe 3: +90°/270° (Idx 2/6) |
 * Stufe 4: +135°/225° | Stufe 5: alle 8 + Smart-Ricochet an der Wand. */
static const uint8_t SF_BOUNCE_DIRS[5][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 2, 6, 0, 0, 0, 0 },
    { 0, 1, 2, 3, 5, 6, 0, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7 }
};
static const uint8_t SF_BOUNCE_COUNT[5] = { 1, 2, 4, 6, 8 };

/* 4. MODULE-STATE (Kanten-Erkennung, Cooldowns, Spawner) */
static uint16_t sf_prev_in;
static uint16_t sf_spawn_cd;
static uint16_t sf_invuln;
static uint8_t  sf_fire_cd;

/* 5. DETERMINISTISCHER RNG (xorshift32, [lo..hi] inkl.) */
int32_t sf_rand_range(uint32_t *seed, int32_t lo, int32_t hi)
{
    uint32_t x = *seed;
    uint32_t span;
    if (x == 0u) x = 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *seed = x;
    span = (uint32_t)(hi - lo) + 1u;
    return (int32_t)(lo + (int32_t)(x % span));
}

/* 6. GEOMETRIE-HILFEN */
static uint8_t sf_octant_from_delta(int32_t dx, int32_t dy)
{
    int32_t adx = (dx < 0) ? -dx : dx;
    int32_t ady = (dy < 0) ? -dy : dy;
    if (adx >= ady) {
        if (adx >= 2 * ady) return (uint8_t)(dx >= 0 ? 0 : 4);
        return (uint8_t)(dx >= 0 ? (dy >= 0 ? 1 : 7) : (dy >= 0 ? 3 : 5));
    }
    if (ady >= 2 * adx) return (uint8_t)(dy >= 0 ? 2 : 6);
    return (uint8_t)(dy >= 0 ? (dx >= 0 ? 1 : 3) : (dx >= 0 ? 7 : 5));
}

static int sf_aabb(int32_t ax, int32_t ay, int32_t aw, int32_t ah,
                   int32_t bx, int32_t by, int32_t bw, int32_t bh)
{
    return (ax < bx + bw) && (bx < ax + aw) &&
           (ay < by + bh) && (by < ay + ah);
}

/* 7. POOL-ALLOKATION (erstes freies Slot, voll -> verwerfen) */
static void sf_spawn_bullet(GameState *s, uint8_t etype, uint8_t wpn,
                            fix16_t x, fix16_t y, fix16_t vx, fix16_t vy,
                            fix16_t w, fix16_t h, int16_t dmg,
                            uint16_t ttl, uint8_t bounces)
{
    int i;
    for (i = 0; i < SF_MAX_BULLETS; ++i) {
        SFEntity *b = &s->bullets[i];
        if (b->active) continue;
        b->active  = 1;
        b->etype   = etype;
        b->wpn     = wpn;
        b->lvl     = 0;
        b->x = x; b->y = y; b->vx = vx; b->vy = vy;
        b->w = w; b->h = h;
        b->dmg     = dmg;
        b->ttl     = ttl;
        b->bounces = bounces;
        b->pal     = 1;
        return;
    }
}

static void sf_spawn_particle(GameState *s, uint8_t etype,
                              fix16_t x, fix16_t y, fix16_t vx, fix16_t vy,
                              fix16_t w, fix16_t h, uint16_t ttl,
                              uint8_t pal, int16_t xp)
{
    int i;
    for (i = 0; i < SF_MAX_PARTICLES; ++i) {
        SFEntity *c = &s->particles[i];
        if (c->active) continue;
        c->active  = 1;
        c->etype   = etype;
        c->x = x; c->y = y; c->vx = vx; c->vy = vy;
        c->w = w; c->h = h;
        c->ttl = ttl;
        c->pal = pal;
        c->dmg = xp;   /* Kristall: XP-Wert im dmg-Feld transportiert */
        return;
    }
}

/* 8. SPIELER-SCHADEN / GAME OVER */
static void sf_damage_player(GameState *s, int16_t dmg)
{
    SFPlayer *p = &s->player;
    int i;
    p->shield -= dmg;
    if (p->shield < 0) {
        p->hp += p->shield;
        p->shield = 0;
    }
    if (p->hp <= 0) {
        p->hp = 0;
        s->game_over = 1;
        for (i = 0; i < SF_MAX_BULLETS;   ++i) s->bullets[i].active   = 0;
        for (i = 0; i < SF_MAX_ENEMIES;   ++i) s->enemies[i].active   = 0;
        for (i = 0; i < SF_MAX_PARTICLES; ++i) s->particles[i].active = 0;
        for (i = 0; i < SF_MAX_CRYSTALS;  ++i) s->crystals[i].active  = 0;
    }
}

/* 9. GEGNER-TOD (Voxel-Shatter + Kristall) / SPAWN */
static void sf_kill_enemy(GameState *s, int idx)
{
    SFEnemy *e = &s->enemies[idx];
    static const int32_t SF_KILL_SCORE[3] = { 50, 75, 150 };
    int32_t n;
    int i;

    s->score += SF_KILL_SCORE[e->kind];   /* int32-Akkumulation (Review) */
    n = (e->shatter == 0) ? 8 : (int32_t)e->shatter;
    for (i = 0; i < n; ++i) {
        sf_spawn_particle(s, SF_ETYPE_DEBRIS,
            e->x + sf_fix(sf_rand_range(&s->seed, 0, sf_unfix(e->w))),
            e->y - sf_fix(sf_rand_range(&s->seed, 0, sf_unfix(e->h))),
            sf_fix(sf_rand_range(&s->seed, -200, 200)),
            sf_fix(sf_rand_range(&s->seed, -300, 100)),
            sf_fix(4), sf_fix(4), 40,
            (uint8_t)(e->kind * 10 + 3), 0);
    }
    sf_spawn_particle(s, SF_ETYPE_CRYSTAL,
        e->x + sf_fix((sf_unfix(e->w) - 8) / 2), e->y - sf_fix(8),
        0, 0, sf_fix(8), sf_fix(8), 600, 20, 10);
    e->active = 0;
}

static void sf_spawn_enemy(GameState *s)
{
    int i;
    for (i = 0; i < SF_MAX_ENEMIES; ++i) {
        SFEnemy *e = &s->enemies[i];
        uint8_t kind;
        int32_t dir;
        if (e->active) continue;

        kind = (uint8_t)sf_rand_range(&s->seed, 0, 2);
        dir  = (s->player.x < sf_fix(1264)) ? -1 : 1;

        e->active  = 1;
        e->kind    = kind;
        e->x       = sf_fix(1264);
        e->vy      = 0;
        e->base_y  = 0;
        e->fire_cd = 0;
        switch (kind) {
        case 0: /* grunt */
            e->w = sf_fix(16); e->h = sf_fix(16); e->hp = 20; e->shatter = 0;
            e->y  = SF_GROUND_Y;
            e->vx = (fix16_t)(sf_fix(40) * dir);
            e->fire_cd = (uint16_t)(100 + sf_rand_range(&s->seed, 0, 50));
            break;
        case 1: /* flyer */
            e->w = sf_fix(12); e->h = sf_fix(12); e->hp = 12; e->shatter = 0;
            e->y       = sf_fix(320);
            e->base_y  = sf_fix(320);
            e->vx      = (fix16_t)(sf_fix(60) * dir);
            break;
        default: /* voxel golem (zerfaellt in 32 Einzel-Voxel) */
            e->w = sf_fix(24); e->h = sf_fix(24); e->hp = 50; e->shatter = 32;
            e->y  = SF_GROUND_Y;
            e->vx = (fix16_t)(sf_fix(20) * dir);
            break;
        }
        return;
    }
}

/* 10. SPIELER-UPDATE (Physik + passive Buffs) */
static void sf_update_player(GameState *s, uint16_t in, uint16_t edge)
{
    SFPlayer *p = &s->player;
    int32_t target = 0;

    if (in & SF_INPUT_LEFT)  { p->facing = 0; target = -SF_WALK_SPEED; }
    if (in & SF_INPUT_RIGHT) { p->facing = 1; target =  SF_WALK_SPEED; }
    if (p->weapon == SF_WPN_SURROUND && (in & SF_INPUT_SECONDARY)) {
        p->vx = 0; /* Spec: Surround-Beam friert vx ein */
    } else {
        p->vx = sf_fix_lerp(p->vx, (fix16_t)target, SF_WALK_LERP_K);
    }

    if ((edge & SF_INPUT_JUMP) && p->on_ground) p->vy = SF_JUMP_VEL;
    p->vy += SF_GRAVITY_PER_TK;

    p->x += sf_fix_mul(p->vx, SF_TICK_FRAC);
    p->y += sf_fix_mul(p->vy, SF_TICK_FRAC);

    if (p->x < SF_X_MIN) p->x = SF_X_MIN;
    if (p->x > SF_X_MAX) p->x = SF_X_MAX;
    if (p->y < 0) { p->y = 0; if (p->vy < 0) p->vy = 0; }
    if (p->y > SF_GROUND_Y) { p->y = SF_GROUND_Y; p->vy = 0; }
    p->on_ground = (p->y >= SF_GROUND_Y) ? 1 : 0;

    if (p->buff_shield_res && (s->tick % 30u) == 0u && p->shield < 100) {
        p->shield++;
    }
    if (p->buff_drone && (s->tick % 10u) == 0u) {
        int32_t pw = p->morph ? SF_PLAYER_MORPH_W : SF_PLAYER_W;
        int32_t ph = p->morph ? SF_PLAYER_MORPH_H : SF_PLAYER_H;
        fix16_t dx = p->facing ? p->x + sf_fix(pw) : p->x - sf_fix(6);
        fix16_t dy = p->y - sf_fix(ph / 2) - sf_fix(3);
        sf_spawn_bullet(s, SF_ETYPE_BULLET, SF_WPN_MULTIPLE,
            dx, dy,
            sf_fix_mul(SF_SPEED_DRONE, p->facing ? 65536 : -65536), 0,
            sf_fix(6), sf_fix(6), 6, 60, 0);
    }
}

/* Bounce 2.0 Stufe 5: Smart-Ricochet Richtung naechster aktiver Gegner. */
static void sf_smart_ricochet(GameState *s, SFEntity *b)
{
    int32_t bx = sf_unfix(b->x);
    int32_t by = sf_unfix(b->y);
    int best = -1;
    int32_t best_d2 = 0;
    int i;

    for (i = 0; i < SF_MAX_ENEMIES; ++i) {
        SFEnemy *e = &s->enemies[i];
        int32_t dx, dy, d2;
        if (!e->active) continue;
        dx = sf_unfix(e->x) + sf_unfix(e->w) / 2 - bx;
        dy = (sf_unfix(e->y) - sf_unfix(e->h) / 2) - by;
        d2 = dx * dx + dy * dy;
        if (best < 0 || d2 < best_d2) { best = i; best_d2 = d2; }
    }
    if (best < 0) { b->vx = -b->vx; return; }
    {
        SFEnemy *e = &s->enemies[best];
        int32_t dx = sf_unfix(e->x) + sf_unfix(e->w) / 2 - bx;
        int32_t dy = (sf_unfix(e->y) - sf_unfix(e->h) / 2) - by;
        uint8_t oct = sf_octant_from_delta(dx, dy);
        b->vx = sf_fix_mul(SF_SPEED_BOUNCE, SF_COS8[oct]);
        b->vy = sf_fix_mul(SF_SPEED_BOUNCE, SF_SIN8[oct]);
    }
}

/* 11. WAFFEN-SYSTEM (vx=COS8, vy=SIN8 — Review-Fix) */
static void sf_weapon_fire(GameState *s, uint16_t in, uint16_t edge)
{
    SFPlayer *p = &s->player;
    int32_t pw  = p->morph ? SF_PLAYER_MORPH_W : SF_PLAYER_W;
    int32_t ph  = p->morph ? SF_PLAYER_MORPH_H : SF_PLAYER_H;
    int32_t pcx = sf_unfix(p->x) + pw / 2;
    int32_t pcy = sf_unfix(p->y) - ph / 2;
    int32_t base = p->facing ? 0 : 4;

    if (edge & SF_INPUT_WPN_NEXT) {
        p->weapon = (uint8_t)((p->weapon + 1) % SF_WPN_COUNT);
        sf_fire_cd = 0;
    }
    if (sf_fire_cd > 0) sf_fire_cd--;

    switch (p->weapon) {
    case SF_WPN_MULTIPLE: {
        uint8_t n;
        int16_t lo, hi, off;
        if ((in & SF_INPUT_FIRE) && sf_fire_cd == 0) {
            n  = sf_spread_counts[p->wpn_lvl - 1];
            lo = (int16_t)(-(int16_t)(n / 2));
            hi = (int16_t)((n - 1) / 2);
            for (off = lo; off <= hi; ++off) {
                uint8_t idx = (uint8_t)((base + off) & 7);
                sf_spawn_bullet(s, SF_ETYPE_BULLET, SF_WPN_MULTIPLE,
                    sf_fix(pcx - 3), sf_fix(pcy - 3),
                    sf_fix_mul(SF_SPEED_MULTIPLE, SF_COS8[idx]),
                    sf_fix_mul(SF_SPEED_MULTIPLE, SF_SIN8[idx]),
                    sf_fix(6), sf_fix(6), 10, 60, 0);
            }
            sf_fire_cd = SF_CD_MULTIPLE;
        }
        break;
    }
    case SF_WPN_LASER: {
        fix16_t fw;
        int32_t h;
        fix16_t lx;
        if ((in & SF_INPUT_FIRE) && sf_fire_cd == 0) {
            fw = sf_fix((int32_t)sf_laser_widths[p->wpn_lvl - 1]);
            if (p->buff_laser_w) fw = sf_fix_mul(fw, 78643); /* 1.2 in 16.16 */
            h  = 24 + 12 * (p->wpn_lvl - 1);
            lx = p->facing ? p->x + sf_fix(pw) : p->x - fw;
            sf_spawn_bullet(s, SF_ETYPE_LASER, SF_WPN_LASER,
                lx, p->y - sf_fix(ph / 2) - sf_fix(h / 2),
                sf_fix_mul(SF_SPEED_LASER, p->facing ? 65536 : -65536), 0,
                fw, sf_fix(h), (int16_t)(5 + p->wpn_lvl * 5), 4,
                (p->wpn_lvl >= 3) ? 255 : 0);
            sf_fire_cd = SF_CD_LASER;
        }
        break;
    }
    case SF_WPN_BOUNCE: {
        uint8_t lvl = p->wpn_lvl;
        uint8_t n   = SF_BOUNCE_COUNT[lvl - 1];
        uint8_t i;
        if ((in & SF_INPUT_FIRE) && sf_fire_cd == 0) {
            for (i = 0; i < n; ++i) {
                uint8_t idx = (uint8_t)((base + SF_BOUNCE_DIRS[lvl - 1][i]) & 7);
                sf_spawn_bullet(s, SF_ETYPE_BULLET, SF_WPN_BOUNCE,
                    sf_fix(pcx - 3), sf_fix(pcy - 3),
                    sf_fix_mul(SF_SPEED_BOUNCE, SF_COS8[idx]),
                    sf_fix_mul(SF_SPEED_BOUNCE, SF_SIN8[idx]),
                    sf_fix(6), sf_fix(6), 12, 100, lvl);
            }
            sf_fire_cd = SF_CD_BOUNCE;
        }
        break;
    }
    case SF_WPN_SURROUND: {
        if (in & SF_INPUT_SECONDARY) {
            p->beam_angle = (fix16_t)((p->beam_angle + 16384) & 0xFFFF);
            if (sf_fire_cd == 0) {
                uint8_t idx = (uint8_t)(s->tick & 7);
                sf_spawn_bullet(s, SF_ETYPE_BULLET, SF_WPN_SURROUND,
                    sf_fix(pcx - 1), sf_fix(pcy - 1),
                    sf_fix_mul(SF_SPEED_SURROUND, SF_COS8[idx]),
                    sf_fix_mul(SF_SPEED_SURROUND, SF_SIN8[idx]),
                    sf_fix(3), sf_fix(3), 8, 30, 0);
                sf_fire_cd = SF_CD_SURROUND;
            }
        }
        break;
    }
    case SF_WPN_MORPH:
        if (edge & SF_INPUT_MORPH) p->morph ^= 1;
        break;
    case SF_WPN_POWER_LINE: {
        if ((in & SF_INPUT_FIRE) && sf_fire_cd == 0) {
            sf_spawn_bullet(s, SF_ETYPE_LASER, SF_WPN_POWER_LINE,
                sf_fix(0), p->y - sf_fix(ph / 2) - sf_fix(4),
                0, 0,
                sf_fix(SF_WORLD_W), sf_fix(8), 20, 6, 0);
            sf_fire_cd = SF_CD_POWER_LINE;
        }
        break;
    }
    default:
        break;
    }
}

/* 12. ENTITY-UPDATES */
static void sf_update_bullets(GameState *s)
{
    int i;
    for (i = 0; i < SF_MAX_BULLETS; ++i) {
        SFEntity *b = &s->bullets[i];
        if (!b->active) continue;

        b->ttl--;
        if (b->ttl == 0) { b->active = 0; continue; }

        b->x += sf_fix_mul(b->vx, SF_TICK_FRAC);
        b->y += sf_fix_mul(b->vy, SF_TICK_FRAC);

        if (b->wpn == SF_WPN_BOUNCE && b->bounces > 0) {
            int32_t bx = sf_unfix(b->x);
            int32_t bw = sf_unfix(b->w);
            int hit_left  = (bx <= sf_unfix(SF_X_MIN)) && (b->vx < 0);
            int hit_right = (bx + bw >= sf_unfix(SF_X_MAX)) && (b->vx > 0);
            if (hit_left || hit_right) {
                if (hit_left) b->x = SF_X_MIN;
                else          b->x = SF_X_MAX - sf_fix(bw);
                if (b->bounces == 5) sf_smart_ricochet(s, b);
                else                 b->vx = -b->vx;
                b->bounces--;
            }
        }

        if (sf_unfix(b->x) < -128 || sf_unfix(b->x) > SF_WORLD_W + 128 ||
            sf_unfix(b->y) < -128 || sf_unfix(b->y) > SF_WORLD_H + 128) {
            b->active = 0;
        }
    }
}

static void sf_update_enemies(GameState *s)
{
    int i;
    for (i = 0; i < SF_MAX_ENEMIES; ++i) {
        SFEnemy *e = &s->enemies[i];
        if (!e->active) continue;

        e->x += sf_fix_mul(e->vx, SF_TICK_FRAC);

        if (e->kind == 1) {
            uint32_t idx = ((uint32_t)s->tick * 83443u) >> 16;
            e->y = e->base_y + sf_fix_mul((fix16_t)SF_SINE128[idx & 127u], sf_fix(40));
        } else {
            e->y = SF_GROUND_Y;
        }

        if (e->kind == 0 && e->fire_cd > 0) {
            e->fire_cd--;
            if (e->fire_cd == 0) {
                int32_t dx = sf_unfix(s->player.x) + 8 - (sf_unfix(e->x) + sf_unfix(e->w) / 2);
                int32_t dy = (sf_unfix(s->player.y) - 8) - (sf_unfix(e->y) - sf_unfix(e->h) / 2);
                uint8_t oct = sf_octant_from_delta(dx, dy);
                sf_spawn_bullet(s, SF_ETYPE_BULLET, SF_WPN_ENEMY,
                    e->x + sf_fix(sf_unfix(e->w) / 2 - 2),
                    e->y - sf_fix(sf_unfix(e->h) / 2),
                    sf_fix_mul(SF_SPEED_ENEMY, SF_COS8[oct]),
                    sf_fix_mul(SF_SPEED_ENEMY, SF_SIN8[oct]),
                    sf_fix(4), sf_fix(4), 8, 60, 0);
                e->fire_cd = (uint16_t)(100 + sf_rand_range(&s->seed, 0, 50));
            }
        }

        if (sf_unfix(e->x) < -64 || sf_unfix(e->x) > SF_WORLD_W + 64) {
            e->active = 0;
        }
    }
}

static void sf_update_particles(GameState *s)
{
    int i;
    for (i = 0; i < SF_MAX_PARTICLES; ++i) {
        SFEntity *c = &s->particles[i];
        if (!c->active) continue;
        c->ttl--;
        if (c->ttl == 0) { c->active = 0; continue; }
        c->x += sf_fix_mul(c->vx, SF_TICK_FRAC);
        c->y += sf_fix_mul(c->vy, SF_TICK_FRAC);
    }
}

/* 13. KOLLISION (AABB, Integer-Pixel via sf_unfix) */
static void sf_collisions(GameState *s)
{
    SFPlayer *p = &s->player;
    int32_t pw   = p->morph ? SF_PLAYER_MORPH_W : SF_PLAYER_W;
    int32_t ph   = p->morph ? SF_PLAYER_MORPH_H : SF_PLAYER_H;
    int32_t px   = sf_unfix(p->x);
    int32_t ptop = sf_unfix(p->y) - ph;
    int bi, ei, ci;

    for (bi = 0; bi < SF_MAX_BULLETS; ++bi) {
        SFEntity *b = &s->bullets[bi];
        int32_t bx, by, bw, bh;
        if (!b->active || b->wpn == SF_WPN_ENEMY) continue;
        if (b->etype != SF_ETYPE_BULLET && b->etype != SF_ETYPE_LASER) continue;
        bx = sf_unfix(b->x); by = sf_unfix(b->y);
        bw = sf_unfix(b->w); bh = sf_unfix(b->h);
        for (ei = 0; ei < SF_MAX_ENEMIES; ++ei) {
            SFEnemy *e = &s->enemies[ei];
            if (!e->active) continue;
            if (!sf_aabb(bx, by, bw, bh,
                         sf_unfix(e->x), sf_unfix(e->y) - sf_unfix(e->h),
                         sf_unfix(e->w), sf_unfix(e->h)))
                continue;
            e->hp -= b->dmg;
            if (b->bounces != 255) b->active = 0;
            if (e->hp <= 0) sf_kill_enemy(s, ei);
            if (!b->active) break;
        }
    }

    for (bi = 0; bi < SF_MAX_BULLETS; ++bi) {
        SFEntity *b = &s->bullets[bi];
        if (!b->active || b->etype != SF_ETYPE_BULLET) continue;
        if (b->wpn != SF_WPN_ENEMY) continue;
        if (!sf_aabb(sf_unfix(b->x), sf_unfix(b->y),
                     sf_unfix(b->w), sf_unfix(b->h), px, ptop, pw, ph))
            continue;
        b->active = 0;
        if (p->morph == 1) continue;
        sf_damage_player(s, b->dmg);
    }

    if (p->morph == 0 && sf_invuln == 0) {
        static const int16_t SF_CONTACT_DMG[3] = { 15, 10, 15 };
        for (ei = 0; ei < SF_MAX_ENEMIES; ++ei) {
            SFEnemy *e = &s->enemies[ei];
            if (!e->active) continue;
            if (!sf_aabb(px, ptop, pw, ph,
                         sf_unfix(e->x), sf_unfix(e->y) - sf_unfix(e->h),
                         sf_unfix(e->w), sf_unfix(e->h)))
                continue;
            sf_damage_player(s, SF_CONTACT_DMG[e->kind]);
            sf_invuln = SF_INVULN_TICKS;
            break;
        }
    }

    for (ci = 0; ci < SF_MAX_PARTICLES; ++ci) {
        SFEntity *c = &s->particles[ci];
        if (!c->active || c->etype != SF_ETYPE_CRYSTAL) continue;
        if (!sf_aabb(sf_unfix(c->x), sf_unfix(c->y),
                     sf_unfix(c->w), sf_unfix(c->h), px, ptop, pw, ph))
            continue;
        p->xp += c->dmg;
        s->score += 10;
        c->active = 0;
    }

    for (ci = 0; ci < SF_MAX_CRYSTALS; ++ci) {
        SFEntity *c = &s->crystals[ci];
        if (!c->active) continue;
        if (!sf_aabb(sf_unfix(c->x), sf_unfix(c->y),
                     sf_unfix(c->w), sf_unfix(c->h), px, ptop, pw, ph))
            continue;
        p->xp += c->dmg;
        s->score += 10;
        c->active = 0;
    }

    if (s->biome == SF_BIOME_GREAT_BATH && !s->chest_opened) {
        const SFBiomeDef *bd = &sf_biomedefs[SF_BIOME_GREAT_BATH];
        if (sf_aabb(px, ptop, pw, ph,
                    sf_unfix(bd->chest_x), sf_unfix(bd->chest_y), 16, 16)) {
            s->chest_opened = 1;
            s->score += 500;
            sf_spawn_particle(s, SF_ETYPE_CRYSTAL,
                bd->chest_x, bd->chest_y, 0, 0,
                sf_fix(8), sf_fix(8), 600, 20, 50);
        }
    }
}

/* 14. LEVEL-UP / BUFF-WAHL (Vampire-Survivors-Layer)
 * Phase-1.1: wpn_lvl folgt dem Level (max 5) — Stufenaufstieg. */
static void sf_level_up(GameState *s, uint16_t in, uint16_t edge)
{
    SFPlayer *p = &s->player;
    while (p->level < SF_MAX_LEVEL && p->xp >= (int32_t)(p->level * 50)) {
        p->xp -= (int32_t)(p->level * 50);
        p->level++;
        /* Stufenaufstieg: wpn_lvl folgt dem Level, wird aber bei 5 gekappt
         * (Tabelle 5 Eintraege); das Spieler-Level darf weiter bis 8. */
        p->wpn_lvl = (p->level >= SF_MAX_WPN_LVL) ? SF_MAX_WPN_LVL : p->level;
        s->levelup_pending = 1;
        s->levelup_idx = 0;
    }
    if (!s->levelup_pending) return;

    if (in & SF_INPUT_UP)   s->levelup_idx = 0;
    if (in & SF_INPUT_LEFT) s->levelup_idx = 1;
    if (in & SF_INPUT_DOWN) s->levelup_idx = 2;
    if (edge & SF_INPUT_LEVELUP) {
        switch (s->levelup_idx) {
        case 0:  p->buff_laser_w    = 1; break;
        case 1:  p->buff_shield_res = 1; break;
        case 2:  p->buff_drone      = 1; break;
        default: break;
        }
        s->levelup_pending = 0;
    }
}

/* 15. PUBLIC API */
void core_init(GameState *s, int16_t biome, uint32_t seed)
{
    SFPlayer *p = &s->player;

    memset(s, 0, sizeof(*s));
    sf_prev_in  = 0;
    sf_spawn_cd = SF_SPAWN_CD_INIT;
    sf_invuln   = 0;
    sf_fire_cd  = 0;

    s->biome = biome;
    s->seed  = (seed == 0u) ? 0x9E3779B9u : seed;

    p->hp        = 100;
    p->shield    = 50;
    p->level     = 1;
    p->weapon    = SF_WPN_MULTIPLE;
    p->wpn_lvl   = 1;
    p->x         = sf_fix(64);
    p->y         = SF_GROUND_Y;
    p->facing    = 1;
    p->on_ground = 1;
}

void core_tick(GameState *s, uint16_t input_mask)
{
    uint16_t in;
    uint16_t edge;

    s->tick++;
    if (s->game_over) { sf_prev_in = input_mask; return; }

    in   = input_mask;
    edge = (uint16_t)(in & (uint16_t)~sf_prev_in);
    sf_prev_in = in;

    sf_update_player(s, in, edge);
    sf_weapon_fire(s, in, edge);
    sf_update_bullets(s);
    sf_update_enemies(s);
    sf_update_particles(s);
    sf_collisions(s);
    sf_level_up(s, in, edge);
    if (sf_invuln > 0) sf_invuln--;

    if (!s->game_over && sf_spawn_cd > 0) {
        sf_spawn_cd--;
        if (sf_spawn_cd == 0) {
            sf_spawn_enemy(s);
            sf_spawn_cd = (uint16_t)(SF_SPAWN_CD_BASE +
                                     sf_rand_range(&s->seed, 0, SF_SPAWN_CD_SPAN));
        }
    }
}
