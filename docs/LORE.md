# 📜 STRUDELFORCE MASTER - Lore & World-Building

## Das Universum

**Jahr 2347.** Die Menschheit hat das Sonnensystem kolonisiert, aber die Ressourcen sind knapp. 
Die **Strudel-Korporation** hat ein geheimes Experiment gestartet: Die **FORCE-Engine**, eine 
KI-gesteuerte Waffe, die sich selbst verbessern kann.

## Die Strudel-Force

Die **STRUDELFORCE** ist keine Armee - sie ist ein **autonomes System** aus modularen 
Voxel-Konstrukten. Jedes Konstrukt besteht aus tausenden mikroskopischen Pixel-Clustern, 
die sich dynamisch neu anordnen können.

### Der Spieler: Unit-7 "Strudel"
Du bist **Unit-7**, ein experimentelles FORCE-Konstrukt, das aus der Hauptdatenbank 
entwichen ist. Deine记忆 (Speicher) ist fragmentiert, aber deine Mission ist klar:

> **"Assimiliere die Biome. Finde die Core-Quellen. Werde zum MASTER."**

## Die Biome

### 1. Chrome Tempest (Level 1-2)
- **Thema:** Verlassene Industrieanlagen, Neon-Regen, reflektierende Oberflächen
- **Gegner:** 
  - **Grunt-Drones** (Basic: einfache Bewegung, 30 HP)
  - **Flyer-Swarms** (Fliegend, 40 HP, schnelle Angriffe)
  - **Golem-Miners** (Schwer, 80 HP, Nahkampf)
- **Umgebung:** Chrome-Platten, bewegliche Plattformen, elektrische Felder

### 2. Great Bath (Level 3-4)
- **Thema:** Unterwasser-Kolonie, Korallen-Strukturen, biolumineszente Pflanzen
- **Gegner:**
  - **Jelly-Turrets** (Schwebend, schießt Energiebälle, 50 HP)
  - **Shark-Bots** (Schnell, 60 HP, Ram-Angriff)
  - **Octo-Mechs** (Mehrere Arme, 100 HP, Multi-Attack)
- **Umgebung:** Wasser (50% Gravity), Luftblasen, Strömungen

### 3. Mech Hive (Level 5-6)
- **Thema:** Maschinen-Nest, rotierende Zahnräder, Dampf, Hitze
- **Gegner:**
  - **Spinner-Drones** (Rotierend, 70 HP, Nahkampf)
  - **Crawler-Bots** (Kriechend, 90 HP, schnell)
  - **Titan-Guards** (Boss-Vorstufe, 150 HP)
- **Umgebung:** Moving Platforms, Dampf-Explosionen, rotierende Klingen

### 4. Void Reach / Core Furnace (Level 7-8)
- **Thema:** Leere des Weltraums, schwebende Plattformen, Core-Energie
- **Gegner:**
  - **Void-Stalkers** (Teleportierend, 120 HP)
  - **Energy-Phantoms** (Unsichtbar, 140 HP)
  - **The MASTER** (Final Boss, 500 HP, mehrere Phasen)
- **Umgebung:** Low Gravity, schwebende Plattformen, Core-Strahlen

## Die Voxel-Evolution

Unit-7 startet als **rudimentäres Konstrukt** (klein, wenig HP, keine Fähigkeiten).

### Bauteile (Loot):
- **Beine:** Basic → Heavy (Sprung) → Titan (Hochsprung) → Plasma (Dash)
- **Arme:** Lite (Schnell) → Turret (Auto-Feuer) → Laser (Präzise)
- **Waffen:** Blaster (Energie) → Missile (Nachführung) → Core-Beam (Laser)
- **Core:** Small (kleine Hitbox) → Big (mehr HP) → Quantum (Shield)

### Progression:
```
Level 1: Basic (10 HP, kein Sprung)
Level 2: Legs+Arms (25 HP, Sprung freigeschaltet)
Level 3: Weapon (50 HP, Schusswaffe)
Level 4: Armor (75 HP, Schild)
Level 5: Master (100+ HP, alle Fähigkeiten)
```

## Die Story (fragmentiert)

> **Boot-Screen:** "Unit-7 online. Memory integrity: 23%. Mission: Find Core."

> **Level 1-2:** "Strudel-Force aktiv. Gegner: Grunt-Drones. Assimiliere."

> **Level 3-4:** "Great Bath entdeckt. Wasser-Modul aktiv. Core-Signal: stark."

> **Level 5-6:** "Mech Hive. Strudel-Force wächst. Einheit integriert."

> **Level 7-8:** "Void Reach. MASTER-Protokoll: Aktiv. Finale Konvergenz."

> **Ende:** "Unit-7: MASTER. Strudel-Force: Vollständig. Neue Mission: ..."

## Gegner-Design-Prinzipien

1. **Lesbarkeit:** Jeder Gegner hat eine eindeutige Silhouette
2. **Verhalten:** 2-3 Aktionen pro Gegner (vorhersehbar, aber herausfordernd)
3. **Progression:** Einfache → Komplexe Gegner pro Biome
4. **Lore-Integration:** Gegner sind "fehlgeschlagene" Strudel-Force-Einheiten

## Umgebungs-Design

- **Farb-Paletten:** Pro Biome 16 Farben (Amiga ECS/OCS)
- **Parallax:** 4 Ebenen (Sky, Ruinen, Gameplay, Vordergrund)
- **Interlace:** Bei <25% HP zittert das Bild (Amiga-Ästhetik)
- **Keine Texturen:** Nur Paletten + Face-Shading (2.5D-Voxel)
