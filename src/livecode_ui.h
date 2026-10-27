/*
 * livecode_ui.h - STRUDELFORCE ESC-Live-Coding-Console (Meilenstein 4)
 * Gerettet aus Smith Primes Iteration-5-Entwurf.
 * Lizenz: MIT (Clean Room)
 */
#ifndef LIVECODE_UI_H
#define LIVECODE_UI_H

#include <stdint.h>
#include <stdio.h>
#include "strudel_cleanroom.h"

#define LIVECODE_MAX_LEN 512
#define LIVECODE_STEPS_PER_CYCLE 16
#define LIVECODE_TICKS_PER_STEP 5   /* 50Hz / 5 = 10 Hz Schritt-Tempo */

typedef struct {
    char buffer[LIVECODE_MAX_LEN + 1];
    int  cursor;
    StrudelPattern active;
    StrudelPattern pending;
    uint8_t have_pending;
    uint32_t tick;
    uint32_t cycle_num;
    uint8_t  step_in_cycle;
    uint8_t  editor_open;
    uint8_t  chest_opened;
    uint8_t  biome;   /* 0 Chrome Tempest, 1 Great Bath, 2 Mechanical Hive */
} LiveCodeUI;

void livecode_init(LiveCodeUI *ui, const char *default_pattern);
void livecode_tick(LiveCodeUI *ui);
int16_t livecode_current_note(const LiveCodeUI *ui);
void livecode_edit_char(LiveCodeUI *ui, char c);
void livecode_edit_backspace(LiveCodeUI *ui);
void livecode_edit_cursor(LiveCodeUI *ui, int dir);
void livecode_compile_pending(LiveCodeUI *ui);
int  livecode_save(LiveCodeUI *ui, const char *path);
int  livecode_load(LiveCodeUI *ui, const char *path);
void livecode_open_chest(LiveCodeUI *ui);
const char *livecode_biome_name(int biome);

#endif /* LIVECODE_UI_H */

