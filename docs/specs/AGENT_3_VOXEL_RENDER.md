# AGENT 3 SPEC: Pseudo-Pixel 3D-Voxel & Parallax Renderer (`src/voxel_render.c`)

## Das visuelle Konzept ("Turrican in 2.5D Voxel")
- Wir laden KEINE schweren externen 3D-Modelle, sondern definieren Sprites und Tiles als kompakte **2D-Farb-Matrizen (16x16 oder 24x24)** mit einer **Tiefen-Maske (`z_depth`)**.
- Beim Start generiert `voxel_extrude_sprite()` daraus ein optimiertes 3D-Voxel-Mesh für Raylib.
- **3D-Parallax-Tiefenstaffelung:**
  - Layer 0 (Vordergrund-Rahmen / Rohre): `Z = +2.0f`
  - Layer 1 (Gameplay-Ebene - Spieler, Gegner, Laser): `Z = 0.0f`
  - Layer 2 (Mid-Background - Ruinen / Kathedralen-Säulen): `Z = -6.0f` (echte 3D-Voxel-Blöcke mit Lichteinfall!)
  - Layer 3 (Deep-Sky / Copper-Horizont): `Z = -20.0f`
- **Reaktive Grafik-Effekte:**
  - **Voxel-Shatter:** Stirbt ein Gegner, zerfällt seine Sprite-Matrix in bis zu 64 physikalische Einzel-Voxel, die von der Laser-Richtung weggeschleudert werden.
  - **Amiga-Interlace & Copper-Shader:** Bei niedrigem Leben oder massiven Explosionen verschiebt ein Post-Processing-Shader jede ungerade Bildschirmzeile um `sin(time)*2px` (Interlace-Look) und moduliert die Hintergrund-Farbpalette im Takt der Bassdrum.
