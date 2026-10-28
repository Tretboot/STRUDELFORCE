/* STRUDELFORCE MASTER - Level Generator Configuration */
#ifndef LEVEL_CONFIG_H
#define LEVEL_CONFIG_H

#include <stdint.h>

/* Biome IDs */
#define BIOME_CHROME_TEMPEST 0
#define BIOME_GREAT_BATH 1
#define BIOME_MECH_HIVE 2
#define BIOME_VOID_REACH 3
#define BIOME_CORE_FURNACE 4
#define BIOME_DESSERT_WASTES 5
#define BIOME_FOREST_RUINS 6
#define BIOME_ICE_CAVES 7

/* Chunk Size */
#define CHUNK_WIDTH 128
#define CHUNK_HEIGHT 30

/* Tile Types */
#define TILE_EMPTY 0
#define TILE_FLOOR 1
#define TILE_WALL 2
#define TILE_LAVA 3
#define TILE_WATER 4
#define TILE_SPIKE 5
#define TILE_PLATFORM 6
#define TILE_ENEMY 7
#define TILE_BOSS 8
#define TILE_EXIT 9
#define TILE_LOOT 10
#define TILE_CHECKPOINT 11

/* Perlin Noise Configuration */
#define PERLIN_OCTAVES 4
#define PERLIN_BASE_FREQ 0.01
#define PERLIN_AMPLITUDE 1.0

/* Level Generation Parameters */
typedef struct {
    uint8_t biome_id;
    uint8_t difficulty;
    uint32_t seed;
    uint8_t chunk_count;
} LevelParams;

/* Function to generate level chunk */
void generate_chunk(uint8_t* chunk_data, LevelParams params, uint8_t chunk_index);

/* Function to get biome-specific parameters */
uint8_t get_biome_difficulty(uint8_t biome_id);
uint32_t get_biome_seed(uint8_t biome_id, uint32_t base_seed);

#endif /* LEVEL_CONFIG_H */
