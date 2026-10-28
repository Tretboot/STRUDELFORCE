#!/usr/bin/env python3
"""
build_tfmx_pack.py - Curate and package ~1MB TFMX Professional audio payload
Packs top Amiga classics (Turrican 2, Turrican 3, Apidya, Hard'n'Heavy, Battle Isle, Monkey Island)
with all required .sam samples into strudel_tfmx.zip.txt for Kiwi Cloud.
"""
import os
import sys
import zipfile
import io

ROOT_DIR = os.path.abspath(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TFMX_DIR = os.path.join(ROOT_DIR, "TFMX Professional")
OUT_ZIP = os.path.join(ROOT_DIR, "strudel_tfmx.zip")
OUT_TXT = os.path.join(ROOT_DIR, "strudel_tfmx.zip.txt")

CURATED_GAMES = [
    "Turrican 2",
    "Turrican 3",
    "Apidya",
    "Hard'n'Heavy",
    "Battle Isle",
    "Monkey Island"
]

def build_tfmx_pack():
    print("=" * 72)
    print("STRUDELFORCE — BUILDING CURATED ~1MB TFMX AUDIO PAYLOAD")
    print("=" * 72)

    if not os.path.exists(TFMX_DIR):
        print(f"[FEHLER] TFMX Professional Verzeichnis nicht gefunden: {TFMX_DIR}")
        sys.exit(1)

    files_to_pack = []
    
    for game in CURATED_GAMES:
        game_dir = os.path.join(TFMX_DIR, game)
        if not os.path.exists(game_dir):
            print(f"[WARNUNG] Spielordner nicht gefunden: {game}")
            continue

        # Collect .tfx and matching .sam files directly under game folder (skip subdirs like TFMX-MOD)
        tfx_files = sorted([f for f in os.listdir(game_dir) if f.lower().endswith(".tfx")])
        for tfx in tfx_files:
            p_tfx = os.path.join(game_dir, tfx)
            sam = tfx[:-4] + ".sam"
            p_sam = os.path.join(game_dir, sam)
            if not os.path.exists(p_sam):
                print(f"[FEHLER] Fehlende .sam Datei fuer: {p_tfx}")
                sys.exit(1)
            files_to_pack.append((p_tfx, f"TFMX Professional/{game}/{tfx}"))
            files_to_pack.append((p_sam, f"TFMX Professional/{game}/{sam}"))

    print(f"Gefundene Dateien: {len(files_to_pack)} ({len(files_to_pack)//2} Tracks mit Samples)\n")

    # Create zip file in memory / file
    with zipfile.ZipFile(OUT_ZIP, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for full_p, arc_p in files_to_pack:
            sz = os.path.getsize(full_p)
            zf.write(full_p, arc_p)
            print(f"  + {arc_p:<60} ({sz:>7} B)")

    zip_sz = os.path.getsize(OUT_ZIP)

    # Rename to .zip.txt to avoid chat UI parsing limits
    if os.path.exists(OUT_TXT):
        os.remove(OUT_TXT)
    os.rename(OUT_ZIP, OUT_TXT)

    print("-" * 72)
    print(f"[ERFOLG] Curated TFMX Archiv erstellt: strudel_tfmx.zip.txt")
    print(f"Dateigroesse: {zip_sz / 1024:.1f} KB ({zip_sz / (1024*1024):.2f} MB, {zip_sz} Bytes)")
    print("=" * 72)
    return zip_sz

if __name__ == "__main__":
    build_tfmx_pack()

