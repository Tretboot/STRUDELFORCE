#!/usr/bin/env python3
"""
STRUDELFORCE_Master Workspace Setup & Multi-Folder Toolkit
Legt alle Analyse-MDs, den geretteten Iteration-5-Code und die Agenten-Tools
direkt im Hauptordner 'STRUDELFORCE_Master' an.
"""

import os
import re
import sys
import hashlib
import zipfile
import base64

WORKSPACE_FILES = {}

# ==============================================================================
# 1. AGENTS.md / README.md (DAS ERSTE, WAS DER VS-CODE-AGENT LIEST)
# ==============================================================================
WORKSPACE_FILES["AGENTS.md"] = """# STRUDELFORCE – Master Workspace & Agent Briefing

Willkommen im Hauptverzeichnis `STRUDELFORCE_Master`. Wenn der User sagt **"Schau dir das hier mal an"**, lies zuerst dieses Dokument, um die Historie, die beiden vorhandenen Code-Bäume und deine Mission zu verstehen.

---

## 1. Was liegt in diesem Verzeichnis? (Die 2 KIWI-Stämme)

Dieses Projekt entstand aus einem A/B-Experiment mit zwei unterschiedlichen Multi-Agenten-Architekturen in einer Cloud-Sandbox ohne C-Compiler:

### Ordner A: `STRUDELFORCE/` (Der "Swarm / Handoff Phase 1"-Stamm)
- **Stärken:** Besitzt bereits alle 5 Architektur-Module als `.c`- und `.h`-Dateien (`core_sim.c`, `cyber_paula.c`, `voxel_render.c`, `livecode_ui.c`, `biomes.c`, `amiga_hw.c`, `main_pc.c`) sowie einen übergreifenden Schnittstellenvertrag (`sf_interfaces.h`, `specs/PHASE1_CONTRACT.md`) und den Gesamttest `src/test_phase1.c`.
- **Wichtiger Bugfix:** Enthält in `core_sim.c` bereits den kritischen Fix für die `SIN8`/`COS8`-Vertauschung (ohne den diagonale Projektile gespiegelt flogen).
- **Schwäche:** Noch kein direkt interaktiv im Terminal spielbares Frontend und kein direkter `.wav`-Audio-Datei-Exporter.

### Ordner B: `STRUDELFORCE_Smith_Core/` (Der "Smith Prime / Meeseeks"-Stamm)
- **Stärken:**
  1. **Sofort spielbares 50Hz-Terminal-Spiel (`src/game_main.c`):** Läuft komplett ohne externe Grafik-Abhängigkeiten interaktiv im Terminal ODER deterministisch per Bot (`--demo 2500`).
  2. **Echter 16-Bit WAV-Audio-Renderer (`src/game_audio.c`):** Rendert über `--demo 2500 --audio out.wav` den reaktiven Soundtrack (8-Bit Paula-Emulation, TFMX-Macros, Biquad-Filter) direkt in eine abspielbare `.wav`-Datei!
  3. **Hervorragende Python-Oracles & Studien-Doku:** `verify_core_sim.py`, `verify_audio_matrix.py`, `WORKFLOW_ESSAY.md` und `captains_log.md`.
- **Was fehlte:** Der Agent wurde mitten in **Iteration 5 (Meilenstein 4: ESC-Live-Coding-Integration)** durch das Output-Token-Limit im Denk-Block abgeschnitten.
- **Die Rettung:** Der komplette Code, den Smith Prime für Iteration 5 im Kopf bereits fertig geschrieben hatte, liegt fertig extrahiert im Ordner **`rescued_iter5/`**!

---

## 2. Eiserne Design- & Architektur-Gesetze (PC Raylib <-> Amiga 68k)

1. **Amiga-First Core (`core_sim.c`):**
   - Strikter C89/C99-Code mit **16.16 Fixed-Point-Arithmetik (`int32_t`)**.
   - **Null `float` oder `double`** und **null `malloc`/`free`** in der Gameplay-Schleife! Alles läuft über statische Ringbuffer in einem festen **50Hz PAL-Tick**.
2. **Kein Minecraft-Look bei den Voxeln (`voxel_render.c`):**
   - **Keine Texturen auf Voxeln!** Wir nutzen reine Amiga-Farbpaletten-Indizes (16/32 Farben pro Biom).
   - Plastizität entsteht ausschließlich durch **direktionales Face-Shading / Ambient Occlusion** (abgedunkelte Seiten-/Unterkanten der Voxel) und **2.5D Tiefen-Extrusion** aus 2D-Sprite-Matrizen.
   - 4 gestaffelte **3D-Parallax-Ebenen** + klassische **Amiga-Copper-Farbverläufe** im Hintergrund und Zeilen-Interlace-Jitter bei niedrigem Leben (`HP < 25%`).
   - Alles, was wir auf dem PC mit Raylib darstellen, muss sich stilistisch 1:1 auf Amiga-Bitplanes + Copper-Listen herunterbrechen lassen.
3. **Cyber-Paula Sound-Engine (`cyber_paula.c` / `game_audio.c`):**
   - Kombiniert den **Clean-Room Strudel-Parser (`strudel_cleanroom.h`)** mit **50Hz TFMX-Sound-Macros (`tfmx_macro_synth.h`)** und `.MOD`-Support.
   - **Hyper-Reaktiv:** Waffenwechsel tauscht das Bass-Macro (Laser = PWM-Saw, Bounce 2.0 = FM-Pluck + 50Hz Arpeggio), Schild-Aufladung dreht die Filter-Resonanz hoch, niedrige HP schließen den Low-Pass-Filter, und die Schatztruhe im Unterwasser-Biom schaltet auf das *Great Bath*-Tribute-Pattern um.

---

## 3. Deine Mission in VS Code (Schritt-für-Schritt)

Wenn du dir das Projekt anschaust, führe folgende Schritte aus:
1. **Überblick & Linter-Check:** Führe `python kiwi_toolkit.py compare` und `python kiwi_toolkit.py lint` aus, um beide Bäume zu prüfen.
2. **Iteration 5 in `STRUDELFORCE_Smith_Core` vollenden:**
   - Kopiere `rescued_iter5/livecode_ui.h`, `rescued_iter5/livecode_ui.c` und `rescued_iter5/verify_livecode.py` nach `STRUDELFORCE_Smith_Core/`.
   - Wende die in `rescued_iter5/ITER5_PATCH_GUIDE.md` dokumentierten Patches auf `STRUDELFORCE_Smith_Core/src/game_audio.h`, `game_audio.c`, `game_main.c` und `Makefile` an.
   - Kompiliere und teste `strudelforce_game` lokal mit `gcc` (inkl. `--demo 2500 --audio test_out.wav` und `python verify_livecode.py`).
3. **Beide Bäume kompilieren & das Beste zusammenführen ("Best-of-Breed"):**
   - Kompiliere auch `STRUDELFORCE/src/test_phase1.c` mit `gcc`.
   - Vereine die Stärken beider Welten: Das spielbare Terminal-/WAV-Frontend + Live-Code-Override aus `STRUDELFORCE_Smith_Core` mit den Voxel-, Biom- und Amiga-Hardware-Modulen (`voxel_render.c`, `biomes.c`, `amiga_hw.c`) sowie der `SIN8`/`COS8`-Korrektur aus `STRUDELFORCE`.
"""

