#!/usr/bin/env python3
"""
kiwi_bootstrap.py - Safe Bootstrap & Verification in Kiwi Cloud Sandbox

Fuehrt nach dem Entpacken in der Kiwi Cloud folgende Schritte aus:
1. Verzeichnis-Integritaet pruefen / erstellen (Injector-Fix)
2. MANIFEST.json pruefen (sha256 aller Dateien)
3. Python-Selftest ausfuehren (tools/selftest.py)
4. Kiwi Survival Guidelines anzeigen
"""
import os
import sys
import json
import hashlib
import subprocess

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

def bootstrap(root_dir=None):
    if not root_dir:
        root_dir = os.path.abspath(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    print("=" * 68)
    print("STRUDELFORCE KIWI CLOUD BOOTSTRAP")
    print(f"Arbeitsverzeichnis: {root_dir}")
    print("=" * 68)

    # 1. Verzeichnisse sicherstellen
    required_dirs = ["src", "assets", "tools", "docs", "tests", "levels", "TFMX Professional", "MOD"]
    for rd in required_dirs:
        p = os.path.join(root_dir, rd)
        os.makedirs(p, exist_ok=True)
    print("[OK] Verzeichnis-Struktur verifiziert.")

    # 2. MANIFEST.json pruefen
    manifest_path = os.path.join(root_dir, "MANIFEST.json")
    if not os.path.exists(manifest_path):
        print("[WARNUNG] Kein MANIFEST.json gefunden. Ueberspringe Hash-Check.")
    else:
        with open(manifest_path, "r", encoding="utf-8") as f:
            m = json.load(f)
        files = m.get("files", {})
        missing = []
        corrupted = []
        for rel, info in files.items():
            fp = os.path.join(root_dir, rel)
            if not os.path.exists(fp):
                missing.append(rel)
                continue
            if sha256_file(fp) != info["sha256"]:
                corrupted.append(rel)

        if missing or corrupted:
            print(f"[FEHLER] Integritaets-Check fehlgeschlagen: {len(missing)} fehlen, {len(corrupted)} beschaedigt!")
            for f in missing[:5]: print(f"  Missing: {f}")
            for f in corrupted[:5]: print(f"  Corrupt: {f}")
            return False
        else:
            print(f"[OK] Alle {len(files)} Dateien sind intakt und sha256-geprueft.")

    # 3. Python-Selftest
    selftest_path = os.path.join(root_dir, "tools", "selftest.py")
    if os.path.exists(selftest_path):
        print("\nFuehre tools/selftest.py aus...")
        res = subprocess.run([sys.executable, selftest_path], cwd=root_dir)
        if res.returncode != 0:
            print("[FEHLER] Selftest fehlgeschlagen!")
            return False
        print("[OK] Python-Selftest bestanden.")

    # 4. Leitfaden ausgeben
    print("\n" + "=" * 68)
    print("KIWI SANDBOX REGELN FUER DIESE SESSION:")
    print("1. ZERO-CHAT-DUMP: Kein Code/Arrays im Chat posten (Kontext-Diät).")
    print("2. CREATE_FILE API nutzen (garantierte Persistenz statt Python-I/O).")
    print("3. MAX 3-5 SUBAGENTEN pro Iteration (keine Kontext-Überlastung).")
    print("4. MEGA-ATOMIC CALLS fuer zusammenhaengende Aenderungen.")
    print("5. AMIGA-FIRST GESETZ: Zero-Float & Zero-Malloc im Simulations-Loop!")
    print("=" * 68 + "\n")
    return True

if __name__ == "__main__":
    ok = bootstrap()
    sys.exit(0 if ok else 1)
