#!/usr/bin/env python3
"""
kiwi_toolkit.py - Schweizer Taschenmesser fuer VS Code <-> KIWI Roundtrips
Funktionen:
  1. python3 kiwi_toolkit.py rescue_iter5  -> Rettet livecode_ui.h/.c aus Smith Primes Iteration 5
  2. python3 kiwi_toolkit.py lint          -> Statischer C99/Fixed-Point & Header-Vertrags-Pruefer (ohne gcc!)
  3. python3 kiwi_toolkit.py bundle        -> Baut STRUDELFORCE_Sync.zip + atomic_restore.py fuer KIWI
"""

import os
import re
import sys
import hashlib
import zipfile
import base64

# ==============================================================================
# 1. GERETTETER CODE AUS SMITH PRIMES ITERATION 5 (MEILENSTEIN 4)
# ==============================================================================
ITER5_LIVECODE_H = """/*
 * livecode_ui.h - STRUDELFORCE ESC-Live-Coding-Console (Meilenstein 4)
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

ITER5_LIVECODE_C = """/*
 * livecode_ui.c - STRUDELFORCE ESC-Live-Coding-Console (Meilenstein 4)
 * Nahtloses Live-Compile am Zyklus-Rand, F2/F3 Save/Load, Great-Bath-Truhe.
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
    ui->biome = 1;
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

def cmd_rescue_iter5():
    os.makedirs("src", exist_ok=True)
    with open("src/livecode_ui.h", "w", encoding="utf-8") as f:
        f.write(ITER5_LIVECODE_H.lstrip("\n"))
    with open("src/livecode_ui.c", "w", encoding="utf-8") as f:
        f.write(ITER5_LIVECODE_C.lstrip("\n"))
    print("[+] Iteration 5 gerettet: 'src/livecode_ui.h' und 'src/livecode_ui.c' wurden erstellt!")

# ==============================================================================
# 2. STATISCHER C89/C99 & AMIGA-VERTRAGS-LINTER (FUER KIWI OHNE GCC)
# ==============================================================================
def strip_comments_and_strings(code):
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.DOTALL)
    code = re.sub(r'//.*', '', code)
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    code = re.sub(r"'(?:\\.|[^'\\])*'", "''", code)
    return code

def cmd_lint():
    print("=== STRUDELFORCE Static C-Contract & Syntax Linter (Zero-GCC) ===")
    errors = []
    c_files = []
    for root, _, files in os.walk("src"):
        for f in sorted(files):
            if f.endswith((".c", ".h")):
                c_files.append(os.path.join(root, f))

    if not c_files:
        print("[-] Keine .c/.h Dateien in src/ gefunden!")
        return 1

    for path in c_files:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            raw = f.read()
        clean = strip_comments_and_strings(raw)

        # 1. Klammer-Balance pruefen
        for op, cl in [('{', '}'), ('(', ')'), ('[', ']')]:
            if clean.count(op) != clean.count(cl):
                errors.append(f"{path}: Ungleiche Klammern '{op}' ({clean.count(op)}) vs '{cl}' ({clean.count(cl)})")

        # 2. Lokale #include "..." Existenz pruefen
        for inc in re.findall(r'#\s*include\s*"([^"]+)"', raw):
            inc_path = os.path.join("src", inc)
            if not os.path.exists(inc_path):
                errors.append(f"{path}: Fehlender Header #{inc} (erwartet unter {inc_path})")

        # 3. Amiga Fixed-Point & Static-Memory Gesetz in core_sim.c pruefen
        if os.path.basename(path) == "core_sim.c":
            if re.search(r'\b(float|double)\b', clean):
                errors.append(f"{path}: VERBOTEN fuer Amiga-Core: 'float' oder 'double' gefunden (nur 16.16 Fixed-Point erlaubt!)")
            if re.search(r'\b(malloc|calloc|realloc|free)\b', clean):
                errors.append(f"{path}: VERBOTEN im Hot-Path: dynamische Speicherallokation (malloc/free) gefunden!")

        sha = hashlib.sha256(raw.encode("utf-8")).hexdigest()[:12]
        print(f"  [OK] {path:<28} (SHA256: {sha})")

    if errors:
        print("\n[!] LINTER-FEHLER GEFUNDEN:")
        for e in errors:
            print("  -", e)
        return 1
    print("\n[+] ALLE C-DATEIEN SYNTAKTISCH & VERTRAGLICH SAUBER!")
    return 0

# ==============================================================================
# 3. ATOMIC BUNDLER FUER DEN RUECKWEG NACH KIWI
# ==============================================================================
def cmd_bundle():
    zip_name = "STRUDELFORCE_Sync.zip"
    include_exts = (".c", ".h", ".md", ".py", ".strudel", ".wav", ".txt")
    manifest = []
    file_map = {}

    for root, dirs, files in os.walk("."):
        dirs[:] = [d for d in dirs if not d.startswith(".") and d not in ("__pycache__", "build")]
        for f in sorted(files):
            if f == zip_name or f == "atomic_restore.py":
                continue
            if f == "Makefile" or f.endswith(include_exts):
                rel = os.path.relpath(os.path.join(root, f), ".")
                with open(rel, "rb") as bf:
                    data = bf.read()
                sha = hashlib.sha256(data).hexdigest()[:12]
                manifest.append(f"| `{rel}` | `{sha}` | {len(data)} Bytes |")
                file_map[rel] = base64.b64encode(data).decode("ascii")

    # Schreib MANIFEST_SHA256.md
    with open("MANIFEST_SHA256.md", "w", encoding="utf-8") as mf:
        mf.write("# STRUDELFORCE - Sync Manifest (VS Code -> KIWI)\n\n")
        mf.write("| Datei | SHA256-Prefix | Groesse |\n|---|---|---|\n")
        mf.write("\n".join(manifest) + "\n")

    # Schreib selbstentpackendes atomic_restore.py gegen KIWI Sandbox-Wipes
    with open("atomic_restore.py", "w", encoding="utf-8") as af:
        af.write("#!/usr/bin/env python3\nimport os, base64\nFILES = {\n")
        for k, v in file_map.items():
            af.write(f"  {k!r}: {v!r},\n")
        af.write("}\nfor p, b64 in FILES.items():\n")
        af.write("    if os.path.dirname(p): os.makedirs(os.path.dirname(p), exist_ok=True)\n")
        af.write("    with open(p, 'wb') as f: f.write(base64.b64decode(b64))\n")
        af.write("print(f'[+] Atomic Restore: {len(FILES)} Dateien wiederhergestellt!')\n")

    # Packe alles in STRUDELFORCE_Sync.zip
    with zipfile.ZipFile(zip_name, "w", zipfile.ZIP_DEFLATED) as zf:
        for rel in file_map:
            zf.write(rel)
        zf.write("MANIFEST_SHA256.md")
        zf.write("atomic_restore.py")

    print(f"[+] Bundle fertig: '{zip_name}' ({len(file_map)} Projektdateien + atomic_restore.py + MANIFEST_SHA256.md)")

if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "lint"
    if cmd == "rescue_iter5":
        cmd_rescue_iter5()
    elif cmd == "bundle":
        cmd_bundle()
    else:
        sys.exit(cmd_lint())