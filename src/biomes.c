#include "sf_interfaces.h"
/*
 * STRUDELFORCE — AGENT 4: Biome-Definitionen + deterministischer Chunk-Generator.
 */

const SFBiomeDef sf_biomedefs[SF_BIOME_COUNT] = {
    [SF_BIOME_CHROME_TEMPEST] = {
        .gravity = sf_fix(180),
        .jump_vel = sf_fix(-560),
        .palette_id = 16,
        .underwater = 0,
        .chest_x = 0, .chest_y = 0,
        .music_file = "user_music/sector1_storm.strudel",
    },
    [SF_BIOME_GREAT_BATH] = {
        .gravity = sf_fix(120),    /* gedämpfte Unterwasser-Gravitation */
        .jump_vel = sf_fix(-700),
        .palette_id = 14,
        .underwater = 1,           /* -> Amiga $BFE001 HW-Tiefpass (AGENT 5) */
        .chest_x = sf_fix(1100), .chest_y = SF_GROUND_Y - sf_fix(16),
        .music_file = "user_music/the_great_bath_chest.strudel",
    },
    [SF_BIOME_MECH_HIVE] = {
        .gravity = sf_fix(200),
        .jump_vel = sf_fix(-600),
        .palette_id = 4,
        .underwater = 0,
        .chest_x = 0, .chest_y = 0,
        .music_file = "user_music/sector1_storm.strudel",
    },
};

int biome_next_chunk(uint32_t *seed, int16_t *biome_out, int16_t *chest_open_out)
{
    int16_t next_biome = (int16_t)sf_rand_range(seed, 0, SF_BIOME_COUNT - 1);
    int16_t chest = (next_biome == SF_BIOME_GREAT_BATH) ? 1 : 0;
    *biome_out = next_biome;
    *chest_open_out = chest;
    return 1;
}
