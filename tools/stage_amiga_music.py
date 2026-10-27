#!/usr/bin/env python3
"""stage_amiga_music.py — Windows-freundlicher Spiegel der Makefile-Ziel
`make stage_amiga_music` (diese Maschine hat kein make).

Kopiert die Turrican-2 TFMX-Tracks aus `TFMX Professional/Turrican 2/`
in `amiga_dist/music/` mit den kurzen Amiga-8.3-Namen, die `amiga_hw.c`
lädet (g_t2_music[]). Aenderungen: in Makefile-Template (gen_sources.py)
UND hier UND in src/amiga_hw.c vornehmen (dreifache Aenderungsregel).
"""
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "TFMX Professional", "Turrican 2")
DST = os.path.join(ROOT, "amiga_dist", "music")

# (Original-Dateiname, Ziel-Name) — muss zu g_t2_music[] in amiga_hw.c passen
TRACKS = [
    ("Turrican 2 - Load.tfx", "T2LOAD.TFX"),
    ("Turrican 2 - Load.sam", "T2LOAD.SAM"),
    ("Turrican 2 - World 1 - Dessert.tfx", "T2W1.TFX"),
    ("Turrican 2 - World 1 - Dessert.sam", "T2W1.SAM"),
    ("Turrican 2 - World 2 - Cave.tfx", "T2W2.TFX"),
    ("Turrican 2 - World 2 - Cave.sam", "T2W2.SAM"),
    ("Turrican 2 - World 3 - Flight.tfx", "T2W3.TFX"),
    ("Turrican 2 - World 3 - Flight.sam", "T2W3.SAM"),
    ("Turrican 2 - World 4 - Factory.tfx", "T2W4.TFX"),
    ("Turrican 2 - World 4 - Factory.sam", "T2W4.SAM"),
]


def main() -> int:
    os.makedirs(DST, exist_ok=True)
    total = 0
    for src_name, dst_name in TRACKS:
        src = os.path.join(SRC, src_name)
        dst = os.path.join(DST, dst_name)
        if not os.path.isfile(src):
            print("[FEHLER] fehlt:", src)
            return 1
        shutil.copy2(src, dst)
        size = os.path.getsize(dst)
        total += size
        print("[OK] %-12s <- %s (%d Bytes)" % (dst_name, src_name, size))
    print("STAGED %d Dateien, %d Bytes (%d KB) nach %s"
          % (len(TRACKS), total, total // 1024, DST))
    return 0


if __name__ == "__main__":
    sys.exit(main())