WORKSPACE_FILES["README.md"] = WORKSPACE_FILES["AGENTS.md"]

# ==============================================================================
# 2. GERETTETER CODE AUS SMITH PRIMES LETZTEN GEDANKEN (ITERATION 5)
# ==============================================================================
WORKSPACE_FILES["rescued_iter5/livecode_ui.h"] = """/*
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
"""

WORKSPACE_FILES["rescued_iter5/livecode_ui.c"] = """/*
 * livecode_ui.c - STRUDELFORCE ESC-Live-Coding-Console (Meilenstein 4)
 *
 * AGENT_4-Spec: editierbarer Strudel-Pattern-Puffer, nahtloses Live-Compile
 * (Pattern-Tausch erst beim naechsten Zyklus-Schlag), F2/F3 Save/Load
 * (user_music/*.strudel), Great-Bath-Schatztruhe mit Tribute-Pattern.
 *
 * Strikte Regeln: kein malloc im Hot-Path (statische Puffer), 50Hz-Tick.
 * Lizenz: MIT (Clean Room)
 */
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
    fputc('\\n', f);
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
    line[strcspn(line, "\\r\\n")] = 0;
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
"""

WORKSPACE_FILES["rescued_iter5/verify_livecode.py"] = """#!/usr/bin/env python3
\"\"\"
verify_livecode.py — Headless-Verifikation der ESC-Live-Coding-Console (Meilenstein 4).
Gerettet aus Smith Primes Iteration-5-Entwurf.
\"\"\"
import os
import tempfile
import verify_headless as vh
import verify_core_sim as vcs

STEPS = 16
TICKS_PER_STEP = 5
DEFAULT_PATTERN = "c2 [c2 eb2] [f2 g2] <bb2 [g2 f2]>"
BATH_PATTERN = "<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"

def compile_pattern(code):
    pat = {'nodes': [], 'root_idx': 0, 'raw_source': '', 'code': ''}
    vh.strudel_compile(pat, code, 0)
    return pat

class LiveCodeUI:
    def __init__(self, default_pattern):
        self.buffer = default_pattern
        self.cursor = len(default_pattern)
        self.active = compile_pattern(default_pattern)
        self.pending = None
        self.have_pending = 0
        self.tick = 0
        self.cycle_num = 0
        self.step_in_cycle = 0
        self.editor_open = 0
        self.chest_opened = 0
        self.biome = 0

    def step_tick(self):
        self.tick += 1
        if self.tick % TICKS_PER_STEP == 0:
            self.step_in_cycle += 1
            if self.step_in_cycle >= STEPS:
                self.step_in_cycle = 0
                self.cycle_num += 1
                if self.have_pending:
                    self.active = self.pending
                    self.have_pending = 0

    def current_note(self):
        return vh.strudel_query_step(self.active, self.cycle_num, self.step_in_cycle, STEPS)

    def edit_char(self, c):
        self.buffer = self.buffer[:self.cursor] + c + self.buffer[self.cursor:]
        self.cursor += 1

    def compile_pending(self):
        self.pending = compile_pattern(self.buffer)
        self.have_pending = 1

    def open_chest(self):
        if self.chest_opened:
            return
        self.chest_opened = 1
        self.biome = 1
        self.buffer = BATH_PATTERN
        self.cursor = len(BATH_PATTERN)
        self.active = compile_pattern(BATH_PATTERN)
        self.cycle_num = 0
        self.step_in_cycle = 0
        self.have_pending = 0

def main():
    failures = []
    lc = LiveCodeUI(DEFAULT_PATTERN)
    seq = []
    for _ in range(STEPS * 2):
        seq.append(lc.current_note())
        for _ in range(TICKS_PER_STEP):
            lc.step_tick()
    def fmt(n):
        return f"[{n}]" if n >= 0 else ("~" if n == -1 else ".")
    c0 = " ".join(fmt(seq[i]) for i in range(0, STEPS))
    c1 = " ".join(fmt(seq[i]) for i in range(STEPS, 2 * STEPS))
    exp_c0 = "[36] . . . [36] . [39] . [41] . [43] . [46] . . ."
    exp_c1 = "[36] . . . [36] . [39] . [41] . [43] . [43] . [41] ."
    print("=== STRUDELFORCE Live-Code Console Self-Test (Python-Oracle) ===")
    print(f"Zyklus 0: {c0}")
    print(f"Zyklus 1: {c1}")
    if c0 != exp_c0: failures.append(f"Zyklus0: {c0} != {exp_c0}")
    if c1 != exp_c1: failures.append(f"Zyklus1: {c1} != {exp_c1}")

    lc = LiveCodeUI(DEFAULT_PATTERN)
    lc.open_chest()
    first = lc.current_note()
    print(f"chest: biome={lc.biome} chest_opened={lc.chest_opened} erste_note={first} (erwartet 48=c3)")
    if lc.biome != 1 or not lc.chest_opened or first != 48:
        failures.append("chest-Logik fehlerhaft")

    print("livecode_check=" + ("OK" if not failures else "FEHLER: " + "; ".join(failures)))
    return 0 if not failures else 1

if __name__ == '__main__':
    raise SystemExit(main())
"""

WORKSPACE_FILES["rescued_iter5/ITER5_PATCH_GUIDE.md"] = """# Anleitung für den VS-Code-Agenten: Iteration 5 in `STRUDELFORCE_Smith_Core` einbauen

Smith Prime hatte diese Integration von `livecode_ui.c/.h` in `game_audio.c` und `game_main.c` bereits komplett entworfen:

## 1. `src/game_audio.h` & `src/game_audio.c`
Füge im Struct `GameAudio` (nach `last_bounces`) zwei Felder für das Read-Only-Override ein:
```c
    uint8_t  bass_override_active; /* 1 = Strudel-Pattern steuert die Bassline */
    int16_t  bass_override_note;   /* -1 = Rest, sonst MIDI-Note */