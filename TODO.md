# STRUDELFORCE — TODO.md (Stand 2026-10-29, nach Aufräum-Runde)

> Legende: `[ ]` offen · `[~]` begonnen · `[x]` erledigt. Gesetze & Architektur: `AGENTS.md`, `PROJECT_BIBLE.md`.
> Historie: `captains_log.md`, `docs/todo_smith.md`. Handoff-/Kiwi-Altlasten: `docs/kiwi/`, `tools/kiwi/`.

## 0. Projekt-Konventionen (neu)
- **Temporäres gehört nach `tmp/`** (gitignored): Frame-Dumps (`strudelforce.exe --dump` schreibt jetzt nach `tmp/dump/`), `.obj` (`build_msvc.bat` → `tmp/obj/`), Scratch. Nie im Root.
- **Referenzbilder** (Turrican-Screens, `wug-test.png`) liegen in `ref/` (nur lokal, gitignored).
- **Release = `python tools/make_release.py`**: Source-ZIP per `git archive` (Pfade mit `export-ignore` in `.gitattributes` bleiben draußen: Handoff-Tools, Kiwi-Doku, Testdaten, Audio-Referenzen, Captain's Log), Windows-ZIP, Amiga-ZIP, `SHA256SUMS.txt`. **Nie** `TFMX Professional/`, `MOD/`, `Kickstart/` mitliefern.
- `tools/gen_sources.py` ist **veraltet** — nicht ausführen (überschreibt Shim-Header, bricht ab). Zu bereinigen (siehe 6.).
- CI: eine Datei `.github/workflows/build.yml` (windows, linux, amiga, release bei Tag `v*`).

## 1. P0 — Amiga-Port im Emulator zum Laufen bringen
Stand: `amiga/` baut (`make -C amiga` → `amiga/out/a.exe`, 57 KB). `tools/build_amiga_adf.py` packt das **echte** Binary. **Noch nie auf Emulator/Hardware gelaufen.**
- [x] Ursache „nur Workbench“ im RetroArch gefunden: `strudelforce.amiga` auf der ADF war ein 40-Byte-Stub (`moveq #0,d0; rts`), und die startup-sequence nutzte `echo`/`run` ohne `C:` auf der Disk. → Stub entfernt, startup-sequence = nur `strudelforce.amiga`.
- [ ] **Neue ADF testen** (`python tools/build_amiga_adf.py`; RetroArch/PUAE mit Kickstart 1.3 A500, `tools/retroarch_test.bat`). Erwartung: Himmel-Verlauf, Boden, Spieler, HUD, Joystick-Port 2.
- [ ] Falls weiter Workbench: (a) Kickstart ≥2.0 in PUAE gewählt? ROM-Pfad `Kickstart/kick34005.A500` (1.3) verwenden; (b) KS 1.3 führt startup-sequence evtl. nur mit `C:Execute`/`C:Run` aus → eigenen **Bootblock** schreiben, der das Programm direkt startet (xdftool `boot install` setzt nur den Standard-Bootblock); (c) Alternative: `exe2adf.exe` (liegt in der Extension) verwenden.
- [ ] **VS-Code-Debug-Flow:** F5 *Amiga 500* (WinUAE via Extension, `.vscode/launch.json`, `amiga.program = amiga/out/a.exe`). Task `compile` ruft `make program=…` (Root-Makefile delegiert an `amiga/`).
- [ ] Framerate messen (68000 @ 7 MHz, aktuell CPU-Rects, 1 `core_tick`/VBlank, Poll statt Interrupt): Bei Unterschreiten 50 fps → VBlank-Interrupt + Tick-Zähler, Blitter für Fills/Bobs, Dirty-Rects.
- [ ] Eingabe: Tastatur (CIA-SDR) zusätzlich zum Joystick; Waffenwechsel (`INPUT_WEAPSW`), Morph, Pause, Menü/Titelscreen.
- [ ] Alt-Code **löschen/archivieren**: `src/amiga_hw.c`, `core_sim_sf.c`, `voxel_render.c`, `biomes.c`, `livecode_ui_sf.c`, `sf_interfaces.h`, `main_pc.c` (Schwarm-Kern; falsche Register, Typ-Konflikt mit `strudelforce.h`). Vorher prüfen, was `test_suite`/`Makefile` noch referenziert.
- [ ] `src/amiga_music.c` (INCBIN-Länge nicht konstant) fixen **oder** ersetzen; Musik auf dem Amiga: TFMX-Streaming (`tfmx_player.c` kompiliert unter m68k) oder P61 (Player liegt im Bartman-Template). Aktuell enthält die ADF `music/` (178 KB), das Programm nutzt sie noch nicht.
- [ ] M68K-Audio-SFX (Paula-Kanal 3, Chip-RAM-Samples), 7-kHz-Filter (CIA-A `$BFE001` Bit 1) beim Great-Bath-Truhen-Tribute.
- [ ] Chip-RAM-Budget dokumentieren (aktuell 2×5×10 KB Bitplanes + Copper); Ziel: 512 KB A500.
- [ ] CI-Amiga-Job **verifizieren**: `build.yml` holt die Toolchain aus der `.vsix` von `BartmanAbyss/vscode-amiga-debug` (Asset-Name/Linux-Pfad ungeprüft!). Beim ersten Lauf Log prüfen; ggf. `--repo` auf den Architect-Fork ändern oder Toolchain als Release-Asset ins eigene Repo legen.

## 2. P0 — Sprites (Amiga + PC)
Ziel: eine gemeinsame Sprite-Datenbasis (`src/sprite_data.h`/`turrican_sprites.h` vorhanden, 16-Farben-Matrizen), zwei Renderer.
- [ ] **Format festlegen:** indizierte Matrizen (4–5 Bit), Maskenplane; Generator `tools/gen_sprites.py` (PNG/ASCII → C-Arrays **und** Amiga-Planar-Daten). Keine Texturen (Gesetz 2): Plastizität nur durch Face-Shading-Farbstufen.
- [ ] **Amiga:** Blitter-Bobs (Cookie-Cut, Minterm `0xCA`) auf den 5 Bitplanes; Hardware-Sprites nur Spieler + Laser-Kern (8 Kanäle, 16 px breit, 3 Farben + attach). Doppelpuffer + Hintergrundrestore bzw. Komplett-Redraw.
- [ ] **PC:** gleiche Daten, Upscale + Glow/Bloom, Partikel (GDI-Pfad in `gui_main.c`).
- [ ] Sprite-Satz: Spieler (Lauf 6 Frames, Sprung, Morph-Ball, Tod), Walker, Shooter, Tank, **Wug**, Pickups/Kristalle, Truhe, Boss-Platzhalter.
- [ ] Animations-Tabelle (Frame-Dauer in Ticks), deterministisch (nur aus `tick`/Zustand, nie RNG im Renderer).

## 3. P0 — Gegner „WUG“ (Entwurf: `docs/WUG.md`)
Vogel auf zwei Beinen (`ref/wug-test.png`): watschelt → bleibt stehen → rot glühende Augen → riesiger Laser; Treffer in der Aufladephase bricht ab.
- [ ] `ENEMY_WUG` + Zustandsautomat im Kern (`game_logic.c`), Python-Oracle-Spiegel (`verify_core_sim.py`), Tests (`test_suite.c`), Balancing, Spawn ab Welle 2 / Level 2.
- [ ] Sprites (2.), Laser-Effekt (4.), Audio-Telegraph (Charge-Ton) & Fire-SFX.

## 4. P0 — Waffen-Sprites & Effekte überarbeiten
- [ ] Pellet, Bounce 2.0, Laser (Breite nach Waffenlevel `sf_laser_widths`), Spread, Halt-Strahl: je eigene Optik, Muzzle-Flash, Einschlag, Explosionen, Funken/Partikel.
- [ ] **Amiga-tauglich:** max. ~8 Bobs + wenige Fill-Linien pro Frame; Effekte über Palette/Copper (Blitzen, Raster-Balken) statt vieler Sprites.
- [ ] **PC-Zusatz:** Additive Glows, Screen-Shake, Trails (nur Renderer, Kern bleibt bit-identisch).

## 5. P1 — Abspann mit komplettem Captain's Log
- [ ] `tools/gen_embedded_credits.py` erweitern: **gesamtes** `captains_log.md` (ungekürzt, aktueller Release-Stand) als Credit-Zeilen einbetten (`src/embedded_credits.h`, derzeit 103 Zeilen, Titel noch 0.3.0). Größe ~50 KB → String-Tabelle, ggf. Zeilenumbruch bei 78 Zeichen, UTF-8→CP437/ASCII-Fallback (Umlaute!).
- [ ] Abspann-Steuerung (ESC/SPACE/Tempo) existiert; Kapitel-Sprungmarken pro Log-Eintrag ergänzen.
- [ ] Log vor jedem Release finalisieren; `captains_log.md` ist `export-ignore` (nur im Spiel eingebettet, nicht doppelt im Source-ZIP).
- [ ] Log-Datei hat einige Kodierungsfehler (`ǬŸ`, `�`) aus früheren Läufen → vor dem Einbetten als UTF-8 bereinigen.

## 6. P1 — Weiteres Aufräumen
- [ ] `tools/` weiter ordnen: Kiwi-/Handoff-Skripte (`build_*injector.py`, `build_handoff.py`, `build_kiwi_handoff.py`, `hotfix_packer.py`, `kiwi_*.py`, `github_exporter.py`) nach `tools/kiwi/` verschieben **und** deren `ROOT = dirname(dirname(__file__))` anpassen; `AGENTS.md`-Verweise nachziehen.
- [ ] `tools/gen_sources.py` entrümpeln oder löschen (Shim-Header-Generatoren sind obsolet; Makefile/.vscode werden von Hand gepflegt).
- [ ] `README.md`/`BUILD_GUIDE.md`/`PROJECT_BIBLE.md` auf neue Struktur (amiga/, tmp/, make_release.py, build.yml) aktualisieren; `docs/kiwi_phase3/` (14 Alt-Berichte) zusammenfassen oder löschen.
- [ ] `docs/img/` (PNG-Level-Previews) nur bei Bedarf regenerieren; `levels/levels_kiwi_extended.json`, `assets/samples/*.wav` auf Nutzen prüfen.
- [ ] Alt-Releasezips in `archive/handoffs/` (lokal, ~45 MB) nach Sichtung löschen; `archive/KiwiReste/` ebenso.
- [ ] Alte Hinweise in `captains_log.md` auf entfernte Dateien (`pack_releases.py`, `kiwi_injector.py`) sind historisch — nicht ändern.

## 7. P0 (aus Vorgänger) — Level-Design Overhaul
- [ ] Aktuelles Level-Gerüst ist unbrauchbar (Plattformen existieren im Kern, aber wenig Vertikalität/Durchgänge). Von Grund auf: echte Kollision, Plattformen, Vertikalität; handgebaute Hommage-Levels (Turrican, Giana Sisters, Mario) + deterministischer Roguelike-Modus. Level-Engine v2 (197 Chunks, 37 Levels: `levels/levels_v2.json`, `levelkit/lv2.py`) liegt vor und muss an Kern + Amiga-Renderer angebunden werden (Amiga zeichnet derzeit nur die ~10 `platforms[]` des Kerns).
- [ ] Balancing erst nach dem Level-Fundament (Grunt 1 Treffer, Tank 15 HP bleibt Baseline).
- [ ] Turrican-2-Visual-Upgrade (Renderer, PC): Plattformen mit Kollision zeigen, Schächte/Portale, HUD-Runde 2, Level-Vignetten (Copper-Farbtemperatur).

## 8. P1 — Voxel-Evolution (Konzept)
- [ ] Spieler wächst modular aus Bauteilen (Beine → Sprung, Arme → Angriffe, Waffen-Module). Statische Strukturen, 16.16, Pickup-System-Anbindung, Amiga = Pixel-Cluster (Gesetz 2).

## 9. P2 — Audio
- [ ] Original-Soundtrack für „STRUDELFORCE MASTER“ (Titel, Biome, Boss); Original-Samples der Klassiker extrahieren; TFMX-Player-Defekte (Wait-States, DMA-Makros) reparieren; `tools/tfmx_oracle.py` als Wächter.
- [ ] 15-Sample-SoundTracker-MOD-Support (3 Lost-Patrol-MODs) — C **und** `tools/mod_oracle.py` gemeinsam.
- [ ] Biquad/SVF-Filter in Fixed-Point (aktuell 1-Pol).

## 10. Release-Checkliste
1. `build_msvc.bat` · `test_suite.exe` · `verify_core_sim.py` (Endlos + `--campaign`) · `tools/selftest.py` grün.
2. `make -C amiga` + `python tools/build_amiga_adf.py` (ADF im Emulator gestartet!).
3. Version in `src/strudelforce.h` (`STRUDELFORCE_VERSION`), `build_msvc.bat`, README; Notes `releases/RELEASE_NOTES_v<VER>.md`.
4. Captain's Log final (Abspann, 5.).
5. Commit, Tag `v<VER>`, Push → CI baut & veröffentlicht (`tools/make_release.py` lokal als Trockenlauf).
