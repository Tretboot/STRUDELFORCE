# LEVEL FORMAT v2 - STRUDELFORCE

## Architektur: bauen mit v1-Chunks, erweitern mit v2-Metadaten

`levels/levels.json` bleibt das **einzige Level-Format** (v1-List-Format,
16x12-ASCII-Chunks) - der v1-Export der alten 9 Levels ist unverändert erhalten.
v2 fügt **nebenbei** `levels/levels_v2.json` hinzu (Musik-Zonen, Notes,
Cutscene-Platzhalter, Kampagnen-Graph) - keine Breaking Change.

## Chunk-Format (v1, unverändert)
- 16 Spalten x 12 Zeilen, Zeile 0 = oben
- Legende: `.` air, `#` solid, `B` brick, `?` bonus, `-` oneway, `^` spike,
  `~` water, `H` ladder, `*` crystal, `C` chest, `P` spawn, `E` exit,
  `w` walker, `f` flyer, `t` turret, `U` powerup, `O` pipe, `M` metal,
  `=` conveyor, `o` owl, `D` diamond, `S` spring, `G` boss-core
- Exits: `exits:{l,r,u,d}` (9 = offen, 255 = zu), flags: start/chest/arena/end
- Level: `seq` = Chunk-IDs links nach rechts

## v2: Musik-Zonen (X-Bereiche)
`levels_v2.json` -> pro Level `music_zones: [[x0, x1, subsong], ...]`
- x in **Tiles** (0..level_w), Bereiche dürfen sich nicht überlappen
- subsong 0..3 (Platzhalter-Tracks; echte Musik später)
- C: `LvMusicZone LV_MZ_<i>[]` + `level_music_zone(idx, x, &sub, &active)`
- Regeln: sortiert, lückenlos empfohlen, Zone 0 startet bei 0

## C-Export (src/level_data.h.c, generiert von lv2.py)
- `LvChunk` / `LvLevel` unverändert (level_engine-kompatibel)
- NEU: `LvMusicZone`, `LV_MZONES[LV_NUM_LEVELS]`, `LV_MZONE_COUNT[]`
- Rogue-Assembler (Xorshift32) unverändert enthalten

## Physikalische Design-Limits (aus der echten C-Physik abgeleitet)
Reach-Oracle (GRAV 0.45, JUMP 2.6, MAX_SPEED 2.4, 50 Hz):
- **Apex 7.51 Units** -> kletterbare Mauern: **<= 6**
- **Sprungweite 27.7 Units** (volle Distanz)
- **Hazard-Run 25.8 Units** (über 1er-Spikes) -> Design-Limit: **<= 16**
- **Senken (Ditches): <= 6** ohne Boden (Bot: 2x 3er-Sprung, Weltboden fängt)
- Keine zwei Ditches/Pools hintereinander (Guard im Generator: `last`-Check)

## Generator (lv2.generate)
- Segmente: flat(28%), ditch(14%), spikes(16%), pool(12%), steps(12%), loot(18%)
- Difficulty-Rampe: Gegner-Dichte steigt mit x/W
- End-Zone: 10 Tiles clean, Exit bei W-4
- Truhe: max 1, Wand mit 2 freien Feldern drüber
- Musik-Zonen: 3-5 Bereiche, lückenlos

## Validierung (lv2.static_checks + Bot)
- Static: Zeilenlängen, Legende, Spawn/Exit, Lücke >10, Mauer >6,
  Hazard-Run >16, Zonen-Überlapp, >32 Gegner
- Bot (Greedy-Right + Sprung-Heuristik) = Spielbarkeits-Untergrenze:
  **Bot gewinnt -> Level ist ok.** 37/37 im aktuellen Stand.

## Editor
- Text-CLI (headless): `python3 levelkit/lv2.py edit --level N`
  (g=Grid, s=set, m=zone, t=Test, p=Preview, e=Export, q[y])
- pygame-GUI: offen (Kernlogik steht in lv2.py, GUI ist der nächste Schritt)
