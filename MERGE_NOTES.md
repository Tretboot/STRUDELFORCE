# STRUDELFORCE — Merge-Report & Provenance (2026-10-27)

> Dieses Dokument erklärt, woher jede Datei des jetzt **einzigen** Projekts stammt,
> welche Konflikte beim Merge gelöst wurden und wie das Ergebnis verifiziert wurde.
> Die historischen Zweig-Ordner liegen unverändert in `archive/branches/`.

---

## 1. Ausgangslage

| Quelle | Inhalt |
|---|---|
| `STRUDELFORCE_v3.zip` (Claude/Kiwi-Run) | 149 Dateien + MANIFEST (sha256). v3-Audio-Stack, Levels, Tools, Doku. **Komplett und konsistent.** |
| Zweig A `STRUDELFORCE/` ("Schwarm") | sf-Kern (`sf_interfaces.h`), Voxel-Renderer, Cyber-Paula, Amiga-HW, Biome, Raylib-Frontend, Phase-1-Test |
| Zweig B `STRUDELFORCE_Smith_Core/` ("Smith") | spielbarer Kern + Terminal/GUI-Frontends, game_audio, TFMX-Player v1, Python-Oracles |
| Lokale Root (MSVC-Session 14:10–15:33) | partieller Merge + `gui_main.c`, `main_amiga.c`, `retro_font.h`, `build_msvc.bat` |
| Claude-Extrakte (47 Dateien) | Alle byte-identisch mit der ZIP. Neu waren nur `KIWI_PERSISTENCE_GUIDE.md` + `c_parse_check.py` (post-ZIP). |

**Ergebnis der Bestandsaufnahme:** 106 Dateien bereits identisch, 5 veraltet, 39 fehlend —
alles aus der ZIP restauriert. `(1)`-Duplikate (4 Stück) waren byte-identisch und wurden verworfen.

---

## 2. Die Dual-Core-Architektur (wichtigste Merge-Entscheidung)

Die beiden Kern-Implementierungen sind **unterschiedliche Codebasen mit kollidierenden
Symbolnamen** — beide definieren `core_init`/`core_tick` und ein eigenes `GameState`:

| Kern | Datei | API | Nutzer |
|---|---|---|---|
| **Smith-Kern** (kanonisch) | `src/core_sim.c` / `core_sim.h` | `core_init(GameState*, uint32_t seed)`, `core_tick(...)` | `game_main.c`, `gui_main.c`, `game_audio.c`, `win_audio.c` |
| **sf-Kern** (Schwarm) | `src/core_sim_sf.c` (renamed von `STRUDELFORCE/src/core_sim.c`) | `core_init(GameState*, int16_t biome, uint32_t seed)` via `sf_interfaces.h` | `main_pc.c`, M68K-Sektion von `amiga_hw.c` (früher `main_amiga.c`, 2026-10-27 entfernt), `test_phase1.c`, `voxel_render.c`, `cyber_paula.c`, `biomes.c`, `amiga_hw.c`, `livecode_ui_sf.c` |

**Regel:** `core_sim.c` und `core_sim_sf.c` dürfen **niemals in einem Binary gelinkt werden**
(C-Linker prüft keine Signaturen — das alte `test_phase1`-Target war deshalb nur durch
Struktur-Layout-Pech "bestanden"). Makefile + `build_msvc.bat` sind entsprechend fixiert.

Analog: `livecode_ui.c/.h` (Smith, Zyklus-Swap, für Game/GUI) und `livecode_ui_sf.c`
(Schwarm-API aus `sf_interfaces.h`, für PC/Amiga-Targets) — zwei verschiedene `LiveCodeUI`-Typen,
nie zusammen linken.

---

## 3. Provenance der `src/`-Dateien

