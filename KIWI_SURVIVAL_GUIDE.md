# KIWI SURVIVAL GUIDE

## STRUDELFORCE MASTER - Sandbox-Learnings für zukünftige KIWI-Sprünge

**Dokument erstellt:** 2026-10-28
**Ziel:** Sichere Übergabe von VS Code zurück in die Cloud-Sandbox

---

## 1. INJECTOR-FIX (Verzeichnis-Erstellung)

### Problem:
Python-Skripte schreiben Dateien in nicht-existierende Verzeichnisse → Silent Failure.

### Lösung:
**IMMER** Verzeichnisse explizit erstellen VOR dem Schreiben:

```python
import os
os.makedirs("/mnt/data/src", exist_ok=True)
os.makedirs("/mnt/data/assets", exist_ok=True)
os.makedirs("/mnt/data/tools", exist_ok=True)
```

Oder den `force_extract.py` / `kiwi_injector.py` Mechanismus nutzen, der:
1. Verzeichnisse erstellt
2. Dateien mit expliziten Pfaden schreibt
3. Persistenz verifiziert

### WICHTIG:
`ls` zeigt Dateien manchmal nicht an, aber Python kann sie lesen. **Verwende `create_file` API** statt Python-I/O für garantierte Persistenz.

---

## 2. ZERO-CHAT-DUMP PROTOKOLL

### Regel:
**KEIN Code, KEINE Arrays, KEINE Datei-Inhalte im Chat!**

### Warum:
- Kontext-Diät (max. 4096 Token)
- Vermeidung von Verwirrung bei langen Sessions
- Fokus auf Status-Flags statt Details

### Richtige Kommunikation:
- ✅ "✅ Datei X aktualisiert (25KB)"
- ✅ "⚠️  Issue in Line 235 gefunden"
- ❌ "Hier ist der Code: `int x = 0; ...`"

### Ausnahme:
Nur bei expliziter Anfrage des Users oder wenn Code < 10 Zeilen ist.

---

## 3. CREATE_FILE vs. PYTHON-I/O

### Erkenntnis:
**`create_file` ist ZUVERLÄSSIGER als Python-Skripte für persistente Dateien.**

### Warum:
- Python-Skripte schreiben in `/mnt/data`, aber `ls` erkennt sie manchmal nicht
- `create_file` garantiert persistente Speicherung im Sandbox-Filesystem
- Verifizierungstools sehen `create_file`-Dateien zuverlässig

### Muster für zukünftige Arbeiten:
```python
# ❌ FALSCH (Python-Skript schreibt Datei)
with open("/mnt/data/file.c", 'w') as f:
    f.write(code)

# ✅ RICHTIG (create_file API)
create_file(path="/mnt/data/file.c", content=code, overwrite=True)
```

### Wann Python-Skripte OK sind:
- Berechnungen ohne Datei-Ausgabe
- JSON-Strukturen generieren (dann mit `create_file` schreiben)
- Kurze Verifikations-Checks

---

## 4. SUBAGENTEN-LIMIT

### Regel:
**Maximal 3-5 Subagenten pro Iteration.**

### Warum:
- Mehr Subagenten → Kontext-Überlastung
- Parallele Calls können Timeouts verursachen
- Ergebnisse werden unzuverlässig

### Best Practices:
1. **Demoscene Mechanic** (Ornith): Code-Optimierung
2. **World Builder** (122B): Level-Generierung
3. **Audio Analyst**: Sound-Engine
4. **Playtester** (optional): Balancing
5. **System Analyst** (optional): Dokumentation

### Workflow:
- Delegiere → Sammle Input → Füge selbst ein (nicht an Subagenten schreiben!)

---

## 5. MEGA-ATOMIC CALL GEGEN WIPES

### Problem:
Mehrere kleine Datei-Änderungen → Risiko von Inkonsistenzen oder Wipes.

### Lösung:
**Komplexe Änderungen in EINEM Python-Skript ausführen:**

```python
# ✅ RICHTIG: Mega-Atomic Call
def main():
    # 1. Datei A lesen und patchen
    # 2. Datei B lesen und patchen
    # 3. Datei C erstellen
    # 4. Report schreiben
    pass

if __name__ == "__main__":
    main()
```

### Warum:
- Alles oder nichts (kein halb-fertiger Zustand)
- Keine Race Conditions zwischen Calls
- Einfache Verifizierung am Ende

---

## 6. C89/C99 DUAL-TARGET ENGINE

### Regeln für Amiga 68k + PC:
- **KEIN Float/double** im Game-Loop (16.16 Fixed-Point)
- **KEIN malloc** im Game-Loop (statische Pools)
- **SIN8/COS8 Lookup-Tables** statt sin()/cos()
- **C89-Syntax** (keine C99 Features wie `for(int i=...)`)

