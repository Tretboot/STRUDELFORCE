#!/usr/bin/env python3
"""
build_kiwi_handoff.py - STRUDELFORCE Kiwi Cloud Handoff Packer

Erstellt ein optimal gepacktes Handoff-ZIP fuer Kiwi Cloud Sandboxes
gemaess KIWI_SURVIVAL_GUIDE.md und docs/KIWI_PERSISTENCE_GUIDE.md.

Optionen:
  python tools/build_kiwi_handoff.py --curated    # Top Action-Klassiker (~6.5 MB ZIP, empfohlen)
  python tools/build_kiwi_handoff.py --full       # Alle 355 spielbaren TFMX Tracks (~17.7 MB ZIP)
  python tools/build_kiwi_handoff.py --out <zip>  # Ziel-Dateiname angeben
"""
import sys
import os
import json
import hashlib
import zipfile
import time

CURATED_ACTION_GAMES = {
    "Turrican 2",
    "Turrican 3",
    "Apidya",
    "Jim Power",
    "Z-Out",
    "Hard'n'Heavy",
    "Battle Isle",
    "BC Kid",
    "Gem X",
    "Gem Z",
    "Master Blazer",
    "Metal Law",
    "Mr. Nutz",
    "PP Hammer",
    "Quik And Silva",
    "Denny",
    "Flies",
    "Fly Harder",
    "Freak Out",
    "Lethal Zone",
    "Logical",
    "Techno Prison",
    "Tusker",
    "Wolfen",
    "Monkey Island"
}

SKIP_DIRS = {
    ".git", "__pycache__", "build", ".kiwi_probe",
    "archive", "releases", "Kickstart", "tmp_obj"
}

SKIP_EXT = {
    ".wav", ".raw", ".adf", ".amiga", ".exe", ".obj", ".o",
    ".ppm", ".bmp", ".zip"
}

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

def collect_files(root_dir, mode="curated"):
    files_to_pack = {}  # relpath -> abspath
    stats = {
        "code_files": 0,
        "doc_files": 0,
        "asset_files": 0,
        "mod_files": 0,
        "tfmx_tracks": 0,
        "sam_files": 0,
        "total_raw": 0
    }

    # 1. Sammle Projektdateien (ohne TFMX Professional)
    for root, dirs, files in os.walk(root_dir):
        dirs[:] = sorted([d for d in dirs if d not in SKIP_DIRS and d != "TFMX Professional"])
        for f in sorted(files):
            ext = os.path.splitext(f)[1].lower()
            if ext in SKIP_EXT:
                continue
            fp = os.path.join(root, f)
            rel = os.path.relpath(fp, root_dir).replace("\\", "/")
            if rel == "MANIFEST.json" or rel.endswith(".zip"):
                continue

            sz = os.path.getsize(fp)
            files_to_pack[rel] = fp
            stats["total_raw"] += sz

            if ext in {".c", ".h", ".py", ".bat", ".sh"} or f == "Makefile":
                stats["code_files"] += 1
            elif ext in {".md", ".txt"}:
                stats["doc_files"] += 1
            elif ext in {".json", ".dat"}:
                stats["asset_files"] += 1
            elif ext == ".mod":
                stats["mod_files"] += 1

    # 2. Sammle TFMX Professional Tracks (nur Ordner mit vorhandenen .sam Samples!)
    tfmx_root = os.path.join(root_dir, "TFMX Professional")
    if os.path.isdir(tfmx_root):
        for game_folder in sorted(os.listdir(tfmx_root)):
            g_path = os.path.join(tfmx_root, game_folder)
            if not os.path.isdir(g_path):
                continue

            # In Curated-Modus nur Top-Klassiker aufnehmen
            if mode == "curated" and game_folder not in CURATED_ACTION_GAMES:
                continue

            g_files = os.listdir(g_path)
            sam_files = [f for f in g_files if f.lower().endswith(".sam")]
            if not sam_files:
                # Ohne .sam Samples ist der Track stumm -> ueberspringen
                continue

            tfx_files = [f for f in g_files if f.lower().endswith(".tfx")]
            for f in tfx_files + sam_files:
                fp = os.path.join(g_path, f)
                rel = os.path.relpath(fp, root_dir).replace("\\", "/")
                sz = os.path.getsize(fp)
                files_to_pack[rel] = fp
                stats["total_raw"] += sz
                if f.lower().endswith(".tfx"):
                    stats["tfmx_tracks"] += 1
                elif f.lower().endswith(".sam"):
                    stats["sam_files"] += 1

    return files_to_pack, stats