| Datei | Herkunft | Anmerkung |
|---|---|---|
| `core_sim.c/.h` | B (Smith) | kanonischer Kern, Oracle-verifiziert |
| `core_sim_sf.c` | A (Schwarm) | **renamed**, SIN8/COS8-Fix, 6-Waffen, sf_interfaces-API |
| `sf_interfaces.h` | A | Schnittstelle des sf-Kerns (inkl. `LiveCodeUI`-Deklaration für pc/amiga) |
| `game_main.c` | B | Terminal-Frontend (Smith-Kern) |
| `gui_main.c` | lokal (Root) | Win32/GDI 2.5D-Voxel-GUI (Smith-Kern) |
| `main_pc.c` | A | Raylib-Frontend, optionales Target `pc_game` (braucht `RAYLIB_DIR`) |
| `main_amiga.c` | lokal (Root) | ⚠ **entfernt (2026-10-27, 19:xx-Session):** war inkonsistent zur aktuellen `sf_interfaces.h`/`amiga_hw.c`-API. Der 68k-`main()` lebt jetzt in der **M68K-Sektion von `amiga_hw.c`** (einziges Amiga-Entry-Point, inkl. TFMX-Paula-Musik) |
| `amiga_intro.c/.h` | B | Copperbar/Sinus-Scroller-Intro |
| `game_audio.c/.h` | **v3 (ZIP)** | +`ext_render`-Hook für externe Quellen (TFMX/MOD/Mixer) |
| `cyber_paula.c/.h` | A | 8-Stimmen-TFMX-Mixer (sf-Seite) |
| `tfmx_player.c/.h` | **v3 (ZIP)** | v2: TFMX 1.5/Pro/7V, mdat./smpl.-Paare, TFHD, TFMXPAK, C89, Oracle-Mirror. API-v1-kompatibel (legacy `tfmx_player_tick`) |
| `tfmx_macro_synth.h` | A = B (identisch) | 50Hz-Makro-Synthese |
| `mod_player.c/.h` | **v3 (ZIP)** | ProTracker-MOD 4/6/8 Kanäle, C89, Oracle-Mirror |
| `sf_mixer.c/.h` | **v3 (ZIP)** | mixt TFMX+MOD+Synth mit Gain-Ramps, `sf_mixer_ext_render` → `GameAudio.ext_render` |
| `sid_header.c/.h` | **v3 (ZIP)** | .sid-Header-Reader (No-Playback, Vorbereitung) |
| `strudel_cleanroom.h` | B, **C89-geräumt** | Deklarations-Hoisting (s. §4.4), `#ifndef`-Guard für `_CRT_SECURE_NO_WARNINGS` |
| `livecode_ui.c/.h` | B | ESC-Konsole mit Zyklusrand-Swap (Smith-Kern) |
| `livecode_ui_sf.c` | A (renamed) | sf-API `livecode_on_input`/`livecode_compile_swap`/`livecode_list_user_music` |
| `voxel_render.c/.h` | A | 4-Layer-2.5D-Planer |
| `biomes.c` | A | 3 Biome + deterministischer Chunk-Generator |
| `amiga_hw.c` | A | OCS/AGA Bitplanes, Copper, CIA-A $BFE001 |
| `win_audio.c/.h` | B | WaveOut-Backend (Windows) |
| `retro_font.h` | lokal (Root) | 8x8-Bitmap-Font |
| `level_data.h` | **v3 (ZIP)**, generiert | aus `levels/levels.json` via `tools/level_tool.py header` |
| `test_core_audio.c` | A = B (identisch) | Audio-Parser-Selftest |
| `test_phase1.c` | A | 43-Check-Integrationstest (sf-Seite) |
| `test_audio_mix.c` | **v3 (ZIP)** | lokales Mixer-Harness (TFMX+MOD → WAV/raw) |
| `compat/dirent.h`, `compat/unistd.h` | A | Win32-Shims für MSVC-SDKs ohne native Header (nur Target 4, `/Isrc/compat`) |

---

## 4. Bei dem Merge behobene Fehler

1. **Makefile `test_phase1` verlinkte den falschen Kern** (B statt A) → UB-Risiko,
   nur durch Layout-Zufall bestanden. → `core_sim_sf.c` + `livecode_ui_sf.c` (fixiert in
   `tools/gen_sources.py`, das die Makefile als Single Source of Truth erzeugt).
2. **Makefile `AMIGA_SRCS`/`amiga_lint`** referenzierten `core_sim.c` + unnötiges
   `game_audio.c` (falsches `GameState`-Layout, Chip-RAM-Ballast). → `core_sim_sf.c`,
   `game_audio.c` entfernt (main_amiga nutzt gar kein Audio-Modul). *(Update 2026-10-27:
   der Amiga-Target streamt seither TFMX-Musik per Paula — `tfmx_player.c` + Paula-Voice-0
   in der M68K-Sektion von `amiga_hw.c`; 8-Voice-Matrix bleibt offener Roadmap-Punkt.)*
3. **MOD-Loop-Test in `tools/selftest.py`** renderte 6 s, der Song endet aber bei ~7,7 s
   → Check "mod loops (D-break)" konnte nie bestehen. → `16574 * 9` (das war der 1 von 33
   Fehltests im Kiwi-Run).
4. **`strudel_cleanroom.h` verletzte C89** (Mischdeklarationen) — betrifft 11 Translation
   Units inkl. Amiga-Build. → alle Deklarationen nach Block-Start hoisted (Semantik invariant).
5. **`dirent.h`/`unistd.h` nicht im MSVC-SDK** → `src/compat/`-Shims für Target 4 angebunden.
6. **`build_msvc.bat`** baute nur 2 von 5 Targets → auf 5 erweitert (Game, GUI, Mixer-Harness,
   Phase-1-Test, Headless-Audio), inkl. Dual-Core-Kommentar.
7. **`kiwi_handoff.py`** würde `archive/` + `TFMX Professional/` in Manifest/Pack ziehen →
   beide in `SKIP_DIRS`.

## 5. Verifikation (lokal, 2026-10-27, Windows/MSVC + Python 3.13)

