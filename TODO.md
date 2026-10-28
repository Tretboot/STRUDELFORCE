# STRUDELFORCE — TODO.md (KIWI-Handoff: Priorisierte Arbeitspakete)

> Stand: 2026-10-28 — Release **v0.1** (Versionskorrektur: war intern "v1.1", jetzt 0.1)
> Legende: `[ ]` offen · `[~]` aktiv · `[x]` erledigt
> Historie der Smith-Phase (Iteration 1–4): `docs/todo_smith.md` (nicht im Handoff-ZIP).
> Bindende Architektur-Grundlagen & die 3 Eisernen Gesetze: `AGENTS.md`.

## P0 — LEVEL-DESIGN OVERHAUL (Höchste Priorität für die KIWI-Phase)
- [ ] **Das aktuelle Level-Gerüst ist UNBRAUCHBAR.** Es erlaubt derzeit nur rudimentäres Rechts/Links-Movement und hat keine echte Spielstruktur.
- [ ] Level-Design **von Grund auf neu bauen**: echte Kollisionen, Plattformen, Vertikalität.
- [ ] Ziel: ein echtes, spielbares Fundament — **erst darauf** folgt das Balancing. (Die aktuelle 1-Hit-Grunt-/15-HP-Tank-Balance bleibt nur als Test-Baseline, wird final nicht fixiert, bis das Level-Fundament steht.)
- [ ] Im Rahmen des Neuaufbaus: handgebaute Hommage-Levels an Klassiker (Turrican, Giana Sisters, Super Mario) **und** ein prozeduraler Roguelike-Modus, der Level-Bausteine zufällig, aber logisch zusammensetzt (deterministischer Seed-RNG nach AGENTS.md).
- [ ] C-Datenstrukturen (Arrays/Structs) für Level/Kollision/Vertikalität vorbereiten — statisch vorallokiert, 16.16-Fixed-Point, null malloc.
- [~] Fundament v0.1 (Bestand, darauf aufbauen): Modus-Auswahl (Kampagne 5×3 Wellen mit festen Biomen vs. Endlos-Roguelike), `MODE_*` + `STATE_LEVEL_CLEAR`/`STATE_VICTORY` im Kern, `core_campaign_biome()`, Oracle-Spiegelung (`verify_core_sim.py --campaign`), Biome-Paletten im GUI-Render.

## P1 — NEUES GAMEPLAY-KONZEPT: DIE VOXEL-EVOLUTION (Konzept für die spätere Implementierung)
- [ ] **Konzept:** Der Spielercharakter wird modular aus Voxeln (PC) bzw. Pixel-Clustern (Amiga) zusammengesetzt.
- [ ] Der Spieler startet als kleines, rudimentäres Konstrukt und findet in der Welt **Bauteile** (Beine, Arme, Waffen).
- [ ] Durch das Einsammeln **wächst der Charakter physisch**, wird detaillierter und schaltet Funktionen frei (z. B. Beine → Sprung/Höhenversatz, Arme → neue Angriffe, Waffen-Module → weitere Waffenslots).
- [ ] Architektur-Vorbereitung: C-Datenstrukturen statisch (16.16-Fixed-Point, null malloc, 512 KB Chip-RAM-Budget), Bauteil-Spawns als Level-Features anbinden (→ P0), Amiga-Seite = Pixel-Cluster auf 6 Bitplanes (Gesetz 2: keine Texturen, nur Paletten + Face-Shading).
- [ ] Anbindung an bestehendes Pickup-System (`core_sim.c`, deterministische Spawns — C == Python-Oracle).

## P2 — AUDIO-STRATEGIE: DER ORIGINAL-SOUNDTRACK
- [ ] Die originalen TFMX-Module (Apidya, Turrican etc.) **nicht nur technisch** (Reverse Engineering), sondern auch **inhaltlich/musikalisch** analysieren: Song-Form, Stimm-Verbrauch, Sample-Zuordnung, Arrangement-Prinzipien.
- [ ] **Eigene Kompositionen schreiben: ein ORIGINAL-SOUNDTRACK für "STRUDELFORCE MASTER"** (Titel/Boot, Biome-Tunes, Boss-Kämpfe).
- [ ] Die **originalen Amiga-Samples der Klassiker extrahieren** (aus den beiliegenden `.smpl`/`.mdat`-Dateien) und sie in den neuen Kompositionen wiederverwenden.
- [ ] Vorher: TFMX-Player/Importer-Defekte reparieren (Byte-Offsets, Wait-States, DMA-Makros) — nur originalgetreues Playback erlaubt verlässliches Reverse Engineering.
- [ ] Ergebnis in den bestehenden Audio-Flow integrieren (Jukebox, TFMX/MOD-Library, Live-Code-Swap am Zyklus-Rand, `tools/tfmx_oracle.py` als Bit-Genauigkeits-Wächter).

