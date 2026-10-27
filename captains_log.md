# STRUDELFORCE — captains_log.md (Forschungstagebuch)

> Chronologisch, detailreich. Jeder Meeseeks-Einsatz = eine Iteration.
> Regel: Was lief gut (Muster, Flags), was lief schief (Sackgassen, Bugs) → für zukünftige Klone.

---

## Captain's Log — Iteration 1 (Bootstrap + Umgebungs-Audit)

**Auftrag:** Projekt im separaten Verzeichnis initialisieren; Altsystem-Artefakte auditieren; drei Steuerdateien anlegen; ersten Meeseeks los schicken.

### 1. Altsystem-Audit (übergeordnetes Verzeichnis /mnt/data)
- **Gefunden:**
  - `gemini-code-1793055820039.py` (30.916 B) — **das Altsystem-Bootstrap-Skript**. Ein Python-Generator, der das komplette STRUDELFORCE-Kit aus Python-String-Konstanten erzeugt: 5 Agent-Specs, den Clean-Room C-Parser (`strudel_cleanroom.h`), die TFMX-Engine (`tfmx_macro_synth.h`), `test_core_audio.c`, Makefile, 3 Amiga-8-Bit-Samples (.wav) und 2 Strudel-Tracks. Packt alles in `STRUDELFORCE_Agent_Kit.zip`.
  - `STRUDELFORCE_Agent_Kit.zip` (37.318 B) — vom Bootstrap erzeugtes Artefakt.
  - `script.sh` (264 B) — Harness-/Laufzeit-Artefakt der Sandbox, **kein** Altsystem-Code.
- **Nicht gefunden:** keine separaten `.txt`-Logs, keine `.c`-Teilergebnisse, keine Build-Artefakte im übergeordneten Verzeichnis. Der gesamte Altsystem-Code steckt **kompakt im Bootstrap-Skript** (als Python-Strings) bzw. im erzeugten Kit.
- **Fazit:** Der "Altsystem"-Fußabdruck ist ein einzelner Self-Contained-Generator. Kein verstreuter Zustand, keine halbfertigen Logs → saubere Ausgangslage für den Clean-Room-Aufbau.

### 2. Bootstrap ausgeführt
- `python3 gemini-code-1793055820039.py` → RC=0. Erzeugte `STRUDELFORCE_Kit/` + `STRUDELFORCE_Agent_Kit.zip`.
- `STRUDELFORCE_Kit` → **`STRUDELFORCE_Smith_Core`** renamed (unser separates Projekt-Verzeichnis).
- Struktur verifiziert: Makefile (Tabs korrekt, `^I`), `src/*.h`, `src/test_core_audio.c`, `assets/samples/*.wav`, `user_music/*.strudel`, `specs/*.md`.

### 3. Umgebungs-Audit (Toolchain) — **Kritischer Stolperstein**
- **Kein C-Compiler:** `gcc`, `cc`, `clang`, `tcc` → alle NICHT vorhanden.
- **Kein Make:** `make` NICHT vorhanden.
- **Keine Binutils:** `ld`, `as`, `ar`, `nm`, `strip` NICHT vorhanden.
- **Kein Netzwerk:** `urllib` → `Operation not permitted`; `apt` vorhanden aber ohne Cache + ohne Netz → keine Installation möglich.
- **Verfügbar:** Python 3.14.4 (mit `ctypes`, `cffi`), Node 24, Bun 1.4.
- **Konsequenz:** `make headless_test && ./strudelforce_test` ist in dieser Sandbox **nicht ausführbar**.
  - cffi-Compilation als Workaround geprüft → benötigt ebenfalls gcc → **ausscheidend**.
  - **Entscheidung:** Faithful Python-Port des Headless-Tests als Verifikations-Workaround; C-Code + Makefile bleiben das saubere Deliverable.

### 4. Was lief gut
- Bootstrap-Skript lief fehlerfrei (RC=0), saubere, vollständige Struktur.
- Makefile-Recipe mit korrekten Tabs (`^I`) — kein klassisches Makefile-Tab-Bug.
- Altsystem-Code ist self-contained → kein verstreuter Zustand, kein "Archäologie"-Overhead.