| Check | Ergebnis |
|---|---|
| `verify_core_sim.py` (2500 Ticks, Seed 1234) | determinism_check=OK, invariants=OK |
| `verify_headless.py` | OK (AST 9 Knoten, Macro-Engine) |
| `verify_audio_matrix.py` | matrix_check=OK (Waffen-Abdeckung, Low-HP-Phase) |
| `verify_livecode.py` | livecode_check=OK (Truhe → c3-Tribute) |
| `tools/selftest.py` | **SELFTEST OK fails=0** (33/33, inkl. MOD-Loop-Fix, 1200 Rogue-Runs) |
| `tools/tfmx_oracle.py --scan "TFMX Professional" 4` | **SCAN ok=396 failed=0** |
| `tools/level_tool.py validate` | chunks=12 levels=5 errors=0 |
| `tools/headless_verify.py` | RESULT: PASS (alle user_music-Tracks) |
| `tools/c_parse_check.py` (pycparser) | 6 clean; rest = dokumentierte Checker-Limitationen (u.) |
| MSVC-Build | game ✓, test_audio_mix ✓, test_phase1 ✓, headless ✓ (gui: Code kompiliert, .exe-Schreibzugriff durch laufende Instanz blockiert) |
| `strudelforce_test.exe` (Phase-1, **korrekter** sf-Kern) | **43 PASS / 0 FAIL** |
| `strudelforce_game.exe --demo 500` | DEMO_DONE — bit-identisch zum Pre-Merge-Run (score=400 kills=4 bounces=79) |
| `test_audio_mix.exe` (Apidya Meadow, 3 s) | frames=49722 peak=8477 clips=0 tfmx_oob=0 |
| `tools/c_vs_oracle.py` | **C_VS_ORACLE OK** — C vs Python bit-genau, 8/8 Sample-Sets diffs=0 |

### c_parse_check: dokumentierte False Positives
- `implicit declaration: sin/pow/FIX` → Mini-Preprocessor stuft System-Header (`math.h`)
  aus und expandiert function-like Makros (`FIX(v)`) nicht; der Code inkludiert korrekt.
  Float in `game_audio.c` bzw. Compiletime-Konstanten in `FIX()` sind die dokumentierte Exception.
- `PARSE ERROR` in `gui_main.c` (`BITMAPINFO` aus `windows.h`), `main_pc.c` (`Color` aus
  `raylib.h`), `amiga_intro.c` (Backslash-ASCII-Art) → Header werden gestutzt, Typen fehlen.
  `main_pc.c` ist lokal deshalb nicht kompilierbar (kein Raylib) — `pc_game` bleibt
  dokumentiertes Option-Target.
- `tolower/isdigit/isspace` → `<ctype.h>` wird im Header korrekt inkludiert, der Checker stuft es aus.

---

## 6. Neues Verzeichnislayout (post-Merge)

```text
STRUDELFORCE_Master/          ← EINE Projekt-Root (= Ziellayout des v3-ZIPs + Merge)
├── src/                      ← vereinter C-Code (Dual-Core, s. §2)
├── tools/                    ← alle Python-Tools (3 alt + 9 v3 + c_parse_check + headless_verify)
├── verify_*.py (4)           ← Smith-Oracles, von Projekt-Root aus ausführbar
├── levels/levels.json        ← 12 Chunks, 5 Level, 4 Paletten (+ deterministischer Rogue-Generator)
├── docs/                     ← specs/ (5 Agents + PHASE1_CONTRACT), img/ (10 PNG),
│                               HANDOFF_SWARM/SMITH.md, WORKFLOW_ESSAY.md, BUILD.md,
│                               todo_smith.md, KIWI_PERSISTENCE_GUIDE.md, PHASE1_AUDIO_SELFTEST.txt
├── tests/                    ← LAST_KIWI_TEST_RUN.txt, data/ (2 TFMX-Paare + sfsong)
├── user_music/               ← 8 Strudel-Tracks
├── assets/samples/           ← 3 Amiga-8-Bit-Samples
├── .vscode/tasks.json        ← VS-Code-Aufgaben (verify, c89, c_vs_oracle, amiga_adf, kiwi pack)
├── Makefile                  ← von tools/gen_sources.py generiert (editiere den Generator!)
├── build_msvc.bat            ← 6-Target MSVC-Build (Windows ohne make)
├── MANIFEST.json             ← sha256-Index (kiwi_handoff.py manifest .)
├── TFMX Professional/        ← 76-Spiele-Asset-Bibliothek (nur lokal referenziert)
├── releases/                 ← fertige v0.5.0-Distribution-ZIPs
└── archive/
    ├── branches/             ← STRUDELFORCE/ + STRUDELFORCE_Smith_Core/ (Originalzustände)
    ├── ClaudeRun/            ← Claude-Extrakte + STRUDELFORCE_v3.zip (Sicherheitskopie)
    ├── build_artifacts/      ← alte .obj/.exe/.wav, Makefile.local_v0.5_backup
    └── STRUDELFORCE_Claude_Handoff.zip
```

**Arbeitsfluss (Kiwi-konform):** Quellen ändern → `python tools/gen_sources.py .`
(Header/Makefile/Strudel regenerieren) → `python tools/selftest.py` + Oracles →
`python tools/kiwi_handoff.py pack . out.zip` (Manifest+Verify+ZIP in einem Schritt).