/*
 * level_engine.c - STRUDELFORCE Level Engine
 *
 * Waendet die generierte level_data.h (LvChunk/LvLevel) zur Runtime:
 *   - ASCII-Chunks -> flaches Tile-Grid (statisch, kein malloc)
 *   - Tile-Grid -> LevelPlatform[] (fix16, y-up, 1 Tile = 1 World-Unit)
 *   - Chunk-Entities (w/t/f/G) -> Enemy-Pool
 *   - Level-Breite -> s->world_w
 *   - v2: Musik-Zonen-Query (X-Bereiche in Tiles)
 *
 * Regeln:
 *   - Ein Solid-Tile erzeugt nur dann eine Plattform, wenn das Tile DARUEBER
 *     nicht solid ist (Nur-Obenflaeche -> minimale Plattform-Anzahl,
 *     Budget MAX_LEVEL_PLATFORMS = 32).
 *   - Horizontale Laeuufe werden zu einer Plattform (w = Laeufe-Laeenge).
 *   - '-' oneway, '#' 'B' 'M' '?' '=' solid -> kind 0
 *   - '^' spikes und '~' wasser -> kind 2 (visuell; Schaden folgt)
 *   - 'H' ladder: reserviert (noch keine Leiter-Physik)
 *   - 'P' -> Spieler-Spawn (auf dem Boden unter dem Tile)
 *   - 'E' -> Exit (Kampagne ist wave-basiert; Marker fuer Editor/Bot)
 *
 * Lizenz: MIT (Clean Room)
 */
#include "strudelforce.h"
#include "level_data.h"
#include "level_engine.h"

#define LE_MAX_W (LV_MAX_CHUNKS * LV_CHUNK_W)   /* 384 Spalten */
#define LE_H     LV_CHUNK_H                     /* 12 Zeilen  */

static char le_grid[LE_H][LE_MAX_W];
static uint8_t le_w;

static int le_is_solid(char c) {
    return c == '#' || c == 'B' || c == 'M' || c == '?' || c == '=';
}

static int le_is_oneway(char c) {
    return c == '-';
}

static void le_add_plat(GameState *s, int x, int y, int w, uint8_t kind) {
    LevelPlatform *pl;
    if (s->platform_count >= MAX_LEVEL_PLATFORMS) return;
    pl = &s->platforms[s->platform_count++];
    pl->x = fix16_from_int(x);
    pl->y = fix16_from_int(y);
    pl->w = fix16_from_int(w);
    pl->h = FIX(1);
    pl->kind = kind;
}

/* Chunks links-rechts aneinander -> flaches Grid. 1 = ok */
static int le_build_grid(const LvLevel *lv) {
    int ci, r, c;
    if (lv->n_chunks < 1 || lv->n_chunks > LV_MAX_CHUNKS) return 0;
    le_w = (uint8_t)(lv->n_chunks * LV_CHUNK_W);
    for (r = 0; r < LE_H; r++)
        for (c = 0; c < (int)le_w; c++)
            le_grid[r][c] = '.';
    for (ci = 0; ci < (int)lv->n_chunks; ci++) {
        const LvChunk *ch = &LV_CHUNKS[lv->seq[ci]];
        for (r = 0; r < LE_H; r++)
            for (c = 0; c < LV_CHUNK_W; c++)
                le_grid[r][ci * LV_CHUNK_W + c] = ch->rows[r][c];
    }
    return 1;
}

int level_load(GameState *s, uint16_t idx) {
    const LvLevel *lv;
    int r, c, spawned;
    if (idx >= (uint16_t)LV_NUM_LEVELS) return 0;
    lv = &LV_LEVELS[idx];
    if (!le_build_grid(lv)) return 0;

    s->platform_count = 0;
    s->world_w = fix16_from_int(le_w);
    s->world_h = fix16_from_int(WORLD_H_UNITS);

    /* Plattformen: Nur-Obenflaeche-Regel, horizontale Laeuufe gemerged */
    for (r = 0; r < LE_H; r++) {
        c = 0;
        while (c < (int)le_w) {
            char t = le_grid[r][c];
            int above_solid = (r > 0) && le_is_solid(le_grid[r - 1][c]);
            if (le_is_solid(t) && !above_solid) {
                int run = 1;
                while (c + run < (int)le_w &&
                       le_is_solid(le_grid[r][c + run]) &&
                       !(r > 0 && le_is_solid(le_grid[r - 1][c + run])))
                    run++;
                le_add_plat(s, c, LE_H - 1 - r, run, 0);
                c += run;
                continue;
            }
            if (le_is_oneway(t)) {
                int run = 1;
                while (c + run < (int)le_w && le_grid[r][c + run] == '-') run++;
                le_add_plat(s, c, LE_H - 1 - r, run, 0);
                c += run;
                continue;
            }
            if (t == '^' || t == '~') {
                int run = 1;
                while (c + run < (int)le_w && le_grid[r][c + run] == t) run++;
                le_add_plat(s, c, LE_H - 1 - r, run, 2);
                c += run;
                continue;
            }
            c++;
        }
    }

    /* Spieler-Spawn: 'P' (Spaltenzentrum, auf dem Boden unter dem Tile) */
    spawned = 0;
    for (r = 0; r < LE_H && !spawned; r++)
        for (c = 0; c < (int)le_w; c++)
            if (le_grid[r][c] == 'P') {
                int gy = 0, rr;
                for (rr = r + 1; rr < LE_H; rr++)
                    if (le_is_solid(le_grid[rr][c]) || le_is_oneway(le_grid[rr][c])) {
                        gy = LE_H - 1 - rr;
                        break;
                    }
                s->px = fix16_from_int(c + 1);
                s->py = fix16_from_int(gy);
                spawned = 1;
                break;
            }
    if (!spawned) s->px = s->world_w / 2;
    return 1;
}

