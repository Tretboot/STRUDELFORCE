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
Stand: `amiga/` baut (`make -C amiga` → `amiga/out/a.exe`, 57 KB). `tools/build_amiga_adf.py` erzeugt voll bootfaehiges OFS-Floppy-Image (`strudelforce.adf`). **Erfolgreich in RetroArch PUAE (A500 / Kickstart 1.3) getestet & per Screenshot verifiziert!**
- [x] Ursache „nur Workbench“ im RetroArch gefunden: `strudelforce.amiga` auf der ADF war ein 40-Byte-Stub (`moveq #0,d0; rts`), und die startup-sequence nutzte `echo`/`run` ohne `C:` auf der Disk. → Stub entfernt, startup-sequence = nur `strudelforce.amiga`.
- [x] **ADF-Dateisystem & Bootblock gefixt:** `tools/build_amiga_adf.py` nutzte fälschlicherweise FFS (`DOS1:ffs`) und `boot2x3x.bin` (prüft auf KS 2.0+). Kickstart 1.3 unterstützt auf Floppy nur OFS (`DOS0:ofs`) und benötigt `boot1x`. Auf `ofs` + `boot1x` umgestellt.
- [x] **Emulator-Test verifiziert:** `strudelforce.adf` bootet in RetroArch (PUAE A500 Kickstart 1.3) fehlerfrei durch: Himmel-Verlauf (Copper), Ruinen-Säulen, Boden, Spieler-Sprite und HUD (Score, HP, LV, Wave 0) werden korrekt gerendert.
- [x] **RetroArch-Testskripte optimiert:** `tools/retroarch_test.bat` nutzt nun `--appendconfig` statt `--config` mit `tools/retroarch_amiga.cfg`, damit Core- und Treibereinstellungen nicht überschrieben werden.
- [x] **Amiga-Parity (v0.3.1): State Machine, Intro, Menue & Loading:** `amiga/main.c` vollstaendig auf State-Machine (`AMI_STATE_INTRO` -> `AMI_STATE_MENU` -> `AMI_STATE_LOADING` -> `AMI_STATE_GAME`) umgestellt.
  - **Cracktro-Intro:** 3D-Wireframe-Vektor-Wuerfel (100% Integer-Fixed-Point-Rotation), Copper-Gradient-Horizont und Sinus-Scroller.
  - **Turrican-2 Hauptmenue:** 2-Ton-Titel (Gold ueber Dunkelrot), Modus-Wahl (Kampagne mit 5 Welten vs. Endlos Roguelike), Steuerung und Credits.
  - **Ladebildschirm:** Authentischer Amiga-Block-Ladebalken mit wandernden Lichtblenden und Hardware-Statusmeldungen (Chip-RAM, Copper, Biome, Paula).
- [x] **Paula Audio-Engine (Amiga-Bare-Metal):** ThePlayer 6.1a (`player610.6.no_cia.bin` + `testmod.p61`) direkt in den M68K-Build eingebunden. 4-Kanal Paula Hardware-DMA Musik laeuft auf VBlank ohne messbare CPU-Last.
- [x] **WinUAE Authentische Hardware-Emulation:** Diskettenlaufwerksgeraeusche aktiviert (`floppy0sound=1`, Lautstaerke 100%), WinUAE-Sound-Samples nach `Amiga Files\WinUAE\plugins\floppysounds\` extrahiert, 1-Klick-Starter `tools/start_winuae.bat` und Konfig `tools/winuae_a500.uae` bereitgestellt.
- [x] **Eingabe-Parität (Tastatur + Joystick + Maus):** Vollständige CIA-A Tastaturabfrage (`0xBFEC01` SDR + `0xBFEE01` CRA Handshake-Puls). Pfeiltasten, WASD, Leertaste, Enter, Return, Ctrl, Alt und ESC nahtlos parallel zu Joystick Port 2 und Port 1 LMB angebunden. Kein versehentliches Beenden bei Linksklick mehr.
- [x] **AmigaOS StrudelOS-Boot-Banner:** Floppy-Volume auf `StrudelOS` formatiert, farbiger ANSI-Startbanner via `dos.library` `Write(Output())` vor dem Custom-Chipset-Takeover.
- [x] **Tools-Aufräumung & Strukturierung:** Hauptverzeichnis `tools/` bereinigt (nur Starter, Konfig und Core-Packer); Sekundärskripte in `tools/analysis/`, `tools/audio/`, `tools/generators/`, `tools/kiwi/`. Versionierte ADFs nach `amiga/builds/`.
- [x] **Kuratierter 10-Track Soundtrack (~335 KB):** Vollständige Audio-Parität zwischen Amiga und PC (Turrican 3 World 1, BiFi Adventure, Cybersty, Mad TV, Software Manager mit 22 Subsongs, Chiptune- & Cracktro-MODs).
- [ ] **VS-Code-Debug-Flow:** F5 *Amiga 500* (WinUAE via Extension, `.vscode/launch.json`, `amiga.program = amiga/out/a.exe`). Task `compile` ruft `make program=…` (Root-Makefile delegiert an `amiga/`).
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
- [x] `ENEMY_WUG` + Zustandsautomat im Kern (`game_logic.c`), Python-Oracle-Spiegel (`verify_core_sim.py`), Tests (`test_suite.c`), Balancing, Spawn ab Welle 2 / Level 2.
- [x] Sprites (2.), Laser-Effekt (4.), Audio-Telegraph (Charge-Ton) & Fire-SFX.
- [x] **Amiga-Integration:** `render_story()` mit THE WUG Star Wars Crawl in `amiga/main.c`.
- [ ] **Amiga-Sound:** ThePlayer 6.1a ist implementiert, aber `#embed` Directiven funktionieren nur mit Bartman VS Code Extension, nicht mit GCC direkt. Als Workaround müsste `INCBIN` verwendet werden oder auf vereinfachte Audio (Single-PWM-Sweep für Laser) ausweichen.

## 4. P0 — Waffen-Sprites & Effekte überarbeiten
- [ ] Pellet, Bounce 2.0, Laser (Breite nach Waffenlevel `sf_laser_widths`), Spread, Halt-Strahl: je eigene Optik, Muzzle-Flash, Einschlag, Explosionen, Funken/Partikel.
- [ ] **Amiga-tauglich:** max. ~8 Bobs + wenige Fill-Linien pro Frame; Effekte über Palette/Copper (Blitzen, Raster-Balken) statt vieler Sprites.
- [ ] **PC-Zusatz:** Additive Glows, Screen-Shake, Trails (nur Renderer, Kern bleibt bit-identisch).

## 5. P1 -- Abspann mit komplettem Captain's Log
- [x] 	ools/generators/gen_embedded_credits.py erweitert: **gesamtes** captains_log.md (ungekuerzt, Stand v0.3.1) als Credit-Zeilen eingebettet (src/embedded_credits.h, 1446 Zeilen, inkl. Kapitel V Banner).
- [x] Abspann-Steuerung mit automatischem Musik- und Subsong-Wechsel bei Start des Captain's Log (Chris Huelsbeck Software Manager Subsong 21) und visuellen Kapitel-Headern (--- [ ... ] ---).
- [x] Echter Star Wars 3D Perspektiv-Crawl (Scanline-Trapezoid Raycasting) in src/gui_main.c implementiert (Zero-Float, kontinuierliche Verjuengung, Horizont-Fade, Drop-Shadow).

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
