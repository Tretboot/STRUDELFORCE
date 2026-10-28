#!/usr/bin/env python3
"""
kiwi_reconcile.py - Automated & Safe Reconcile Tool for Kiwi Cloud Returns

Vergleicht, prueft und integriert Exporte aus der Kiwi Cloud zurueck
in das lokale STRUDELFORCE_Master Arbeitsverzeichnis.

Verwendung:
  python tools/kiwi_reconcile.py <export.zip>          # Dry-Run: Pruefung & Diff-Analyse
  python tools/kiwi_reconcile.py <export.zip> --apply  # Sichere Uebernahme & Verifikation
"""
import sys
import os
import shutil
import zipfile
import hashlib
import re
import subprocess
import time

SAFETY_CRITICAL_SOURCES = {
    "src/game_logic.c",
    "src/engine_core.c",
    "src/soundengine_core.c",
    "src/strudelforce.h"
}

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

def check_zero_float(content, filename):
    issues = []
    # Pruefe auf float/double Definitionen ausserhalb von Kommentaren
    lines = content.splitlines()
    for idx, line in enumerate(lines, 1):
        s = line.strip()
        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            continue
        if re.search(r'\b(float|double)\b', s) and not "float_div" in s and not "float_mod" in s:
            issues.append(f"Zeile {idx}: Float/Double gefunden: {s}")
        if re.search(r'\b(sinf|cosf|tanf|sqrtf)\b', s):
            issues.append(f"Zeile {idx}: Math.h Funktion gefunden: {s}")
        if "#include <math.h>" in s:
            issues.append(f"Zeile {idx}: <math.h> Include gefunden!")
    return issues

def check_monolith_integrity(content, filename):
    issues = []
    if "game_logic.c" in filename:
        required_symbols = ["core_init", "core_tick", "core_setup_platforms", "START_LIVES"]
        for sym in required_symbols:
            if sym not in content:
                issues.append(f"Kritisches Symbol fehlt in game_logic.c: {sym} (Verdacht auf Stub-Ueberschreibung!)")
    if "strudelforce.h" in filename:
        required_structs = ["GameState", "LevelPlatform", "Projectile", "Enemy"]
        for sym in required_structs:
            if sym not in content:
                issues.append(f"Kritische Struktur fehlt in strudelforce.h: {sym}")
    return issues