/* Hoehe der naechsten begehbaeren Oberflaeche unter (c,r), in World-Units */
static int le_ground_below(int c, int r) {
    int rr;
    for (rr = r + 1; rr < LE_H; rr++)
        if (le_is_solid(le_grid[rr][c]) || le_is_oneway(le_grid[rr][c]))
            return LE_H - 1 - rr;
    return 0;
}

int level_spawn_enemies(GameState *s, uint16_t idx) {
    const LvLevel *lv;
    int r, c, n = 0;
    if (idx >= (uint16_t)LV_NUM_LEVELS) return 0;
    lv = &LV_LEVELS[idx];
    if (!le_build_grid(lv)) return 0;
    for (r = 0; r < LE_H; r++) {
        for (c = 0; c < (int)le_w; c++) {
            char t = le_grid[r][c];
            uint8_t kind;
            Enemy *e;
            if (t == 'w') kind = ENEMY_WALKER;
            else if (t == 't' || t == 'f') kind = ENEMY_SHOOTER;
            else if (t == 'G') kind = ENEMY_TANK;
            else continue;
            e = &s->enemies[s->enemy_head];
            s->enemy_head = (uint8_t)((s->enemy_head + 1) % MAX_ENEMIES);
            e->active = 1;
            e->kind = kind;
            e->x = fix16_from_int(c + 1);
            e->y = fix16_from_int(le_ground_below(c, r));
            e->vx = 0;
            e->hp = (kind == ENEMY_TANK) ? 15 : ((kind == ENEMY_SHOOTER) ? 5 : 3);
            e->score_val = (kind == ENEMY_TANK) ? 500 : ((kind == ENEMY_SHOOTER) ? 150 : 100);
            e->xp_val = (kind == ENEMY_TANK) ? 50 : ((kind == ENEMY_SHOOTER) ? 15 : 10);
            e->hit_timer = 0;
            e->shoot_timer = 0;
            n++;
        }
    }
    return n;
}

int level_load_campaign(GameState *s) {
    uint16_t idx;
    if (s->mode != MODE_CAMPAIGN) return 0;
    idx = (uint16_t)(s->campaign_level - 1);
    if (idx >= (uint16_t)LV_NUM_LEVELS) idx = (uint16_t)LV_NUM_LEVELS - 1;
    if (!level_load(s, idx)) return 0;
    level_spawn_enemies(s, idx);
    return 1;
}

uint16_t level_count(void) {
    return (uint16_t)LV_NUM_LEVELS;
}

uint16_t level_width_units(uint16_t idx) {
    if (idx >= (uint16_t)LV_NUM_LEVELS) return 0;
    return (uint16_t)(LV_LEVELS[idx].n_chunks * LV_CHUNK_W);
}

/* v2: Musik-Zonen-Query. x in World-Units/Tiles.
 * 1 = Zone aktiv (subsong_out gesetzt), 0 = keine Zone. */
int level_music_zone(uint16_t idx, int x, uint8_t *subsong_out, uint8_t *active_out) {
    int i;
    const LvMusicZone *z;
    uint8_t n;
    if (idx >= (uint16_t)LV_NUM_LEVELS) return 0;
    z = LV_MZONES[idx];
    n = LV_MZONE_COUNT[idx];
    for (i = 0; i < (int)n; i++) {
        if (x >= (int)z[i].x0 && x < (int)z[i].x1) {
            if (active_out) *active_out = 1;
            if (subsong_out) *subsong_out = z[i].subsong;
            return 1;
        }
    }
    if (active_out) *active_out = 0;
    return 0;
}