### Fixed-Point Optimierungen:
- `/65536` → `>>16` (Bit-Shift statt Division)
- `*65536` → `<<16` (Bit-Shift statt Multiplikation)
- `FIX16_TO_INT(x)` → `(x >> 16)`
- `FIX16_FROM_INT(x)` → `(x << 16)`

---

## 7. ASSET-PACKING & VFS

### Pattern:
1. **Asset-Packer** (Python): Packt alle Assets in `assets.dat`
2. **VFS** (C): Lädt Blob einmalig, gibt Pointer zurück
3. **Zero-Malloc**: Statische Dekompressions-Puffer

### Code-Struktur:
```c
// engine_core.c
static uint8_t decompress_buffer[131072];  // 128KB statisch

const uint8_t *asset_get(AssetID id, uint32_t *out_size) {
    // Dekomprimiere in statischem Puffer
    // Gib Pointer zurück (Zero-Malloc im Game-Loop)
}
```

---

## 8. LOKALISIERUNG (i18n)

### Pattern:
1. **lang.json** (Quelle): Alle Texte in DE/EN/JP
2. **build_lang.py** (Generator): Erstellt `localization.h` zur Compile-Zeit
3. **localization.h** (C): Statische Arrays, O(1) Lookup

### Code:
```c
// localization.h
typedef enum { TXT_MAIN_MENU = 0, ... } TextID;
static const char *tbl_de[TEXT_COUNT] = { "...", ... };
static inline const char *get_text(TextID id) {
    return tbl_de[id];  // O(1) Lookup, Zero-Malloc
}
```

---

## 9. PERFORMANCE-METRIKEN (Amiga 500)

### Limits:
- **RAM:** 512 KB Chip-RAM (~400 KB nutzbar)
- **CPU:** 68000 @ 7.09 MHz
- **Audio:** 8 Paula-Kanäle
- **Frame-Time:** 20 ms (50 Hz PAL)

### Budget-Nutzung (STRUDELFORCE):
- **RAM:** ~400 KB (78%)
- **CPU:** ~67.5% im Game-Loop
- **Audio:** 8/8 Kanäle (100%)
- **Frame-Time:** ~13.5 ms (67.5%)

### Headroom:
- **32.5% CPU-Puffer** für Partikel, Parallax, UI

---

## 10. CHECKLISTE FÜR NEUE SESSIONS & ROUNDTRIP

### Vorbereitung des Handoffs (Lokal):
- [ ] `build_msvc.bat` und `test_suite.exe` (32 Checks) ausführen
- [ ] `python tools/build_kiwi_handoff.py --lite` ausführen
- [ ] `STRUDELFORCE_kiwi_handoff_lite.zip` (4.26 MB, 53 TFMX Tracks + 6 MODs) in die Cloud laden

### Beim Start einer neuen KIWI-Session:
- [ ] `python tools/kiwi_bootstrap.py` ausführen (erstellt Verzeichnisse & verifiziert Hashes)
- [ ] `PROJECT_BIBLE.md` und `docs/KIWI_HANDOFF_WORKFLOW.md` lesen für Kontext
- [ ] Zero-Chat-Dump Protokoll aktivieren
- [ ] Max 3-5 Subagenten delegieren
- [ ] `create_file` für alle Datei-Operationen verwenden
- [ ] Mega-Atomic Calls für komplexe Änderungen
- [ ] C89/C99 Regeln einhalten (Zero-Float, Zero-Malloc)

### Vor dem Export aus der Cloud (Return):
- [ ] `.o` Dateien und temporäre Build-Artefakte löschen
- [ ] `python tools/build_kiwi_handoff.py --pack-return --out strudelforce_cloud_export.zip`
- [ ] Export-ZIP herunterladen

### Re-Integration lokal (VS Code / Antigravity):
- [ ] Dry-Run: `python tools/kiwi_reconcile.py strudelforce_cloud_export.zip`
- [ ] Anwenden mit automatischem Backup: `python tools/kiwi_reconcile.py strudelforce_cloud_export.zip --apply`
- [ ] Lokale Verifikation: `build_msvc.bat` und `test_suite.exe` (32 PASS)
- [ ] Git commit & push

---

## KONTAKT / NEXT STEPS

Bei Unsicherheiten:
1. `docs/KIWI_HANDOFF_WORKFLOW.md` lesen (Detail-Anleitung für den Roundtrip)
2. `PROJECT_BIBLE.md` lesen (Architektur-Vision)
3. `WORKFLOW_ESSAY.md` lesen (Iteration-Historie)
4. Code-Review mit `kiwi_toolkit.py lint` durchführen
5. Bei kritischen Issues: Manual Intervention

---

**Ende des KIWI SURVIVAL GUIDE**
*Dieses Dokument sollte mit jedem Projekt-Export archiviert werden.*
