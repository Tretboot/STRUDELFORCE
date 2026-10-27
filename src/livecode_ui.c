/*
 * livecode_ui.c - STRUDELFORCE ESC-Live-Coding-Console (Meilenstein 4)
 *
 * AGENT_4-Spec: editierbarer Strudel-Pattern-Puffer, nahtloses Live-Compile
 * (Pattern-Tausch erst beim naechsten Zyklus-Schlag), F2/F3 Save/Load
 * (user_music/*.strudel), Great-Bath-Schatztruhe mit Tribute-Pattern.
 *
 * Strikte Regeln: kein malloc im Hot-Path (statische Puffer), 50Hz-Tick.
 * Lizenz: MIT (Clean Room)
 */
#define _CRT_SECURE_NO_WARNINGS
#include "livecode_ui.h"
#include <string.h>

#define BATH_PATTERN "<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"

void livecode_init(LiveCodeUI *ui, const char *default_pattern) {
    memset(ui, 0, sizeof(LiveCodeUI));
    strncpy(ui->buffer, default_pattern, LIVECODE_MAX_LEN);
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
            /* Nahtloser Tausch: erst am Zyklus-Rand (kein Audio-Aussetzer) */
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

void livecode_open_chest(LiveCodeUI *ui) {
    if (ui->chest_opened) return;
    ui->chest_opened = 1;
    ui->biome = 1; /* The Great Bath (Unterwasser-Kathedrale) */
    strncpy(ui->buffer, BATH_PATTERN, LIVECODE_MAX_LEN);
    ui->buffer[LIVECODE_MAX_LEN] = 0;
    ui->cursor = (int)strlen(ui->buffer);
    strudel_compile(&ui->active, ui->buffer, 0);
    ui->cycle_num = 0;
    ui->step_in_cycle = 0;
    ui->have_pending = 0;
}

const char *livecode_biome_name(int biome) {
    switch (biome) {
        case 1: return "The Great Bath";
        case 2: return "Mechanical Hive";
        default: return "Chrome Tempest";
    }
}

