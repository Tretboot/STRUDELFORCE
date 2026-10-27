# AGENT 4 SPEC: In-Game Live-Coding Console (`ESC`) & Biome (`src/livecode_ui.c`)

## 1. Das `ESC -> SOUND / MUSIK` Live-Coding Terminal
Wenn der Spieler `ESC` drückt und den Reiter **SOUND / MUSIK** wählt:
- Zeige den aktuell laufenden **Strudel-Pattern-Code** in einem editierbaren Retro-Textpuffer (grüne oder bernsteinfarbene Amiga-Topaz-Schrift auf halbtransparentem Raster-Hintergrund, sodass man dahinter das Voxel-Spiel sieht!).
- **Tastenkombinationen im Editor:**
  - `CTRL + ENTER`: Kompiliert den Textpuffer sofort über `strudel_parse_pattern()` und tauscht das Pattern nahtlos beim nächsten Zyklus-Schlag aus (Live-Coding ohne Audio-Aussetzer!).
  - `F2`: Speichert das aktuelle Setup als `.strudel` Textdatei.
  - `F3` ("Bring Your Own Soundfile"): Öffnet einen Dateiwähler für `.strudel`, `.mod` oder `.tfmx` (`mdat`/`smpl`) Dateien aus dem Ordner `user_music/`.

## 2. Die 3 Haupt-Biome + Endlos-Generator
1. **Biom 1: "Chrome Tempest" (Oberwelt-Ruinen):** Metallisch-blaue Voxel, schräger Voxel-Regen, Blitz-Lichtquellen, treibender C-Moll Synth-Bass.
2. **Biom 2: "The Great Bath" (Unterwasser-Kathedrale):** Türkis-blaue Lichtbrechung, gedämpfte Gravitation, versteckte goldene Voxel-Schatztruhe. Öffnet man sie, schaltet der Soundtrack auf das berühmte *Great Bath*-Tribute-Pattern um!
3. **Biom 3: "Mechanical Hive" (Alien-Kern):** Pulsierende rot-goldene Voxel-Wände, enge Röhren für das Morph-Rad.