### 5. Lessons Learned / Stolpersteine für zukünftige Klone
1. **NIEMALS versuchen, gcc/make in dieser Sandbox zu installieren** (kein Netz, kein apt-Cache). Zeitverschwendung. Sofort auf den Python-Verifikations-Pfad wechseln.
2. **Audio-Engine nutzt `float`/`double`** (`tfmx_macro_synth.h`: `sin`, `pow`, `cutoff_hz`, `resonance_q`). Für den PC-Ziel ok, **verstößt aber** gegen die Amiga-68k Fixed-Point-Regel → muss im Amiga-Backport (Phase 2/5) auf 16.16 Fixed-Point umgestellt werden. Nicht übersehen.
3. **Euklidischer Degenerations-Fall:** `strudel_query_step` berechnet `sub_span = span / euclid_n`. Bei `span=4, euclid_n=8` → `sub_span=0→1`. Das komprimiert das (3,8)-Euklid-Raster stark. Semantik prüfen, ob beabsichtigt (möglicher Clean-Room-Feinschliff).
4. **C89-Striktheit:** Header nutzen C99-Features (`for`-Declarations wie `for (int i=0;...)`, `strchr`, `sscanf`). Für striktes C89 müssten `for`-Declarations hoisted werden. Aktuell `-std=c99` im Makefile → ok für PC; Amiga-Backport ggf. C89-pur.
5. **Referenz-Trace** des Headless-Tests (AST=9, Cycle-Grid) in handoff.md §6 abgelegt → jede Verifikation dagegen prüfen.

### 6. Nächster Schritt
- **Meeseeks #1** los schicken: Toolchain-Gap bestätigen, `verify_headless.py` (faithful Python-Port) bauen + ausführen, `BUILD.md` schreiben, kompakten Statusbericht liefern.

---

## Captain's Log — Iteration 2 (Meeseeks #1: Headless-Verifikation + Abnahme + Recovery)

**Auftrag:** Toolchain-Gap bestätigen, faithful Python-Port des Headless-Tests bauen + ausführen, BUILD.md schreiben, kompakten Statusbericht liefern.

### 1. Meeseeks #1 — Einsatz & Report
- Klon lieferte einen sauberen 15-Zeilen-Report: Toolchain-Gap bestätigt (gcc/cc/make/tcc/clang alle abwesend), `verify_headless.py` gebaut, Output angeblich byte-exakt zur C-Logik, `BUILD.md` + `test_output_python.txt` + ZIP erstellt, SHA256-Präfixe der unveränderten C-Quellen gemeldet (`f98fb524…`, `be7dec5a…`, `0faa251a…`, `11f306dd…`).
- **Aber:** Die gemeldeten Dateien waren in meinem `/mnt/data` **nicht vorhanden**.

### 2. Abnahme-Fund — Persistenz-Gap (Kritisch, Systematik)
- Eigenes `find` + `python3 verify_headless.py` (RC=2, Datei fehlt) + ZIP-Check (fehlt) → Meeseeks-Dateien nie in meiner Sandbox angekommen.
- **Tieferer Befund:** Nicht nur Subagent-Dateien, sondern auch **meine** bash-generierten Dateien ohne erkannte Extension (`Makefile`, `strudel_cleanroom.h`, `tfmx_macro_synth.h`, `*.wav`, `*.strudel`) verschwinden zwischen Aufrufen. Nur Dateien mit erkannten Extensions (`.md`, `.py`, `.c`, `.txt`, `.zip`, `.sh`) überleben. Das erklärt auch, warum der Dateibaum zwischen Aufrufen schrumpfte.
- **Folge-Protokoll (BINDEND für alle zukünftigen Klone, s. handoff.md §4):**
  1. Recovery: `python3 gemini-code-1793055820039.py` → `cp` aus `STRUDELFORCE_Kit/` → `rm -rf STRUDELFORCE_Kit`. Das Bootstrap ist **deterministisch** (gleiche SHA256-Präfixe bei jedem Lauf — verifiziert).
  2. ZIP immer im selben bash-Aufruf packen, in dem die C-Dateien frisch regeneriert wurden.
  3. Subagent-Datei-Output niemals blind vertrauen — Abnahme immer mit eigenem `find`/`sha256sum`.

