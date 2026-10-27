# LOCAL_ONLY_TESTS — C-Tests & Oracles lokal ausführen

> Anleitung für die vollständige lokale Verifikation von STRUDELFORCE **ohne**
> Cloud/Kiwi-Run. Stand: 2026-10-27 (post-Merge, Windows/MSVC + Python 3.13 referenziert).
> Alles Deterministische: gleiche Eingaben → bit-identische Ausgaben.

---

## 1. Voraussetzungen

| Tool | Zweck | Optional? |
|---|---|---|
| Visual Studio 2022 (BuildTools oder Community) + x64-MSVC | C-Compiler (`cl.exe`) für die Exes | nein (für C-Tests) |
| Python ≥ 3.10 | Oracles, Selftest, `c_vs_oracle`, Handoff-Tools | nein (für Oracles) |
| `pycparser` (`pip install pycparser`) | `tools/c_parse_check.py` | ja |
| `m68k-amigaos-gcc` + `amitools` | Amiga-Cross-Build / ADF / VAMOS-Test | ja (nur Amiga-Targets) |
| Raylib (`%RAYLIB_DIR%`) | Target `pc_game` | ja (nur Raylib-Frontend) |

**Vor jedem Build-Check:** `tasklist | findstr strudelforce` — eine laufende
`strudelforce_*.exe` blockiert den Link-Schritt (LNK1104, .exe gesperrt).

---

## 2. C-Builds

### Option A — Windows/MSVC (1 Klick, 6 Targets)
```cmd
build_msvc.bat
```
| # | Target | Kern | Inhalt |
|---|---|---|---|
| 1 | `strudelforce_game.exe` | Smith (`core_sim.c`) | Terminal-Spiel + Live-Audio |
| 2 | `strudelforce_gui.exe` | Smith (`core_sim.c`) | 2.5D Voxel-GUI + TFMX-Explorer + Strudel-Studio |
| 3 | `test_audio_mix.exe` | Smith (`core_sim.c`) + v3-Audio | TFMX+MOD+Mixer-Harness |
| 4 | `strudelforce_test.exe` | **sf (`core_sim_sf.c`)** | Phase-1-Integrationstest (43 Checks) |
| 5 | `strudelforce_headless.exe` | — (selbstbezüglich) | Audio-Parser-Selftest |
| 6 | `win_audio_selftest.exe` | — (nur `win_audio.c`) | Ring-/Thread-Selftest des Audio-Transports v2 |

> ⚠️ **Dual-Core-Regel:** Targets 1–3 linken den Smith-Kern, Target 4 den sf-Kern.
> Target 6 ist kern-neutral (kein `core_sim*`).
> `core_sim.c` und `core_sim_sf.c` niemals in dieselbe Verlinkung legen.

### Option B — make (WSL / Linux / macOS)
```bash
make all              # game + gui + test_audio_mix
make test             # verify + test_audio_mix + c_vs_oracle + test_phase1
make c_vs_oracle      # C- vs. Python-Referenzplayer (bit-genau)
make headless_test    # Headless Audio-Matrix-Dump
make verify           # Python-Verify (TFMX, MOD, Levels)
make c89_check        # C89-Stichproben via gcc -std=c89 -Wall
make strudel_export   # TFMX-Module im Batch exportieren
make amiga_lint       # 68k-Lint (m68k-amigaos-gcc falls vorhanden, sonst Syntax-Stub)
make amiga_adf        # (optional) bootfähige strudelforce.adf via xdftool
make vamos_test       # (optional) Testlauf im VAMOS-Emulator
```

---

## 3. C-Testlauf (erwartete Ergebnisse)

