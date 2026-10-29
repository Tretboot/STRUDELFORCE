# SLIM BOOTSTRAP - STRUDELFORCE Level-Task

## Was ist das?
Der `sf_levelkit_injector.py` ist der **slimme Injektor** für die
Level-/Sprite-Iteration. Er enthält NUR die Dateien, die für diesen Task
benötigt werden (<50, erprobte Persistenz-Grenze der Sandbox). Das komplette
Game (89 Dateien) bleibt auf der Festplatte / im alten `kiwi_injector_slim.py`.

## Neustart (frische Sandbox)
```
python3 sf_levelkit_injector.py            # entpackt nach ./strudelforce/
python3 sf_levelkit_injector.py --check    # nur Manifest-Prüfung
```
Dann:
```
cd strudelforce
python3 tools/hfile_guard.py guard .       # .h -> .h.c (Sandbox-Wipe-Schutz)
python3 levelkit/lv2.py selftest           # 37/37 erwartet
```

## NO-OVERWRITE-REGEL (wichtigste Lektion)
Der Injektor **überschreibt niemals** eine bestehende Datei:
- Existiert das Ziel (gleiche Größe + SHA) -> SKIP
- Existiert das Ziel (anderes) -> SKIP + CONFLICT-Meldung
- Nur fehlende Dateien werden geschrieben
Damit gehen lokale Änderungen nie durch einen Neustart verloren.

## Wipe-Regeln (aus Erfahrung, 4 Wipe-Ereignisse)
1. `.h`-Dateien werden systematisch gewiped -> immer `.h.c` (hfile_guard)
2. `create_file`/`edit_file`-Dateien überleben besser als Bash-geschriebene
3. Pro Bash-Call persistieren max ~50 Dateien -> Batches <= 20
4. Nach jedem Arbeitsschritt: Selftest + (wenn nötig) Snapshot
5. `levels.json` IMMER im v1-List-Format halten (Chunk-Liste mit
   {id, rows, exits, flags}) - lv2.py hängt an, ersetzt nie

## Dateiverzeichnis (16 Dateien)
| Datei | Zweck |
|---|---|
| levelkit/lv2.py | Level-Kit v2 (Modell/Physik/Bot/Gen/Export/CLI) |
| src/level_engine.c | C-Engine: Chunks -> Plattformen/Gegner/Musik |
| src/level_engine.h.c | Header (als .h.c gegen Wipe) |
| src/game_logic.c | Core-Logik MIT 3 level_engine-Patches |
| src/level_data.h.c | Generiert: 197 Chunks, 37 Levels, Zonen |
| src/strudelforce.h.c | Master-Header (unverändert) |
| levels/levels.json | v1-Format: 17 Origin + 180 v2-Chunks, 37 Levels |
| levels/levels_v2.json | v2-Metadaten (Musik-Zonen) |
| Makefile | MIT level_engine.c |
| tools/hfile_guard.py | .h/.h.c Warden |
| assets/tiles_v2.png | Tile-Palette-Preview |
| assets/preview_gen7.png | Level-Vorschau |
| ref/turrican2_*.png | 3 Sprite-Referenzen (Player/Enemies/Level) |
| KIWI_SURVIVAL_GUIDE.md | Sandbox-Überlebensregeln |
| captains_log.md | Iterations-Log |
| todo.md | Aktueller Stand |

## Offene Dokus (im alten Injektor, nicht hier)
PROJECT_BIBLE, BUILD_GUIDE, RELEASE_0.3.0, docs/* (LORE, BIOMES, MUSIC,
WORKFLOW_ESSAY), tools/* (level_tool.py, oracles), tests/data/*, assets/*
(Musik, TFMX, Story). Alles im `kiwi_injector_slim.py` (89 Dateien).
