/*
 * voxel_render.c - STRUDELFORCE 2.5D-Voxel-Render-Planer (AGENT 3)
 * Layers (Schreibreihenfolge == Draw-Reihenfolge, hinten -> vorne):
 *   3: z=-20 Copper-Horizont (65 Bänder)
 *   2: z=-6  Ruinen-Säulen  (24, 1/4-Parallax gebacken)
 *   1: z=0   Gameplay       (Player, Gegner, Projektile, Debris, Kristalle, Truhe)
 *   0: z=+2  Vordergrund-Rohre (6)
 * Low-HP: Amiga-Interlace-Jitter auf ungeraden Zeilen von Layer 0/3.
 */
#include "sf_interfaces.h"
#include "voxel_render.h"
#include "amiga_crt.h"
#ifndef M68K
#include <math.h>
#endif

static uint8_t buf_player_colors[16 * 16];
static uint8_t buf_player_depth[16 * 16];
static uint8_t buf_grunt_colors[16 * 16];
static uint8_t buf_grunt_depth[16 * 16];
static uint8_t buf_flyer_colors[16 * 16];
static uint8_t buf_flyer_depth[16 * 16];
static uint8_t buf_golem_colors[24 * 24];
static uint8_t buf_golem_depth[24 * 24];
static uint8_t buf_chest_colors[24 * 24];
static uint8_t buf_chest_depth[24 * 24];
static uint8_t buf_crystal_colors[16 * 16];
static uint8_t buf_crystal_depth[16 * 16];

const SpriteDef sprite_player = { "player", 16, 16, buf_player_colors, buf_player_depth };
const SpriteDef sprite_grunt  = { "grunt",  16, 16, buf_grunt_colors,  buf_grunt_depth  };
const SpriteDef sprite_flyer  = { "flyer",  16, 16, buf_flyer_colors,  buf_flyer_depth  };
const SpriteDef sprite_golem  = { "golem",  24, 24, buf_golem_colors,  buf_golem_depth  };
const SpriteDef sprite_chest  = { "chest",  24, 24, buf_chest_colors,  buf_chest_depth  };
const SpriteDef sprite_crystal= { "crystal",16, 16, buf_crystal_colors,buf_crystal_depth};

static uint8_t sprites_exposed = 0;
static int     exposure_count  = 0;
static uint8_t sprites_built   = 0;

/* Einfache geometrische Formen (Farb-Indizes 0..31), z_depth = 2+(x+y)%3
 * als diagonale "Bevel"-Extrusion; 0 = transparent. */
static void sprites_build(void)
{
    int x, y, i;

    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int c = 0;
            if (y >= 1 && y <= 3 && x >= 5 && x <= 10)       c = 24; /* visor */
            else if (y >= 4 && y <= 12 && x >= 4 && x <= 11) c = 16; /* torso */
            else if (y >= 13 && y <= 15 && x >= 4 && x <= 5) c = 16;
            else if (y >= 13 && y <= 15 && x >= 10 && x <= 11) c = 16;
            i = y * 16 + x;
            buf_player_colors[i] = (uint8_t)c;
            buf_player_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int c = 0;
            if (y >= 2 && y <= 4 && x >= 5 && x <= 10)       c = 8;
            else if (y >= 5 && y <= 13 && x >= 3 && x <= 12) c = 8;
            else if (y >= 14 && y <= 15 && x >= 4 && x <= 5) c = 8;
            else if (y >= 14 && y <= 15 && x >= 10 && x <= 11) c = 8;
            i = y * 16 + x;
            buf_grunt_colors[i] = (uint8_t)c;
            buf_grunt_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int d = (x > 7 ? x - 7 : 7 - x) + (y > 7 ? y - 7 : 7 - y);
            int c = (d <= 6 && d >= 2) ? 10 : 0;
            if (d <= 1) c = 15;
            i = y * 16 + x;
            buf_flyer_colors[i] = (uint8_t)c;
            buf_flyer_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 24; y++) {
        for (x = 0; x < 24; x++) {
            int c = 0;
            if (y >= 4 && y <= 20 && x >= 4 && x <= 19) c = 4;
            if (y >= 6 && y <= 7 && ((x >= 8 && x <= 9) || (x >= 14 && x <= 15))) c = 12;
            if (y >= 10 && y <= 13 && x >= 10 && x <= 13) c = 12;
            i = y * 24 + x;
            buf_golem_colors[i] = (uint8_t)c;
            buf_golem_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 24; y++) {
        for (x = 0; x < 24; x++) {
            int c = 0;
            if (y >= 8 && y <= 20 && x >= 3 && x <= 20) c = 12;
            if (y >= 12 && y <= 14 && x >= 11 && x <= 12) c = 4;
            i = y * 24 + x;
            buf_chest_colors[i] = (uint8_t)c;
            buf_chest_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int d = (x > 7 ? x - 7 : 7 - x) + (y > 7 ? y - 7 : 7 - y);
            int c = (d <= 6) ? 14 : 0;
            if (d <= 1) c = 15;
            i = y * 16 + x;
            buf_crystal_colors[i] = (uint8_t)c;
            buf_crystal_depth[i]  = c ? (uint8_t)(2 + (x + y) % 3) : 0;
        }
    }
    sprites_built = 1;
}

