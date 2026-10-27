# STRUDELFORCE — BUILD.md (Kompilieren & Testen)

## 1. Headless-Test des C-Audio-Kerns (schnellster Einstieg)
Läuft auf jedem Linux/macOS/Windows (via WSL/MinGW) mit `gcc` + `make`, **ohne Raylib**:

```bash
cd STRUDELFORCE_Smith_Core
make headless_test
./strudelforce_test
```

Erwartetes Output (Referenz-Trace, s. handoff.md §6):
```
=== STRUDELFORCE Clean-Room Audio Core Self-Test ===
Live-Code Pattern: c2 [eb2 g2] c3*2 <bb2 g2(3,8)>
Generierte AST-Knoten: 9

Cycle 0: [36]  .   .   .  [39]  .  [43]  .  [48]  .  [48]  .  [46]  .   .   .
Cycle 1: [36]  .   .   .  [39]  .  [43]  .  [48]  .  [48]  .  [43]  ~   ~  [43]

TFMX 50Hz Macro Engine bereit. Sample-RAM initialisiert (65536 Bytes).
```
(`[N]` = MIDI-Note, `~` = Pause, `.` = Sustain)

## 2. Spielbares Terminal-Game (Meilenstein 2+3, ohne Raylib)
```bash
make game
./strudelforce_game                       # interaktiv: WASD/Pfeile, W springen,
                                          # Z feuern, X Surround-Beam, C Waffenwechsel, Q Beenden
./strudelforce_game --demo 2500           # deterministischer Headless-Demo-Run (Bot)
./strudelforce_game --demo 2500 --audio demo.wav   # + 16-Bit WAV-Dump (16574 Hz Mono)
```
Der `--audio`-Dump ist die **hörbare C-Verifikation** der reaktiven Audio-Matrix
(Waffe→Bass-Macro, HP<25%→Cutoff 550Hz + 4-Bit-Quant, SFX-Transitions).
Deterministischer Seed → reproduzierbares, diff-bares Audio.

Cleanup: `make clean`

### CFLAGS-Erklärung
| Flag | Bedeutung |
|------|-----------|
| `-std=c99` | C99-Standard (Header nutzen C99-`for`-Declarations, `strchr`, `sscanf`) |
| `-Wall -Wextra` | Vollständige Warnebene (saubere Warnfreiheit ist Projekt-Ziel) |
| `-O2` | Standard-Optimierung |
| `-Isrc` | Header-Pfad für `strudel_cleanroom.h` / `tfmx_macro_synth.h` |
| `-lm` | Math-Lib (sin/pow in der TFMX-Engine) |

### Manuelle Kompilierung (ohne Make)
```bash
gcc -std=c99 -Wall -Wextra -O2 -Isrc src/test_core_audio.c -o strudelforce_test -lm
./strudelforce_test
```

## 3. Dual-Target-Architektur
- **PC-Ziel:** `gcc` (oder `clang`) + Raylib-Frontend (Phase 3+, `LDFLAGS_PC = -lraylib -lm -lpthread -ldl` im Makefile).
- **Amiga-Ziel:** `m68k-amigaos-gcc` (Bebbo-Toolchain / Bartman). Der Kern ist strikt C89/C99 mit 16.16 Fixed-Point (Gameplay-Core); die Audio-Engine nutzt aktuell noch `float`/`double` und muss im Amiga-Backport (Agent 5) auf Fixed-Point umgestellt werden (s. captains_log.md, Stolperstein #2).

## 4. Verifikation OHNE C-Toolchain (Sandbox-Workaround)
In Umgebungen ohne gcc/make/Netzwerk (z. B. KI-Sandboxes) dienen die faithful Python-Oracles als Verifikation:

```bash
python3 verify_core_sim.py --seed 1234 --ticks 2500   # Gameplay-Core: Determinismus + Invarianten
python3 verify_audio_matrix.py                        # Reaktive Audio-Matrix (importiert Core-Oracle)
python3 verify_headless.py                            # Strudel-Parser + TFMX-Init
```

Die Oracles führen die EXAKTE C-Logik aus (gleiche Fixed-Point-Konstanten, gleiche
Sequenz, gleicher PRNG). Sie sind **kein Ersatz** für den C-Build, sondern belegen,
dass die C-Logik die Referenz-Traces reproduziert. Details: WORKFLOW_ESSAY.md.

## 5. Projekt-Inhalt
- `src/strudel_cleanroom.h` — Clean-Room Strudel/TidalCycles Mini-Notation Parser (C99, header-only)
- `src/tfmx_macro_synth.h` — 50Hz TFMX-Macro-Engine + Paula-Simulator (C99, header-only)
- `src/core_sim.h/.c` — Gameplay-Core (16.16 Fixed-Point, Waffen, Bounce 2.0, statische Ringbuffer)
- `src/game_audio.h/.c` — Reaktive Audio-Engine (8 Stimmen, Low-Pass, 4-Bit-Quant, SFX-Diffs)
- `src/game_main.c` — Terminal-Frontend (50Hz) + `--demo`/`--audio`-Modus
- `src/test_core_audio.c` — Headless-Selbsttest (Audio-Parser)
- `Makefile` — Dual-Target (PC/Amiga), Targets: `headless_test`, `game`, `test_game`
- `specs/` — 5 Modul-Spezifikationen
- `assets/samples/` — 3 generierte 8-Bit Amiga-Samples (.wav, 16574 Hz)
- `user_music/` — Beispiel-Strudel-Tracks
- `verify_core_sim.py` / `verify_audio_matrix.py` / `verify_headless.py` — Python-Oracles
- `test_output_python.txt` — eingefangener Verifikations-Run
- `WORKFLOW_ESSAY.md` — Workflow-Studie: A/B-Vergleiche, Sackgassen, Protokoll für künftige Modelle
- `todo.md` / `handoff.md` / `captains_log.md` — Steuerdateien (Projekt-Status)
