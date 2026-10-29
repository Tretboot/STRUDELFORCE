# STRUDELFORCE — Kiwi Cloud Handoff & Reconcile Workflow

Dieses Dokument definiert den standardisierten, verlustfreien Roundtrip-Prozess zwischen der lokalen Entwicklungsumgebung (VS Code / Windows) und der **Kiwi Cloud Sandbox**.

---

## 0. Python-Injection Handoff (Empfohlen für Chat/Prompt & Leveleditor)

Falls ein direkter ZIP-Upload im Sandbox-Interface blockiert wird (z. B. `.zip` in `.txt` umbenannt schlägt fehl oder Token-Limits verhindern große Binär-Uploads), existiert der **selbstextrahierende Python-Injektor**:

```bash
# 1. Injektor bauen (Solid LZMA + Base64, enthält Leveleditor-Dateien + 1 TFMX + 1 MOD):
python tools/build_kiwi_injector.py

# 2. Skript in Kiwi hochladen/ausführen:
python kiwi_level_injector.py --test
```
*Vorteile:* Nur ~699 KB (bzw. ~321 KB im `--slim` Modus), garantiert fehlerfreie Verzeichnisstruktur via `os.makedirs`, Zero-Chat-Dump, SHA-256 Integritätsprüfung aller Dateien. Siehe auch [KIWI_INJECTOR_GUIDE.md](file:///f:/AgentHome/STRUDELFORCE_Master/docs/KIWI_INJECTOR_GUIDE.md).

---

## 1. Architektur-Übersicht & Paket-Größen (ZIP-Verfahren)

Um Übertragungszeiten in die Cloud zu minimieren und Timeouts zu verhindern, existiert ein dreistufiges Packing-Profil:

| Profil | Befehl | Inhalt | ZIP-Größe | Einsatzzweck |
|---|---|---|---|---|
| **LITE** *(Empfohlen)* | `python tools/build_kiwi_handoff.py --lite` | Kompletter Code, Assets, Tools + **12 Elite Action Games** (53 TFMX Tracks + 53 Samples + 6 MODs) | **4.26 MB** | **Standard für Cloud-Sessions (< 5 MB Budget)** |
| **CURATED** | `python tools/build_kiwi_handoff.py --curated` | Kompletter Code + 25 Action Games (106 TFMX Tracks + 47 MODs) | **8.45 MB** | Erweiterte Soundtests (< 18 MB Budget) |
| **FULL** | `python tools/build_kiwi_handoff.py --full` | Alle 355 spielbaren TFMX Tracks + 265 Sample-Banken | **16.03 MB** | Vollständiges Archiv (< 18 MB Budget) |

### Enthaltene Elite-Soundtracks im LITE-Paket (4.26 MB):
1. **Turrican 2** (6 Tracks: World 1 Dessert, World 2 Cave, World 3 Flight, World 4 Factory, World 5 Aliens, Title)
2. **Turrican 3** (6 Tracks: World 1..3, Factory, Aliens, Title)
3. **Apidya** (6 Tracks: Meadow, Sewer, Pond, Techno, Theme, Level 5)
4. **Jim Power** (7 Tracks: Level 1, Level 2, Boss, Planet Mutant etc.)
5. **Z-Out** (7 Tracks: Level 1..4, Boss, Title)
6. **Battle Isle** (1 Track)
7. **Hard'n'Heavy** (2 Tracks)
8. **BC Kid** (7 Tracks)
9. **Gem X** (2 Tracks)
10. **Metal Law** (5 Tracks)
11. **PP Hammer** (2 Tracks)
12. **Quik And Silva** (2 Tracks)
*Zusätzlich: 6 Wing Commander Amiga Combat MODs.*

---

## 2. Phase 1: Vorbereitung & Handoff-Erstellung (Lokal)

1. **Testsuite & Status prüfen:**
   ```cmd
   test_suite.exe
   python tools/selftest.py
   ```
2. **Lite-Handoff-Paket schnüren:**
   ```bash
   python tools/build_kiwi_handoff.py --lite
   ```
   *Erzeugt:* `STRUDELFORCE_kiwi_handoff_lite.zip` (4.26 MB) mit automatisch generiertem `MANIFEST.json` (sha256 aller 293 Dateien).

---

## 3. Phase 2: Start in der Kiwi Cloud Sandbox

1. **ZIP in die Sandbox hochladen & entpacken.**
2. **Bootstrap- & Integritäts-Check ausführen:**
   ```bash
   python tools/kiwi_bootstrap.py
   ```
   *Was das Skript leistet:*
   - Stellt alle Verzeichnisse explizit via `os.makedirs` bereit (verhindert den Sandbox-Silent-Drop-Bug).
   - Prüft alle Dateien gegen `MANIFEST.json` per SHA-256.
   - Führt `tools/selftest.py` aus.
   - Initialisiert die Session-Regeln.

3. **Die 5 Eisernen Kiwi-Survival-Regeln während der Session beachten:**
   - **Regel 1 (Zero-Chat-Dump):** Niemals ganze C-Dateien oder Hex-Arrays im Chat ausgeben (Kontext-Diät einhalten).
   - **Regel 2 (create_file API):** Datei-Erstellung bevorzugt über die API statt Python-`open()` durchführen (garantierte Persistenz).
   - **Regel 3 (Subagenten-Limit):** Maximal 3 bis 5 Subagenten pro Iteration delegieren.
   - **Regel 4 (Mega-Atomic Calls):** Zusammengehörige Änderungen in einem einzigen Python-Aufruf ausführen.
   - **Regel 5 (Amiga-First Gesetz):** Null `float`, null `double`, null `sin()`, null `malloc` im Simulations-Loop (`game_logic.c` / `engine_core.c`).

---

## 4. Phase 3: Export aus der Kiwi Cloud (Return)

Nach Abschluss der Optimierungen in der Cloud:
```bash
python tools/build_kiwi_handoff.py --pack-return --out strudelforce_cloud_export.zip
```
*Ergebnis:* Ein kompaktes Export-ZIP, das alle neuen/geänderten Quellen, Assets und Dokumentationen mit aktuellem `MANIFEST.json` enthält.

---

## 5. Phase 4: Sichere Re-Integration lokal (VS Code / Antigravity)

Beim Zurückkommen in die lokale Umgebung verhindert das Tool [`tools/kiwi_reconcile.py`](file:///f:/AgentHome/STRUDELFORCE_Master/tools/kiwi_reconcile.py), dass versehentlich unvollständige Stubs den Monolithen überschreiben oder Float-Syntaxfehler den C89-Kern brechen:

1. **Schritt 1: Dry-Run (Prüfung & Diff-Analyse):**
   ```bash
   python tools/kiwi_reconcile.py strudelforce_cloud_export.zip
   ```
   - Entpackt in eine isolierte Staging-Umgebung (`tmp_kiwi_stage/`).
   - Identifiziert neue, modifizierte und identische Dateien.
   - **Automatische Sicherheits-Prüfungen:**
     - *Zero-Float Check:* Erkennt unzulässige `float`, `double`, `sinf`, `<math.h>`.
     - *Monolith Integrity Check:* Prüft das Vorhandensein von `core_init`, `core_tick`, `GameState`, 3 Leben, Kollision.
   - Zeigt Warnungen an, falls Sicherheitsregeln verletzt wurden.

2. **Schritt 2: Sichere Übernahme mit automatischem Backup:**
   ```bash
   python tools/kiwi_reconcile.py strudelforce_cloud_export.zip --apply
   ```
   - Erstellt ein lokales Timestamp-Backup aller modifizierten Dateien in `archive/pre_reconcile_<zeitstempel>/`.
   - Übernimmt neue Assets, Story-JSONs, Level-Konfigurationen und validierten Code.
   - Führt direkt im Anschluss `tools/selftest.py` aus.

3. **Schritt 3: Build & Determinismus bestätigen:**
   ```cmd
   build_msvc.bat
   test_suite.exe
   ```
   - Bestätigt `32 PASS, 0 FAIL` und unveränderten Determinismus (`Score=35150, Kills=191, Bounces=42047, Lives=9`).

4. **Schritt 4: Git Commit & Push:**
   ```bash
   git add -A
   git commit -m "feat(kiwi-cloud): integrate verified improvements from cloud run"
   git push origin main
   ```