### 3. Recovery + Re-Build der Artefakte (von SMITH PRIME selbst, nicht Meeseeks)
- Bootstrap neu ausgeführt (RC=0), alle C-Quellen/Assets/Makefile per `cp` zurückkopiert, SHA256-Präfixe **stimmen exakt** mit dem Meeseeks-Report überein → der Klon hatte die C-Quellen tatsächlich unverändert gelassen; nur die Übertragung seiner neuen Dateien fehlte.
- `verify_headless.py` + `BUILD.md` wurden von SMITH PRIME direkt neu erstellt (`.py`/`.md` persistieren zuverlässig).

### 4. ECHTER RESEARCH-FIND — EUCLID-sscanf-Bug (wichtigster Fund der Iteration)
- Der erste Python-Port lief, aber **Cycle 1 wies Abweichung** auf: statt `[43] ~ ~ [43]` kam `[43] . . [43]` (Sustain statt Rest).
- **Root Cause:** Im C-Code parst `strudel_parse_expr` den EUCLID-Teil per `sscanf(paren+1, "%d,%d", &k, &n)`. `sscanf` ist **tolerant** gegenüber nachfolgendem Müll: Für Token `g2(3,8)` liest es `3` und `8` und ignoriert das schließende `)`. Mein Python-Port hatte `int("8)")` versucht → `ValueError` → EUCLID-Knoten stillschweigend zu ATOM degradiert → `local_step==0`-Logik statt Rest-Logik → falsches Sustain.
- **Fix im Port:** `re.match(r'\s*(-?\d+)\s*,\s*(-?\d+)', tok[paren+1:])` — mimt sscanf-Toleranz exakt.
- **Ergebnis nach Fix:** Referenz-Trace **ALLE OK** (AST=9, Cycle 0, Cycle 1, TFMX-Zeile). Der C-Code ist hier also **korrekt**; der Bug lag im Port. → **Lesson:** C-`sscanf`/`atoi`-Semantik (tolerant, leading-parse) ist bei C→Python-Ports die häufigste semantische Falle. Immer `atoi` = "leading int or 0" und `sscanf` = "leading parse, ignore rest" modellieren.
- **Offene Design-Frage (für Iteration 3+):** `strudel_query_step` EUCLID: `sub_span = span / euclid_n` mit Fallback `0→1`. Bei `span=4, euclid_n=8` → `sub_span=1`, das (3,8)-Raster wird komprimiert. Ist das beabsichtigte Clean-Room-Verhalten oder ein Feinschliff-Kandidat? → in todo.md Phase 2 vermerkt.

### 5. Was lief gut
- Deterministisches Bootstrap-Skript = perfektes Recovery-Werkzeug (SHA256-stabil über Läufe).
- SHA256-Präfix-Protokoll: Meeseeks-Report ließ sich objektiv gegen die Recovery verifizieren.
- Referenz-Trace (handoff.md §6) hat den Port-Bug **sofort** sichtbar gemacht — Trace-first-Verifikation zahlt sich aus.
- Der EUCLID-Fix ist klein, aber ein Muster für alle zukünftigen C→Python-Ports.

### 6. Lessons Learned / Stolpersteine (ergänzt)
6. **Persistenz-Regel:** `.h`, `Makefile`, `.wav`, `.strudel` in /mnt/data = flüchtig. Recovery-Protokoll (handoff.md §4) vor JEDEM Arbeitspaket ausführen, das C-Dateien braucht.
7. **Subagent-Abnahme:** Niemals "Meeseeks sagt es existiert" = "es existiert". Immer `find` + `sha256sum` selbst.
8. **sscanf/atoi-Toleranz** bei C→Python-Portierung (s. §4).