def build_handoff(mode="curated", out_path=None):
    root_dir = os.path.abspath(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if not out_path:
        out_path = os.path.join(root_dir, f"STRUDELFORCE_kiwi_handoff_{mode}.zip")

    print("=" * 68)
    print(f"STRUDELFORCE KIWI CLOUD HANDOFF PACKER (Modus: {mode.upper()})")
    print("=" * 68)

    files_to_pack, stats = collect_files(root_dir, mode=mode)

    print(f"Sammle Dateien aus: {root_dir}")
    print(f"  - Quellcode / Skripte : {stats['code_files']} Dateien")
    print(f"  - Dokumentation       : {stats['doc_files']} Dateien")
    print(f"  - Assets (JSON/Config): {stats['asset_files']} Dateien")
    print(f"  - ProTracker MODs     : {stats['mod_files']} Dateien")
    print(f"  - TFMX Tracks (.tfx)  : {stats['tfmx_tracks']} Tracks")
    print(f"  - TFMX Samples (.sam) : {stats['sam_files']} Sample-Banken")
    print(f"  - Gesamtdateien       : {len(files_to_pack)}")
    print(f"  - Rohgroesse          : {stats['total_raw'] / (1024 * 1024):.2f} MB")

    # Manifest erstellen
    manifest_entries = {}
    for rel, absp in files_to_pack.items():
        manifest_entries[rel] = {
            "size": os.path.getsize(absp),
            "sha256": sha256_file(absp)
        }

    manifest_data = {
        "format": "kiwi-manifest/1",
        "project": "STRUDELFORCE",
        "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "mode": mode,
        "count": len(manifest_entries),
        "stats": stats,
        "files": manifest_entries
    }

    manifest_json_str = json.dumps(manifest_data, indent=2, sort_keys=True)

    # In temporaere Datei im Root schreiben fuer Standalone-Sichtbarkeit
    manifest_path = os.path.join(root_dir, "MANIFEST.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        f.write(manifest_json_str)

    # Erstelle ZIP mit Deflate Level 9
    print(f"\nPacke ZIP nach: {out_path}")
    base_prefix = "STRUDELFORCE_Master"
    with zipfile.ZipFile(out_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        # 1. Manifest
        zf.writestr(f"{base_prefix}/MANIFEST.json", manifest_json_str)
        # 2. Alle gesammelten Dateien
        for rel, absp in sorted(files_to_pack.items()):
            arcname = f"{base_prefix}/{rel}"
            zf.write(absp, arcname)

    zip_size = os.path.getsize(out_path)
    ratio = (1.0 - (zip_size / stats["total_raw"])) * 100.0 if stats["total_raw"] > 0 else 0

    print(f"  -> ZIP-Groesse : {zip_size / (1024 * 1024):.2f} MB ({zip_size} Bytes)")
    print(f"  -> Kompression : {ratio:.1f}% Platzersparnis")
    print(f"  -> Kiwi Budget : {zip_size / (1024 * 1024):.2f} MB / 18.00 MB Limit ({'OK' if zip_size < 18*1024*1024 else 'WARNUNG: Limit ueberschritten'})")

    # Verifikation des erstellten ZIPs
    print("\nVerifiziere ZIP-Integritaet gegen MANIFEST.json...")
    with zipfile.ZipFile(out_path, "r") as zf:
        names = zf.namelist()
        missing = []
        corrupted = []
        for rel, info in manifest_entries.items():
            aname = f"{base_prefix}/{rel}"
            if aname not in names:
                missing.append(rel)
                continue
            data = zf.read(aname)
            actual_sha = hashlib.sha256(data).hexdigest()
            if actual_sha != info["sha256"]:
                corrupted.append(rel)

    if not missing and not corrupted:
        print(f"  [PASS] Alle {len(manifest_entries)} Dateien im ZIP sind 100% intakt und sha256-verifiziert!")
        print("=" * 68)
        return True
    else:
        print(f"  [FAIL] Verifikation fehlgeschlagen: {len(missing)} missing, {len(corrupted)} corrupted!")
        print("=" * 68)
        return False

if __name__ == "__main__":
    mode = "curated"
    out = None
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--full":
            mode = "full"
        elif args[i] == "--curated":
            mode = "curated"
        elif args[i] == "--out" and i + 1 < len(args):
            out = args[i + 1]
            i += 1
        i += 1
    success = build_handoff(mode=mode, out_path=out)
    sys.exit(0 if success else 1)