## Erledigt in v0.1 (2026-10-28, lokal)
- [x] **Steuerung neu (GUI):** WASD + Pfeiltasten = bewegen, Leertaste = Sprung, **STRG = Feuer**, **ALT = Halt-Strahl**, C = Waffe, S = Morph, T = Truhe, ESC = Menü.
- [x] **Controller-Support (XInput):** Stick/D-Pad = bewegen, A = Feuer, X = Beam, B = Morph, RS = Waffe, Start = Menü; Menue-Navigation per D-Pad + A; Verbindungsanzeige im Menü-Footer.
- [x] **Kampagne vs. Endlos-Roguelike:** Modus-Auswahl im Hauptmenü; Kampagne = 5 Level × 3 Wellen, feste Biome-Reihenfolge (Chrome Tempest → Great Bath → Mech Hive → Void Reach → Core Furnace), Level-Clear-Boni, SIEG-Screen nach Level 5; Endlos = altes Verhalten **bit-identisch**.
- [x] **Biome-Paletten im Render:** Copper-Himmel, Säulen & Boden wechseln pro Kampagne-Level (reine Paletten, Gesetz 2); Wave-/Level-Intro-Banner im Gameplay.
- [x] Haupt-EXE: `strudelforce_v0.1.exe` (früher `strudelforce_gui.exe` → intern "v1.1" → korrigiert auf 0.1); Versionsnummer im Menü + Fenster-Titel.
- [x] **Intro mit Original-Samples:** Apidya Meadow (.tfx + .sam) ist **in die EXE gepackt** (`src/embedded_music.h`) und läuft als Intro/Menü-Musik — keine Disk-Dateien nötig (auch ADF-fähig).
- [x] **MOD-Ordner angebunden:** Jukebox scannt `TFMX Professional/` + `MOD/` (41/45 MODs spielbar); 3 × Lost_Patrol = nicht Standard-31-Sample-Format (kein M.K!/M!K!-Sig bei Offset 1080) → Player lehnt ab (bekannt, dokumentiert).
- [x] **Explorer mit freier Pfadeingabe:** beliebigen (auch absoluten) Pfad tippen + ENTER → Ordner wechseln oder .tfx/.mod direkt laden/spielen.
- [x] Jukebox-Fallback ohne Disk: 3 Embedded-Tracks (Meadow TFMX, WC Regular Combat, WC Overall Victory), rotierend pro Wave; Selftest `test_embedded_music.exe` (Build [7/7]).
- [x] KIWI-Return-Paket organisiert: `archive/kiwi_return_2026-10-28/`, Doku nach `docs/` (LORE, BIOMES, TFMX-Analyse), keine KIWI-C-Stubs im Build.

## Carry-over (offen aus den lokalen Phasen)
- [ ] 15-Sample-SoundTracker-MOD-Unterstützung (würde die 3 Lost-Patrol-Dateien spielbar machen) — C **und** `tools/mod_oracle.py` bit-genau gemeinsam ändern!
- [ ] M68K-Build mit `m68k-amiga-elf-gcc` + 880K-A500-ADF + RetroArch/PUAE-Test (`tools/retroarch_test.bat`) — Toolchain lokal nicht in PATH; Boot-Screen-Code in `amiga_hw.c` ist nur parse-geprüft, noch nicht auf der Zielmaschine kompiliert.
- [ ] Biquad/SVF-Filter in Fixed-Point (aktuell nur 1-Pole im Core).
- [ ] `main_pc.c` (Raylib) lokal nicht kompilierbar — Target `pc_game` mit `RAYLIB_DIR` setzen.
- [ ] Generatoren nicht handeditieren: `Makefile`, `src/tfmx_player.h`, `src/mod_player.h`, `src/sf_mixer.h`, `src/sid_header.h`, `src/level_data.h`, `.vscode/tasks.json` → Änderungen in `tools/gen_sources.py` bzw. `tools/level_tool.py`, dann regenerieren.