### 7. Lieferung (Meilenstein 1 erreicht)
- `STRUDELFORCE_Smith_Core.zip` (21 Dateien, 49.260 B, CRC-OK) in `/mnt/data/` — enthält: C-Quellen (unverändert, SHA256-verifiziert), Makefile, 5 Specs, 3 Amiga-8-Bit-Samples, 2 Strudel-Tracks, BUILD.md, verify_headless.py, test_output_python.txt, 3 Steuerdateien.
- Kompilieren auf echter Maschine: `cd STRUDELFORCE_Smith_Core && make headless_test && ./strudelforce_test` (Details in BUILD.md).
- **Nächster Schritt (Iteration 3):** Meilenstein 2 — `src/core_sim.c` (Fixed-Point Gameplay-Kern).

---

## Captain's Log — Iteration 3 (Meilenstein 2: spielbarer Core + Terminal-Frontend + Workflow-Essay)

**Auftrag (User):** Durchgehend arbeiten bis zu einer spiel- & testbaren Version; selbst headless testen; Meeseeks kurz & mit Toolchain-/Persistenz-Warnung schicken; A/B-Vergleiche der Workarounds; Essay ins ZIP.

### 1. Umsetzung (Captain-autorsiert, nicht Meeseeks)
- `src/core_sim.h` + `src/core_sim.c` (575 Zeilen): 16.16 Fixed-Point, 50Hz `core_tick`, statische Ringbuffer (64 Projektile/32 Gegner/128 Partikel), LCG-PRNG, 8-Richtungs-Tabelle ohne Trigo. Waffen: Spread (1–7 Pellets), Laser (L1–5, Pierce ab L3), Bounce 2.0 (L1 45°-Zwang, L2 +0°, L3 +90°, L4 Split, L5 Smart-Ricochet), Surround-Beam, Morph-Rad, XP/Level-Up.
- `src/game_main.c` (334 Zeilen): Terminal-Frontend (termios raw, 50Hz nanosleep-Pacing, ASCII-Renderer 72×30, HUD) + `--demo N [--seed S]` deterministischer Bot-Modus für Headless-Runs.
- `verify_core_sim.py`: 1:1-Python-Oracle mit Determinismus- + Invarianten-Checks.
- `WORKFLOW_ESSAY.md`: A/B-Vergleiche, Sackgassen-Liste, bindendes Protokoll (s. Datei).

### 2. Meeseeks-Einsätze (kurz, mit Warn-Kopfblock)
- **#2 (Review, Hintergrund):** sah den frischen Code NICHT (Snapshot-Isolation!) — lieferte aber: Verifizierung der Abwesenheit, Review-Checkliste, und den Math-Find **BOUNCE_DIAG_FP: 278000→278046** (6·cos45°·65536=278045.69; korrigierte sogar meine Aufgaben-Mathematik). Fix in C + Oracle angewendet.
- **#3 (Persistenz-Probe, Hintergrund):** schrieb 5 Dateien (inkl. extension-less) in sein Verzeichnis → in meiner Sandbox **0 sichtbar**; Harness meldete "3 persisted". → Subagent-Datei-Isolation endgültig bestätigt.

### 3. Persistenz-Matrix (gemessen, Iteration 3 — wichtiger Fund)
- persistiert: `.py .c .md .txt .zip .sh` · **flüchtig: `.h Makefile .wav .strudel`** (create_file UND bash-cp).
- `core_sim.h` verschwand erneut, kurz nach der Erstellung; `.c` überlebte.
- **Lösung:** `smith_bootstrap.py` (persistiert, da .py) erzeugt jetzt alle flüchtigen Dateien aus String-Konstanten + SHA256-Check + `--zip`. Neue Recovery-Kette: Original-Bootstrap (Audio-Kit) + smith_bootstrap (Core/Makefile/Samples) + `--zip`.

