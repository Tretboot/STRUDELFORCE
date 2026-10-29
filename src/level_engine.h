/*
 * level_engine.h - STRUDELFORCE Level Engine
 *
 * Dockt die generierte level_data.h (LvChunk/LvLevel, ASCII-Chunks) an die
 * Runtime (GameState) an. Null malloc, statischer Tile-Puffer, C89.
 *
 * Lizenz: MIT (Clean Room)
 */
#ifndef LEVEL_ENGINE_H
#define LEVEL_ENGINE_H

#include "strudelforce.h"

/* Laedt Level idx (0..level_count()-1) in den GameState:
 *   - Tile-Grid -> LevelPlatform[] (fix16, y-up, 1 Tile = 1 World-Unit)
 *   - s->world_w = n_chunks * 16 Units
 *   - Spieler-Spawn ('P', auf dem Boden unter dem Tile)
 * Ruegt die Plattform-Liste, faengt Gegner NICHT an.
 * Gibt 1 zurueck bei Erfolg, 0 bei ungueltigem Index. */
int level_load(GameState *s, uint16_t level_idx);

/* Spawnt die Chunk-Gegner (w=walker, t/f=shooter, G=tank mini-boss)
 * in den Enemy-Pool. Gibt die Anzahl zurueck. */
int level_spawn_enemies(GameState *s, uint16_t level_idx);

/* Kampagne-Hook: Level = campaign_level-1 (geclampt), Plattformen + Gegner. */
int level_load_campaign(GameState *s);

/* Iteration 9: Endlos-Wellen (Rogue-Assembler gedockt).
 * Laedt die Chunk-Sequenz fuer die Wellen-Nummer (1-basiert) und
 * spawnt die Chunk-Gegner + Wellen-Bonus-Gegner. Gibt 1 zurueck. */
int level_load_endless_wave(GameState *s);

uint16_t level_count(void);
uint16_t level_width_units(uint16_t level_idx);

/* v2: Musik-Zonen-Query (X-Bereiche in Tiles).
 * 1 = Zone aktiv (subsong_out gesetzt), 0 = keine Zone. */
int level_music_zone(uint16_t level_idx, int x, uint8_t *subsong_out, uint8_t *active_out);

#endif /* LEVEL_ENGINE_H */