void voxel_expose_sprites(void)
{
    if (!sprites_exposed) {
        static const SpriteDef *defs[6] = {
            &sprite_player, &sprite_grunt, &sprite_flyer,
            &sprite_golem,  &sprite_chest, &sprite_crystal
        };
        int d, p;
        if (!sprites_built) sprites_build();
        exposure_count = 0;
        for (d = 0; d < 6; d++) {
            for (p = 0; p < (int)(defs[d]->w * defs[d]->h); p++)
                exposure_count += (int)defs[d]->z_depth[p];
        }
        sprites_exposed = 1;
    }
}

void voxel_init(void)
{
    if (!sprites_built) sprites_build();
    voxel_expose_sprites();
}

uint16_t voxel_build_render_plan(const GameState *s, VoxelRenderItem *out, uint16_t max_items)
{
    uint16_t n = 0;
    int i;

    if (!sprites_built) sprites_build();

#define ADD_ITEM(ix, iy, iw, ih, iz, ikind, ipal, ilayer)             \
    do {                                                               \
        if (n < max_items) {                                           \
            out[n].x = (ix);  out[n].y = (iy);                         \
            out[n].w = (iw); out[n].h = (ih);                          \
            out[n].z = (iz);                                           \
            out[n].kind = (uint8_t)(ikind);                            \
            out[n].pal = (uint8_t)(ipal);                              \
            out[n].layer = (uint8_t)(ilayer);                          \
            out[n].flags = 0;                                          \
            n++;                                                       \
        }                                                              \
    } while (0)

    /* LAYER 3 (z=-20): 65 Copper-Horizont-Bänder */
    for (i = 0; i < 65; i++) {
        ADD_ITEM(sf_fix(0), sf_fix(i * 11), sf_fix(1280), sf_fix(11),
                 sf_fix(-20), 0, (uint8_t)(i % 8), 3);
    }

    /* LAYER 2 (z=-6): 24 Ruinen-Säulen, 1/4-Parallax in den Plan gebacken */
    {
        int x_offset = sf_unfix(s->player.x) / 4;
        for (i = 0; i < 24; i++) {
            int px = (i * 53 - x_offset) % 1280;
            if (px < 0) px += 1280;
            ADD_ITEM(sf_fix(px), sf_fix(720 - 128), sf_fix(32), sf_fix(128),
                     sf_fix(-6), 0, 6, 2);
        }
    }

    /* LAYER 1 (z=0): Player */
    ADD_ITEM(s->player.x, s->player.y - sf_fix(16),
             sf_fix(16), sf_fix(16), sf_fix(0), 1, 16, 1);

    /* Gegner */
    for (i = 0; i < SF_MAX_ENEMIES; i++) {
        SFEnemy *e = &s->enemies[i];
        int sz;
        if (!e->active) continue;
        sz = (e->kind == 2) ? 24 : 16;
        ADD_ITEM(e->x, e->y - sf_fix(sz), sf_fix(sz), sf_fix(sz),
                 sf_fix(0), 1, (uint8_t)(e->kind * 8 + 4), 1);
    }
    /* Projektile */
    for (i = 0; i < SF_MAX_BULLETS; i++) {
        SFEntity *b = &s->bullets[i];
        if (!b->active) continue;
        ADD_ITEM(b->x, b->y, b->w, b->h, sf_fix(0), 0, b->pal, 1);
    }
    /* Partikel / Debris */
    for (i = 0; i < SF_MAX_PARTICLES; i++) {
        SFEntity *c = &s->particles[i];
        if (!c->active) continue;
        ADD_ITEM(c->x, c->y, sf_fix(1), sf_fix(1), sf_fix(0), 3, c->pal, 1);
    }
    /* Kristalle */
    for (i = 0; i < SF_MAX_CRYSTALS; i++) {
        SFEntity *c = &s->crystals[i];
        if (!c->active) continue;
        ADD_ITEM(c->x, c->y, sf_fix(16), sf_fix(16), sf_fix(0), 1, 20, 1);
    }
    /* Great-Bath-Schatztruhe (noch geschlossen) */
    if (s->biome == SF_BIOME_GREAT_BATH && !s->chest_opened) {
        const SFBiomeDef *bd = &sf_biomedefs[SF_BIOME_GREAT_BATH];
        ADD_ITEM(bd->chest_x, bd->chest_y, sf_fix(24), sf_fix(24),
                 sf_fix(0), 1, 12, 1);
    }

    /* LAYER 0 (z=+2): 6 Vordergrund-Rohre */
    {
        static const int pipe_x[6] = { 0, 213, 426, 639, 852, 1065 };
        for (i = 0; i < 6; i++) {
            ADD_ITEM(sf_fix(pipe_x[i]), sf_fix(0), sf_fix(48), sf_fix(720),
                     sf_fix(2), 0, 2, 0);
        }
    }

    /* LOW-HP INTERLACE: Layer 0/3, ungerade y-Zeilen, ~0.5 Hz Jitter */
    if (s->player.hp < 25) {
        /* Interlace-Jitter: PC = sinf, M68K = deterministischer LUT (kein libm) */
#ifdef M68K
        static const int16_t sf_jit8[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };
        int jitter = sf_jit8[s->tick & 7];
#else
        int jitter = (int)(2.0f * sinf((float)s->tick * 0.126f));
#endif
        for (i = 0; i < (int)n; i++) {
            if ((out[i].layer == 0 || out[i].layer == 3) && out[i].kind != 4) {
                if (sf_unfix(out[i].y) & 1)
                    out[i].x += sf_fix(jitter);
            }
        }
    }

#undef ADD_ITEM
    return n;
}