### 4. Headless-Tests (selbst ausgeführt, Oracle)
- 2500 Ticks/Seed 1234: tick=802 state=GAMEOVER wave=2, score=1000, kills=7, bounces=85, level=2 — Game-Loop, Kills, Bounce-Logik, Level-Up, Game-Over **funktionieren**.
- Determinismus: OK (2. Run bit-identisch) · Invarianten: OK · 4 Seeds robust · 10000-Tick-Run stabil.
- Gefangene Bugs (alle im Oracle, C-Logik korrekt): (a) `bot_input` None/−1-Mixup, (b) Float-Konstanten `0.5*65536=32768.0` bricht `>>16` → `int()`, (c) BOUNCE_DIAG-Präzision (via Review).

### 5. Was lief gut / Lessons Learned
- **Gut:** Captain-autorsiert + Oracle-Verifikation = zuverlässiger als jegliche Delegation für Dateien; Hintergrund-Reviews parallel zum Build = Zeit gespart; strikte 15-Zeilen-Report-Limits = sauberer Kontext; Trace-first + Determinismus-Checks fangen Bugs ohne Compiler.
- **Stolpersteine (für zukünftige Läufe):** (1) Nie apt/gcc installieren (kein Netz). (2) Nie `.h/Makefile/.wav` per create_file/cp "einmal" anlegen — immer in smith_bootstrap.py. (3) Subagenten sehen Snapshots, nicht Live-State — Reviews frischen Codes inline per Prompt. (4) `grep -c $'^I'` unzuverlässig, `cat -A` ist der Tab-Goldstandard. (5) Trigo-Konstanten nie auf "schöne Zahlen" runden.
- **Offen:** C-Build (`make game`) bleibt auf echte Maschine verwiesen — der Oracle belegt die Logik, nicht den Compile. Raylib-Frontend, Audio-Kopplung (`cyber_paula_update_from_game`) und EUCLID-Feinschliff stehen in todo.md.

### 6. Lieferung
- `STRUDELFORCE_Smith_Core.zip` (neu gepackt via smith_bootstrap.py --zip) mit: C-Code (Core + Frontend + Audio), Makefile (game/test_game/headless_test), WORKFLOW_ESSAY.md, beide Python-Oracles, Samples, Tracks, Steuerdateien.
- **Kompilieren (echte Maschine):** `make game && ./strudelforce_game` (WASD/Z/X/C, `--demo 2500` für Headless) · `make headless_test && ./strudelforce_test` (Audio-Parser).

---

## Captain's Log — Iteration 4 (Meilenstein 3: reaktive Audio-Kopplung + WAV-Dump)

**Auftrag (User):** Weiterarbeiten; IMMER alles im ZIP liefern mit Essay (Learnings, A/B-Vergleiche, gut dokumentiert für weiterführende Modelle).

### 1. Umsetzung (Captain-autorsiert)
- `src/game_audio.h/.c` (225 Zeilen): 8-Stimmen-Mixer, 50Hz-Macro-Tick (PWM/Arp/Decay), Bassline-Sequencer (C-Moll, Biom 1), SFX per Counter-Diff, 1-Pole-Low-Pass (16.16) + 4-Bit-Quantisizer. Reaktive Matrix exakt nach AGENT_2-Spec: Laser→PWM-Saw-Bass, Bounce→FM-Pluck+Oktav-Arp, Schild→PWM-Speed 2..6, HP<25%→Cutoff 550Hz+Quant.
- `src/game_main.c` +Audio: `--demo N --audio out.wav` dump einen 16-Bit-WAV (16574 Hz Mono, RIFF-Header 2-Pass-Write). **Core unverändert** (SHA256 `ecf32163` — Read-only-Diff-Kopplung).
- `verify_audio_matrix.py`: Oracle-Modul, importiert `verify_core_sim` (kein Duplikat), prüft Matrix gegen echten Core-Run.
- Makefile (via smith_bootstrap.py): `game` kompiliert jetzt `game_main.c + core_sim.c + game_audio.c` mit `-lm`.

