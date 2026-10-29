#!/usr/bin/env python3
"""stage_amiga_music.py — Windows-freundlicher Spiegel der Makefile-Ziel
`make stage_amiga_music`.

Kopiert die kuratierten TFMX-Tracks (Turrican 3 World 1, Bifi Adventure, Cybersty, Mad TV)
in `amiga_dist/music/` mit kurzen 8.3-Namen fuer das INCBIN-Embedding in `src/amiga_music.c`.
"""
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DST = os.path.join(ROOT, "amiga_dist", "music")
DIST = os.path.join(ROOT, "amiga_dist")

TRACKS = [
    ("TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.tfx", "T3W1.TFX"),
    ("TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.sam", "T3W1.SAM"),
    ("TFMX Professional/BiFi Adventure/Bifi Adventure.tfx", "BIFI.TFX"),
    ("TFMX Professional/BiFi Adventure/Bifi Adventure.sam", "BIFI.SAM"),
    ("TFMX Professional/Cybersty/Cybersty.tfx", "CYBER.TFX"),
    ("TFMX Professional/Cybersty/Cybersty.sam", "CYBER.SAM"),
    ("TFMX Professional/Mad TV/Mad TV - Ongame 6.tfx", "MADTV.TFX"),
    ("TFMX Professional/Mad TV/Mad TV - Ongame 6.sam", "MADTV.SAM"),
]

def main() -> int:
    if os.path.isdir(DST):
        shutil.rmtree(DST)
    os.makedirs(DST, exist_ok=True)
    os.makedirs(os.path.join(DIST, "s"), exist_ok=True)
    total = 0
    for src_rel, dst_name in TRACKS:
        src = os.path.join(ROOT, src_rel)
        dst = os.path.join(DST, dst_name)
        if not os.path.isfile(src):
            print("[FEHLER] fehlt:", src)
            return 1
        shutil.copy2(src, dst)
        size = os.path.getsize(dst)
        total += size
        print("[OK] %-12s <- %s (%d Bytes)" % (dst_name, src_rel, size))

    # startup-sequence fuer echte Boot-Floppy
    seq_path = os.path.join(DIST, "s", "startup-sequence")
    with open(seq_path, "w", newline="\n") as f:
        f.write("; STRUDELFORCE v0.3.1 - startup-sequence (A500)\n"
                "; Data-Disk: run  df0:strudelforce.amiga\n"
                "; Boot-Disk:  startet das Spiel direkt.\n"
                "echo \"STRUDELFORCE 2.5D Voxel Shooter (A500 v0.3.1)\"\n"
                "run \"strudelforce.amiga\"\n")

    print("STAGED %d Dateien, %d Bytes (%d KB) nach %s"
          % (len(TRACKS), total, total // 1024, DST))
    return 0

if __name__ == "__main__":
    sys.exit(main())