```cmd
:: 1) Phase-1-Integrationstest (sf-Kern) — Erwartung: 43 PASS / 0 FAIL
.\strudelforce_test.exe

:: 2) Audio-Parser-Selftest (headless)
.\strudelforce_headless.exe

:: 3) TFMX/MOD-Mix — Erwartung: Exit 0, Zeile mit frames=..., clips=0, tfmx_oob=0
.\test_audio_mix.exe --tfmx "TFMX Professional\<Spiel>\<Modul>.tfx" --secs 3 --wav out.wav
::   (Modul-Paare liegen u.a. in tests/data/; --raw statt --wav für den Oracle-Vergleich)

:: 4) Deterministische Game-Demo — Erwartung: DEMO_DONE mit
::    score=400 kills=4 bounces=79 bei --demo 500 (Referenzwert post-Merge)
.\strudelforce_game.exe --demo 500
::   Seed explizit fixieren für Reproduktion: --seed 1234
::   16-Bit WAV-Dump: --audio out.wav
```

---

## 4. Python-Oracles & Selftest (von Projekt-Root)

```bash
python verify_core_sim.py        # Kern-Determinismus + Invarianten (2500 Ticks, Seed 1234)
python verify_headless.py        # Strudel-AST + Macro-Engine
python verify_audio_matrix.py    # Reaktive Audio-Matrix (Waffen/Schild/Low-HP)
python verify_livecode.py        # Live-Code-Compiler + Truhen-Tribute (c3-Pattern)
python tools/selftest.py         # 33-Check-Suite (Parser, MOD-Loop, TFMX, Levels, Rogue 1200x)
python tools/c_vs_oracle.py      # C- vs. Python-Player bit-genau (baut test_audio_mix selbst)
python tools/tfmx_oracle.py --scan "TFMX Professional" 4   # 396/396 Module OK (Referenz)
python tools/level_tool.py validate    # chunks=12 levels=5 errors=0
python tools/headless_verify.py        # alle user_music-Tracks (RESULT: PASS)
python tools/c_parse_check.py          # C-Syntax ohne Compiler (pycparser)
```

Erwarteter Gesamtzustand: **alle OK**, `selftest` mit `SELFTEST OK fails=0` (33/33),
`c_vs_oracle` mit `C_VS_ORACLE OK` (8/8 Sample-Sets, diffs=0).

**Bekannte `c_parse_check.py`-False Positives** (kein Code-Fehler, s. `MERGE_NOTES.md` §5):
`sin`/`pow`/`FIX` (System-Header bzw. function-like Makros), `BITMAPINFO`/`Color`
(gestutzte `windows.h`/`raylib.h`), `tolower`-Familie, Backslash-ASCII-Art in `amiga_intro.c`.

---

## 5. Fehlerbehebung

| Symptom | Ursache | Fix |
|---|---|---|
| `LNK1104: cannot open ...exe` | Alt-Instanz läuft noch | Prozess beenden (`taskkill /IM strudelforce_gui.exe`), neu bauen |
| `fatal error C1083: dirent.h` | MSVC-SDK ohne POSIX-Header | `/Isrc/compat` (steht in build_msvc.bat Target 4) |
| `main_pc.c` nicht kompilierbar | Raylib fehlt | erwartet — nur Target `pc_game` mit `RAYLIB_DIR`; lokal sonst überspringen |
| `make: command not found` (Windows) | kein make | `build_msvc.bat` verwenden oder WSL |
| `c_vs_oracle` meldet diffs | C-Änderung bricht Bit-Genauigkeit | C- und Python-Referenz parallel anpassen, Oracle zuerst |
| `test_phase1` mit 0/43 oder Crashes | falscher Kern gelinkt | nur `core_sim_sf.c` + `livecode_ui_sf.c` (Makefile/Script prüfen) |

---

## 6. Schnell-Check vor jedem Handoff (Kiwi-konform)

```bash
# 1) Regenerierte Artefakte aktuell halten (nach Quellen-Änderung!)
python tools/gen_sources.py .
# 2) Kern-Suite
python tools/selftest.py && python verify_core_sim.py && python c_vs_oracle.py 2>nul || python tools/c_vs_oracle.py
# 3) C-Build + Smoke (Windows: build_msvc.bat, Linux: make test)
# 4) Manifest + ZIP
python tools/kiwi_handoff.py pack . STRUDELFORCE_Handoff_YYYY-MM-DD.zip
```
`archive/` und `TFMX Professional/` werden dabei automatisch übersprungen (`SKIP_DIRS`).