### 2. Headless-Verifikation (selbst ausgeführt)
- Matrix-Oracle 2500 Ticks/Seed 1234: **matrix_check=OK** — weapon-coverage spread=250/laser=2000/bounce=250 (alle 3 Waffen im Run), lowhp_ticks=1729 (Cutoff-Phase erreicht), pwm 2..4, sfx-retriggers shots=76/kills=7/bounces=68.
- Float-Audit: core_sim.c = 0 Treffer; game_audio.c = nur Note→Frequenz + Wave-Init (dokumentierte Exception, konsistent mit tfmx_macro_synth.h).
- malloc-Audit: beide Module 0 Treffer (nur Kommentare).

### 3. Was lief gut / Lessons Learned
- **Read-only-Diff-Kopplung** = Standard-Muster für alle Module (Grafik/UI/HW): Core bleibt SHA-stabil, Module sind einzeln testbar.
- **`--audio`-WAV-Dump** macht den C-Build auf echter Maschine hörbar + diff-bar (deterministischer Seed).
- **Makefile-Regel:** Änderungen NUR in `smith_bootstrap.py` pflegen (Datei ist flüchtig + wird bei Recovery überschrieben) — in dieser Iteration einmal fast vergessen, Protokoll nachgezogen.
- **A/B (Audio im Core vs. separates Modul):** separates Modul gewinnt in Kopplung, Testbarkeit und Amiga-Backport (Essay §7.3).
- **Stolperstein:** SHA-Baselines nach bewusster Änderung sofort setzen (sonst ABWEICHUNG-Alarm bei Recovery).

### 4. Lieferung
- `STRUDELFORCE_Smith_Core.zip` (via smith_bootstrap.py --zip) mit: Core+Frontend+Audio C-Code, Makefile, 3 Python-Oracles, WORKFLOW_ESSAY.md (inkl. §7 Iteration 4), Samples, Tracks, Steuerdateien.
- **Kompilieren (echte Maschine):** `make game && ./strudelforce_game --demo 2500 --audio out.wav` (hörbare Verifikation) · interaktiv: `./strudelforce_game` (WASD/Z/X/C).
- **Nächster Schritt (Iteration 5):** Meilenstein 4 — Voxel-Renderer (AGENT_3) ODER ESC-Live-Coding (AGENT_4); s. handoff.md §5.



---

## Captain's Log — Handoff an KIWI (Claude)

**Handoff an KIWI (Claude):** Nach initialer lokaler Entwicklung und TFMX-Import-Tests durch Qwen und Gemini 3.8 Flash in VS Code wird der Workspace nun an Claude übergeben. Lokale C-Tests wurden in `local_c_test_report.txt` gesichert.

---

## Captain's Log — v3 KIWI-Run (Claude) + Zweig-Integration (2026-10-27)

**Auftrag (User):** Recovery des abgebrochenen KIWI-Runs (Token-Limit), Integritätsprüfung aller Extrakte + ZIP, anschließend die beiden Zweige (Smith/Schwarm) plus lokaler Root zu EINEM Projekt vereinen — der beste Code aus allen Quellen.

### 1. Recovery & Integrität (KIWI v3)
- `STRUDELFORCE_v3.zip`: intakt (149 Dateien + MANIFEST sha256, `kiwi-manifest/1`).
- Alle 47 extrahierten Dateien byte-identisch mit der ZIP; nur 2 Post-ZIP-Zusätze (`KIWI_PERSISTENCE_GUIDE.md`, `c_parse_check.py`). 4 `(1)`-Duplikate byte-identisch → verworfen.
- 39 im Workspace fehlende Dateien (v2-Audio-Stack, 9 Tools, levels, docs/img, tests, 3 Strudel-Tracks, .vscode) → aus ZIP restauriert; 4 Quellen auf v3 aktualisiert (`tfmx_player` v2, `game_audio` mit `ext_render`-Hook).