def reconcile(zip_path, apply_changes=False):
    root_dir = os.path.abspath(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    stage_dir = os.path.join(root_dir, "tmp_kiwi_stage")

    if not os.path.isfile(zip_path):
        print(f"[FEHLER] Datei nicht gefunden: {zip_path}")
        return False

    print("=" * 70)
    print("STRUDELFORCE KIWI CLOUD RECONCILIATION")
    print(f"Eingabe-ZIP : {zip_path}")
    print(f"Modus       : {'ANWENDEN (APPLY)' if apply_changes else 'DRY-RUN (Vorschau)'}")
    print("=" * 70)

    # 1. Bereinige & Entpacke in Staging-Ordner
    if os.path.exists(stage_dir):
        shutil.rmtree(stage_dir)
    os.makedirs(stage_dir, exist_ok=True)

    with zipfile.ZipFile(zip_path, "r") as zf:
        zf.extractall(stage_dir)

    # Wenn der Inhalt in einem Unterordner wie STRUDELFORCE_Master liegt, diesen finden
    extract_root = stage_dir
    subdirs = os.listdir(stage_dir)
    if len(subdirs) == 1 and os.path.isdir(os.path.join(stage_dir, subdirs[0])):
        extract_root = os.path.join(stage_dir, subdirs[0])

    print(f"Staging entpackt nach: {extract_root}")

    # 2. Vergleiche Dateien
    new_files = []
    modified_files = []
    unchanged_files = []
    safety_violations = []

    for r, dirs, files in os.walk(extract_root):
        for f in files:
            fp = os.path.join(r, f)
            rel = os.path.relpath(fp, extract_root).replace("\\", "/")
            if rel == "MANIFEST.json" or rel.endswith(".zip"):
                continue

            local_fp = os.path.join(root_dir, rel)
            if not os.path.exists(local_fp):
                new_files.append((rel, fp))
            else:
                h_in = sha256_file(fp)
                h_loc = sha256_file(local_fp)
                if h_in != h_loc:
                    modified_files.append((rel, fp, local_fp))
                else:
                    unchanged_files.append(rel)

    print(f"\nVergleichs-Ergebnis:")
    print(f"  - Neue Dateien        : {len(new_files)}")
    print(f"  - Modifizierte Dateien: {len(modified_files)}")
    print(f"  - Unveraenderte       : {len(unchanged_files)}")

    # 3. Sicherheits-Pruefungen fuer modifizierte / neue C-Dateien
    print("\nFuehre Sicherheits-Checks gemaess der 3 Eisernen Gesetze durch...")
    for rel, fp, *rest in modified_files + [(nf[0], nf[1], None) for nf in new_files if nf[0].endswith((".c", ".h"))]:
        if not rel.endswith((".c", ".h")):
            continue
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as fc:
                content = fc.read()
        except Exception as e:
            safety_violations.append((rel, [f"Lesefehler: {e}"]))
            continue

        # Zero-Float Check im Amiga Core
        if rel in {"src/game_logic.c", "src/engine_core.c"}:
            zf_issues = check_zero_float(content, rel)
            if zf_issues:
                safety_violations.append((rel, zf_issues))

        # Monolith Integrity Check
        if rel in SAFETY_CRITICAL_SOURCES:
            mi_issues = check_monolith_integrity(content, rel)
            if mi_issues:
                safety_violations.append((rel, mi_issues))

    if safety_violations:
        print("\n[WARNUNG] Gefundene Sicherheits-Verletzungen in den eingehenden Quellen:")
        for rel, issues in safety_violations:
            print(f"  Datei: {rel}")
            for iss in issues[:3]:
                print(f"    -> {iss}")
            if len(issues) > 3:
                print(f"    -> ... und {len(issues) - 3} weitere")
    else:
        print("  [PASS] Alle Quellcodedateien erfuellen Zero-Float & Monolith-Integritaet.")

    # 4. Details anzeigen
    if new_files:
        print("\nNeue Dateien:")
        for rel, _ in new_files[:10]:
            print(f"  + {rel}")
        if len(new_files) > 10: print(f"  ... und {len(new_files) - 10} weitere")

    if modified_files:
        print("\nModifizierte Dateien:")
        for rel, _, _ in modified_files:
            crit = " [KRITISCH]" if rel in SAFETY_CRITICAL_SOURCES else ""
            print(f"  * {rel}{crit}")

    # 5. Aenderungen anwenden
    if apply_changes:
        if safety_violations:
            print("\n[STOPP] Wegen Sicherheitsverletzungen wird kein automatisches Ueberschreiben durchgefuehrt!")
            print("Bitte pruefen Sie die betroffenen Dateien manuell im Staging-Ordner.")
            return False

        # Backup erstellen
        ts = time.strftime("%Y%m%d_%H%M%S")
        backup_dir = os.path.join(root_dir, "archive", f"pre_reconcile_{ts}")
        os.makedirs(backup_dir, exist_ok=True)
        print(f"\nErstelle Backup der lokalen Dateien nach: {backup_dir}")
        for rel, _, local_fp in modified_files:
            b_dest = os.path.join(backup_dir, rel)
            os.makedirs(os.path.dirname(b_dest), exist_ok=True)
            shutil.copy2(local_fp, b_dest)

        # Kopiere neue und modifizierte Dateien
        print("\nWende Aenderungen an...")
        for rel, staged_fp in new_files:
            dest = os.path.join(root_dir, rel)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(staged_fp, dest)
            print(f"  [NEU] {rel}")

        for rel, staged_fp, _ in modified_files:
            dest = os.path.join(root_dir, rel)
            shutil.copy2(staged_fp, dest)
            print(f"  [MOD] {rel}")

        # Verifikation nach dem Merge
        print("\nFuehre lokale Verifikation durch (selftest.py)...")
        res = subprocess.run([sys.executable, os.path.join(root_dir, "tools", "selftest.py")], cwd=root_dir)
        if res.returncode == 0:
            print("\n[ERFOLG] Reconciliation erfolgreich abgeschlossen und verifiziert!")
        else:
            print("\n[WARNUNG] Selftest nach Merge meldete Fehler. Bitte pruefen!")
    else:
        print("\n(Tipp: Fuehren Sie das Skript mit '--apply' aus, um diese Aenderungen sicher zu uebernehmen.)")

    # Staging aufraeumen
    shutil.rmtree(stage_dir, ignore_errors=True)
    print("=" * 70)
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Verwendung: python tools/kiwi_reconcile.py <export.zip> [--apply]")
        sys.exit(1)
    zpath = sys.argv[1]
    apply_mode = "--apply" in sys.argv
    success = reconcile(zpath, apply_changes=apply_mode)
    sys.exit(0 if success else 1)
