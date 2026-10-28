#!/usr/bin/env python3
"""
gen_embedded_music.py - Erzeugt src/embedded_music.h (GENERIERT, nicht handeditieren!)

Packt ausgewaehlte Chris Huelsbeck TFMX/MOD-Tracks IN die EXE (static const Arrays):
  1. Turrican 2: "Load" (Fanfare), "World 1 - Desert", "World 2 - Cave" (Multi-Subsongs!)
  2. Apidya: "Ongame 1 - Meadow" (Chris Huelsbeck Amiga Anthem)
  3. Software Manager: Multi-Subsongs (22 Subsongs!) fuer dynamische Abspann-Phasen & Gameplay
  4. 3 Amiga Cracktros: Fairlight, Skidrow, Paradox (fuer das zufaellige Amiga-Start-Intro!)
  5. 8-Bit & MOD Klassiker: Blue Monday Chiptune, Wing Commander Combat, Lost Patrol

Total Audio-Budget: ~450 KB -> strudelforce.exe bleibt strikt UNTER 1 MB!

Aufruf:  python tools/gen_embedded_music.py
Output:  src/embedded_music.h
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TRACKS = [
    ("TFMX_APIDYA_MEADOW",   "tfmx", "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx"),
    ("SAM_APIDYA_MEADOW",    "sam",  "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.sam"),
    ("TFMX_T2_LOAD",         "tfmx", "TFMX Professional/Turrican 2/Turrican 2 - Load.tfx"),
    ("SAM_T2_LOAD",          "sam",  "TFMX Professional/Turrican 2/Turrican 2 - Load.sam"),
    ("TFMX_T2_DESERT",       "tfmx", "TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.tfx"),
    ("SAM_T2_DESERT",        "sam",  "TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.sam"),
    ("TFMX_T2_CAVE",         "tfmx", "TFMX Professional/Turrican 2/Turrican 2 - World 2 - Cave.tfx"),
    ("SAM_T2_CAVE",          "sam",  "TFMX Professional/Turrican 2/Turrican 2 - World 2 - Cave.sam"),
    ("TFMX_SOFTMAN",         "tfmx", "TFMX Professional/Software Manager/Software Manager - Msx.tfx"),
    ("SAM_SOFTMAN",          "sam",  "TFMX Professional/Software Manager/Software Manager - Msx.sam"),
    ("MOD_LOST_PATROL",      "mod",  "MOD/Lost_Patrol/tunev1.mod"),
    ("MOD_BLUE_MONDAY_CHIP", "mod",  "MOD/Pop Songs/- Dance and Pop/1980s/New Order/blue-monday (chip tune).mod"),
    ("MOD_WC_REGULARCOMBAT", "mod",  "MOD/Mark_TDK_Knight_Wing_Commander_Amiga/wing_commander_regularcombat.mod"),
    ("MOD_FAIRLIGHT",        "mod",  "MOD/Cracktro/Mod.Fairlight.Mod"),
    ("MOD_SKIDROW",          "mod",  "MOD/Cracktro/Mod.Skidrow Crack10.Mod"),
    ("MOD_PARADOX",          "mod",  "MOD/Cracktro/Mod.Paradox Crack 12.Mod"),
]

NAMES = {
    "TFMX_APIDYA_MEADOW":    "Apidya - Ongame 1 - Meadow (TFMX)",
    "SAM_APIDYA_MEADOW":     "Apidya - Ongame 1 - Meadow (Samples .sam)",
    "TFMX_T2_LOAD":          "Turrican 2 - Load (TFMX)",
    "SAM_T2_LOAD":           "Turrican 2 - Load (Samples .sam)",
    "TFMX_T2_DESERT":        "Turrican 2 - World 1 - Desert (TFMX)",
    "SAM_T2_DESERT":         "Turrican 2 - World 1 - Desert (Samples .sam)",
    "TFMX_T2_CAVE":          "Turrican 2 - World 2 - Cave (TFMX)",
    "SAM_T2_CAVE":           "Turrican 2 - World 2 - Cave (Samples .sam)",
    "TFMX_SOFTMAN":          "Software Manager - Msx (TFMX)",
    "SAM_SOFTMAN":           "Software Manager - Msx (Samples .sam)",
    "MOD_LOST_PATROL":       "Lost Patrol - tunev1 (MOD)",
    "MOD_BLUE_MONDAY_CHIP":  "New Order - blue-monday (chip tune) (MOD)",
    "MOD_WC_REGULARCOMBAT":  "Wing Commander - Regular Combat (MOD)",
    "MOD_FAIRLIGHT":         "Fairlight - Amiga Cracktro (MOD)",
    "MOD_SKIDROW":           "Skidrow - Cracktro 10 (MOD)",
    "MOD_PARADOX":           "Paradox - Cracktro 12 (MOD)",
}


def main():
    out = os.path.join(ROOT, "src", "embedded_music.h")
    lines = []
    lines.append("/* ============================================================================")
    lines.append(" * embedded_music.h - GENERIERT von tools/gen_embedded_music.py")
    lines.append(" * NICHT HANDEDITIEREN. Tracks aendern -> Generator laufen lassen.")
    lines.append(" *")
    lines.append(" * In-EXE-Musik: Chris Huelsbeck TFMX Master-Soundtracks mit Multi-Subsongs")
    lines.append(" * (Turrican 2, Apidya, Software Manager) + Amiga Cracktros & MODs.")
    lines.append(" * static const -> kein Heap, Amiga-INCBIN-kompatibel, null Disk-Zugriffe.")
    lines.append(" * ========================================================================= */")
    lines.append("#ifndef EMBEDDED_MUSIC_H")
    lines.append("#define EMBEDDED_MUSIC_H")
    lines.append("#include <stdint.h>")
    lines.append("")

    total = 0
    for sym, kind, rel in TRACKS:
        path = os.path.join(ROOT, rel)
        if not os.path.isfile(path):
            print("FEHLT: %s" % rel)
            return 1
        data = open(path, "rb").read()
        if len(data) > 256 * 1024:
            print("ZU GROSS (%d Bytes > 256 KB): %s" % (len(data), rel))
            return 1
        total += len(data)
        lines.append("/* %s (%d Bytes) aus %s */" % (NAMES[sym], len(data), rel))
        lines.append("static const uint8_t EMB_%s[%d] = {" % (sym, len(data)))
        for i in range(0, len(data), 16):
            chunk = data[i:i + 16]
            lines.append("    " + ", ".join("0x%02X" % b for b in chunk) + ",")
        lines.append("};")
        lines.append("static const uint32_t EMB_%s_LEN = %d;" % (sym, len(data)))
        lines.append("static const char EMB_%s_NAME[] = \"%s\";" % (sym, NAMES[sym]))
        lines.append("")

    lines.append("#define EMBEDDED_TRACKS_TOTAL_BYTES %d" % total)
    lines.append("")
    lines.append("#endif /* EMBEDDED_MUSIC_H */")

    with open(out, "w", encoding="ascii", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    print("OK: %s geschrieben (%d Tracks, %d Bytes Embedded)" % (out, len(TRACKS), total))
    return 0


if __name__ == "__main__":
    sys.exit(main())