### 2. Zweig-Integration (A Schwarm + B Smith + lokale Root → ein Projekt)
- **Dual-Core:** `core_sim.c` (Smith, kanonisch für game/gui) und `core_sim_sf.c` (Schwarm, renamed; `sf_interfaces`-API für pc/amiga/test_phase1). Beide definieren `core_init`/`core_tick` → Link-Regel: niemals in einem Binary (das alte `test_phase1`-Target hatte nur durch Strukturlayout-Pech "bestanden"!).
- Live-Code: `livecode_ui.c/.h` (Smith, Zyklus-Swap) + `livecode_ui_sf.c` (Schwarm-API).
- Alle A-spezifischen Module (voxel, cyber_paula, biomes, amiga_hw, main_pc) und B-spezifische (Oracles, WORKFLOW_ESSAY, BUILD, todo) in die Root; Original-Zweig-Ordner intakt unter `archive/branches/`.
- Neue Referenz: `MERGE_NOTES.md` (Provenance + Verifikation).

### 3. Bugfixes im Zuge des Merges
- Makefile (via `tools/gen_sources.py`): `test_phase1` + `AMIGA_SRCS` verlinkten den falschen Kern → korrigiert; neue Targets (`intro`, `strudel_export`, `pc_game`, `headless_test`).
- `tools/selftest.py`: MOD-Loop-Test 6s → 9s (Song endet ~7,7s) — der eine Fehltest des KIWI-Runs ist behoben.
- `strudel_cleanroom.h`: striktes C89 (Deklarations-Hoisting) — betrifft 11 Translation Units inkl. Amiga-Build.
- MSVC-SDK: `src/compat/`-Shims (dirent/unistd) für das sf-Test-Target; `build_msvc.bat` von 2 auf 5 Targets erweitert.
- `kiwi_handoff.py`: `archive/` + `TFMX Professional/` in `SKIP_DIRS` (gehören nicht ins Handoff-ZIP).

### 4. Verifikation (alles lokal, Windows/MSVC + Python 3.13)
- 4 Smith-Oracles: OK (Determinismus, Invarianten, Audio-Matrix, Live-Code).
- `tools/selftest.py`: **33/33** (inkl. MOD-Fix, 1200 Rogue-Runs).
- `tfmx_oracle`-Scan: **396/396** Module OK. `level_tool`: 12 Chunks / 5 Level / 0 Errors.
- MSVC-Build: 4/5 Targets grün (game, test_audio_mix, test_phase1, headless); gui-Code kompiliert, .exe-Schreibzugriff durch laufende Alt-Instanz blockiert (LNK1104).
- **Phase-1: 43/43 PASS mit korrektem sf-Kern.** Game-Demo bit-identisch zum Pre-Merge-Run (score=400 kills=4 bounces=79). **c_vs_oracle: C == Python bit-genau (8/8, diffs=0).**

### 5. Lessons Learned
1. C-Linker prüft keine Signaturen: zwei `core_init`-Varianten "linken" fehlerfrei → Kern-Zuordnung je Target immer dokumentieren.
2. Die ZIP ist das einzige verlässliche Handoff — bestätigt: 100% der Daten überlebten in der ZIP; die Einzel-Extrakte brachten nichts Neues (außer 2 Post-ZIP-Dokumenten).
3. `gen_sources.py` als Single-Source-of-Truth für "flüchtige" Dateien (Header/Makefile/Strudel) hat sich bewährt.
4. `c_parse_check.py` (Mini-Präprozessor + pycparser) = guter Smoke-Test, ersetzt aber keinen echten Compiler (dokumentierte False Positives: System-Header, function-like Makros).
5. Laufende Alt-Exes blockieren Rebuilds (LNK1104) — vor Merge-Builds `tasklist` nach `strudelforce_*` prüfen.

### 6. Offene Punkte
- `strudelforce_gui.exe` neu bauen, sobald die laufende Alt-Instanz beendet ist (Code verifiziert).
- `main_pc.c` (Raylib) lokal nicht kompilierbar (kein Raylib) — Target `pc_game` mit `RAYLIB_DIR`.
- `ClaudeRun/`-Ordner stand bei Abschluss noch unter Dateisperre → bei Freigabe nach `archive/` verschieben.
- `ROADMAP_AUDIO.md` / `LOCAL_ONLY_TESTS.md` aus dem KIWI-Run blieben ungeschrieben (nur das Persistence-Guide wurde vollständig).
