# STRUDELFORCE - Mission für Claude (KIWI)
Stand: 2026-10-27 - finales Handoff. Details & Priorität: TODO.md; bindende Architektur-Regeln: AGENTS.md (die 3 Eisernen Gesetze).
Deine Aufgaben in dieser Iteration:
**AUFGABE 1 (P0, HÖCHSTE PRIORITÄT): LEVEL-DESIGN OVERHAUL**
- Das aktuelle Level-Gerüst ist UNBRAUCHBAR: Es erlaubt derzeit nur rudimentäres Rechts/Links-Movement und hat keine echte Spielstruktur.
- Das Level-Design muss VON GRUND AUF neu gebaut werden: echte Kollisionen, Plattformen, Vertikalität.
- Ziel ist ein echtes, spielbares Fundament - erst DARAUF folgt das Balancing.
- Wir brauchen strukturierte Voxel-Levels (basierend auf unserer 2.5D Architektur):
  handgebaute Hommagen an Klassiker wie Turrican, Giana Sisters und Super Mario,
  ZUSÄTZLICH ein prozedural generierter Roguelike-Modus, der Level-Bausteine zufällig, aber logisch zusammensetzt.
  Nutze deine Stärke im Level-Design, um die Datenstrukturen (Arrays/Structs) in C dafür vorzubereiten.
**AUFGABE 2: TFMX REVERSE ENGINEERING + DER ORIGINAL-SOUNDTRACK (AUDIO)**
- Der aktuelle TFMX-Konverter/Player erzeugt fehlerhaften Sound und Ladefehler.
- Analysiere die beiliegenden originalen .mdat/.smpl Dateien (z.B. Apidya).
- Repariere die Byte-Offsets, Wait-States und DMA-Makros im C-Code / Python-Skript. Der Sound muss originalgetreu laufen.
- NEU: Die originalen TFMX-Module (Apidya, Turrican etc.) sollen NICHT NUR technisch, sondern auch
  INHALTLICH/MUSIKALISCH analysiert werden (Song-Form, Stimm-Verbrauch, Sample-Zuordnung, Arrangement-Prinzipien).
- Ziel: EIN EIGENER ORIGINAL-SOUNDTRACK für "STRUDELFORCE MASTER" (Titel/Boot, Biome-Tunes, Boss-Kämpfe).
- Lass dich von den Klassikern inspirieren, extrahiere deren originale Amiga-Samples (aus .smpl/.mdat)
  und wiederverwende sie in unseren neuen Kompositionen.
**AUFGABE 3: DIE VOXEL-EVOLUTION (NEUES GAMEPLAY-KONZEPT, für die spätere Implementierung)**
- Der Spielercharakter wird modular aus Voxeln (PC) bzw. Pixel-Clustern (Amiga) zusammengesetzt.
- Der Spieler startet als kleines, rudimentäres Konstrukt und findet in der Welt Bauteile (Beine, Arme, Waffen).
- Durch das Einsammeln wächst der Charakter physisch, wird detaillierter und schaltet Funktionen frei.
- Architektur-Vorbereitung: statische C-Datenstrukturen (16.16-Fixed-Point, null malloc, 512 KB Chip-RAM),
  Bauteil-Spawns als Level-Features anbinden (-> Aufgabe 1), Amiga-Seite = Pixel-Cluster auf 6 Bitplanes.
