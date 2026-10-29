# STRUDELFORCE — Kiwi Python Injection & Packing Guide

Dieses Dokument dokumentiert das selbstextrahierende Python-Injektionsverfahren für **Kiwi** (Claude / LibreChat Cloud-Sandbox) mit speziellem Fokus auf die **Leveleditor-Entwicklung**.

---

## 1. Hintergrund: Warum Python-Injection statt .zip / .zip.txt?

### Das Problem mit Cloud-Uploads:
1. **Binary-Filter & MIME-Checks:** Manche Web-Interfaces (wie LibreChat oder bestimmte Sandbox-Gateways) blockieren `.zip`-Uploads oder beschädigen Binärdaten, wenn eine `.zip` einfach in `.txt` umbenannt wird (z. B. durch erzwungenes UTF-8-Encoding, Null-Byte-Abschneiden oder Prompt-Token-Grenzen).
2. **Große ZIPs (> 4 MB):** Pakete mit vielen Audio-Tracks (wie die 4.26 MB Lite-ZIP) sprengen oft das Chat-Prompt- oder Upload-Budget.
3. **Sandbox-Persistenz:** Python-Skripte können direkt im Terminal der Sandbox via `python kiwi_level_injector.py` ausgeführt werden. Sie erstellen alle Verzeichnisse garantiert mit `os.makedirs(..., exist_ok=True)` und verhindern den gefürchteten Silent-Drop-Bug von unvollständigen Extraktionen.

---

## 2. Das Packverfahren (L4H / LHA / Solid LZMA + Base64)

### Begriffsklärung "l4h / LHA":
- **Historischer Amiga-Kontext:** Im Amiga-Ökosystem ist **LHA/LZH** das Standardarchivformat (bekannt durch Kompressionsmethoden wie `-lh4-` und `-lh5-`).
- **Moderne Python-Injektion:** Da in der Cloud-Sandbox keine nativen Amiga-Decompressoren oder C-Compiler zur Laufzeit der Extraktion vorausgesetzt werden können, verwendet der Injektor **reine Python Standard-Bibliotheken** (`lzma`, `tarfile`, `base64`, `hashlib`):
  1. Alle kuratierten Dateien werden in einen **In-Memory Solid-Tar-Stream** geschrieben. Durch die Solid-Kompression werden redundante C-Header, Structs und Markdown-Texte über Dateigrenzen hinweg eliminiert.
  2. Der Tar-Stream wird via **LZMA (Preset 9)** extrem stark komprimiert (bis zu 85% Ersparnis).
  3. Der Binärstrom wird als **ASCII-Base64-Payload** direkt in ein einziges portables Python-Skript eingebettet.
  4. Optional steht auch **ZLIB (Deflate Level 9)** per `--algo zlib` zur Verfügung.

### Größenvergleich:
| Format / Profil | Ungepackt | Komprimiert | Skript-Größe (Base64) | Status |
|---|---|---|---|---|
| **Standard (Level-Editor)** | ~4.04 MB | 511 KB (LZMA) | **~699 KB** | **Empfohlen** (enthält alle Audio-Tests & 38/38 PASS) |
| **Slim (Ohne embedded audio)** | ~1.12 MB | 228 KB (LZMA) | **~321 KB** | Ultra-leicht (< 350 KB, nur Kern & Level-Pipeline) |
| **Alte Lite-ZIP** | ~6.5 MB | 4.26 MB (ZIP) | Nicht als Text inline bar | Zu groß für Prompt-Injection |

Beide Injektor-Varianten liegen weit unter dem 1.000.000-Zeichen-Limit von LibreChat und Claude!

---

## 3. Kuratierte Dateiauswahl (Bereinigung für Kiwi)

Um das Skript schlank und fokussiert zu halten, wurden alle unrelevanten Dateien strikt entfernt:

### Enthalten (Relevant für Leveleditor & History):
1. **Level-Pipeline:**
   - `levels/levels.json` (Single Source of Truth für Chunks & Legend)
   - `levels/levels_kiwi_extended.json` (Erweiterte Kiwi-Chunks)
   - `tools/level_tool.py` (Validierung, BFS Exit-Connectivity, Header-Gen)
   - `docs/kiwi_level_config_reference.h` (Kiwi's Tile- und Plattform-Referenz)
2. **Assets & Gamedesign:**
   - `assets/balancing.json`, `assets/story.json`, `assets/lang.json`, `assets/level_01.json`, `assets/track_metadata.json`
   - Story-Kapitel (`chapter_*.tfmx.json`), SFX-Definitionen, Strudel-Tracks.
3. **C-Kern (Zero-Float, Fixed-Point 16.16, 50Hz Simulation):**
   - `src/strudelforce.h` (Master-Header, Spielzustand, Entitäten, Kacheln)
   - `src/game_logic.c` (Physik, Plattform-Kollision, 3 Leben, Spawner, Wellen)
   - `src/engine_core.c` (2.5D Voxel-Szenen-Planer, Parallax-Kamera, Shims)
   - `src/soundengine_core.c` (TFMX, MOD, Cyber-Paula Mixer, SFX)
   - `src/game_main.c` (Portables 50Hz ANSI/ASCII Terminal-Spiel für Linux)
   - `src/gui_main.c`, `src/test_suite.c`, sämtliche Modul-Header.
4. **Tools & Verifikations-Oracles:**
   - `tools/selftest.py` (Vollständige Audio-, Level- & Syntax-Prüfung)
   - `tools/strudel_bridge.py`, `tools/mod_oracle.py`, `tools/tfmx_oracle.py`, `tools/kiwi_handoff.py`
   - `verify_core_sim.py` (Bit-exaktes Simulations-Oracle)
5. **Dokumentation & Vollständige Historie:**
   - `captains_log.md` (Komplette Chronologie aller Iterationen)
   - `PROJECT_BIBLE.md` (Die 3 Eisernen Gesetze)
   - `KIWI_SURVIVAL_GUIDE.md` (Verhaltensregeln für die Sandbox)
   - `BUILD_GUIDE.md`, `README.md`, `RELEASE_0.3.0.md`
   - `docs/BIOMES.md`, `docs/ITEMS_LEVELUP.md`, `docs/LORE.md`, etc.
6. **Audio (Kompaktes Minimum):**
   - **TFMX:** `tests/data/Turrican 2 - World 1 - Dessert.tfx` (13 KB) + `.sam` (40 KB)
   - **MOD:** `tests/data/selftest_song.mod` (4.5 KB)
   *(Garantiert, dass `tools/selftest.py` mit `SELFTEST OK fails=0` durchläuft!)*

### Strikt ausgeschlossen (Gereinigt):
- ❌ **Screenshots & Bilder:** Keine `.png`, `.bmp`, `.ppm` (erspart > 35 MB Ballast!).
- ❌ **Audiodatei-Wälder:** Keine 350+ unbenutzten TFMX-Tracks, keine 45 unbenutzten MODs, keine `.wav`-Samples.
- ❌ **Build-Artefakte:** Keine `.exe`, `.obj`, `.o`, `.adf`, `.raw`.
- ❌ **Archive & Alt-Zweige:** Keine `archive/`, `tmp_*/`, `.git/`, `.zip`.

---

## 4. Wie man den Injektor baut

### Standard-Build (Empfohlen):
```cmd
python tools/build_kiwi_injector.py
```
*Ergebnis:* Erzeugt `kiwi_level_injector.py` (und kopiert es als `kiwi_injector.py`), ~699 KB.

### Ultra-kompakter Slim-Build (ohne embedded_music.h):
```cmd
python tools/build_kiwi_injector.py --slim --out kiwi_injector_slim.py
```
*Ergebnis:* Erzeugt `kiwi_injector_slim.py`, nur ~321 KB!

### ZLIB-Kompatibilitäts-Build:
```cmd
python tools/build_kiwi_injector.py --algo zlib --out kiwi_injector_zlib.py
```

---

## 5. Wie man den Injektor in Kiwi anwendet

### In der Cloud-Sandbox (Terminal):
1. **Injektor ausführen:**
   ```bash
   python kiwi_level_injector.py
   ```
   *Entpackt alle 90 Dateien bit-genau ins aktuelle Verzeichnis.*

2. **Entpacken mit sofortigem Verifikationstest:**
   ```bash
   python kiwi_level_injector.py --test
   ```
   *Entpackt und führt automatisch `tools/selftest.py` und `verify_core_sim.py` aus.*

3. **Manifest vorab inspizieren (Dry-Run):**
   ```bash
   python kiwi_level_injector.py --list
   ```

### Wichtigste Befehle für Kiwi während der Leveleditor-Arbeit:
```bash
# 1. Level-Integrität & Pfadfindung testen:
python tools/level_tool.py validate

# 2. Vollständige Engine-Selftests:
python tools/selftest.py

# 3. 50Hz Terminal-Spiel bauen & testen:
gcc -std=c99 -O2 -Isrc src/game_main.c src/game_logic.c src/engine_core.c src/soundengine_core.c -lm -o strudelforce_game
./strudelforce_game --demo 500 --no-intro
```

