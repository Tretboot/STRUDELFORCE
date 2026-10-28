#!/usr/bin/env python3
"""
gen_embedded_music.py - Erzeugt src/embedded_music.h (GENERIERT, nicht handeditieren!)

Packt ausgewaehlte Chris Hülsbeck TFMX/MOD-Tracks IN die EXE (static const Arrays):
  1. Turrican 2: "Load", "World 1 - Dessert", "World 2 - Cave" (Multi-Subsongs!)
  2. Turrican 3: "Loader", "World 1 - Factory", "World 2 - Dungeons"
  3. Apidya: "Ongame 1 - Meadow", "Ongame 5 - Bosspanic"
  4. Wing Commander: "Regular Combat" (Action MOD), "Overall Victory" (MOD)

Total Audio-Budget: ~477 KB, bleibt weit unter dem 1.44 MB Floppy-Limit!

Aufruf:  python tools/gen_embedded_music.py
Output:  src/embedded_music.h
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (Symbol-Präfix, Typ, Pfad relativ zum Workspace-Root)
TRACKS = [
    ("TFMX_APIDYA_MEADOW",   "tfmx", "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx"),
    ("SAM_APIDYA_MEADOW",    "sam",  "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.sam"),
    ("TFMX_APIDYA_TECHNO",   "tfmx", "TFMX Professional/Apidya/Apidya - Ongame 4 - Techno.tfx"),
    ("SAM_APIDYA_TECHNO",    "sam",  "TFMX Professional/Apidya/Apidya - Ongame 4 - Techno.sam"),
    ("TFMX_APIDYA_BOSSPANIC", "tfmx", "TFMX Professional/Apidya/Apidya - Ongame 5 - Bosspanic.tfx"),
    ("SAM_APIDYA_BOSSPANIC",  "sam",  "TFMX Professional/Apidya/Apidya - Ongame 5 - Bosspanic.sam"),
    ("TFMX_T2_LOAD",         "tfmx", "TFMX Professional/Turrican 2/Turrican 2 - Load.tfx"),
    ("SAM_T2_LOAD",          "sam",  "TFMX Professional/Turrican 2/Turrican 2 - Load.sam"),
    ("TFMX_T2_DESERT",       "tfmx", "TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.tfx"),
    ("SAM_T2_DESERT",        "sam",  "TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.sam"),
    ("TFMX_T2_CAVE",         "tfmx", "TFMX Professional/Turrican 2/Turrican 2 - World 2 - Cave.tfx"),
    ("SAM_T2_CAVE",          "sam",  "TFMX Professional/Turrican 2/Turrican 2 - World 2 - Cave.sam"),
    ("TFMX_T3_LOADER",       "tfmx", "TFMX Professional/Turrican 3/Turrican 3 - Loader.tfx"),
    ("SAM_T3_LOADER",        "sam",  "TFMX Professional/Turrican 3/Turrican 3 - Loader.sam"),
    ("TFMX_T3_FACTORY",      "tfmx", "TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.tfx"),
    ("SAM_T3_FACTORY",       "sam",  "TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.sam"),
    ("TFMX_T3_DUNGEONS",     "tfmx", "TFMX Professional/Turrican 3/Turrican 3 - World 2 - Dungeons.tfx"),
    ("SAM_T3_DUNGEONS",      "sam",  "TFMX Professional/Turrican 3/Turrican 3 - World 2 - Dungeons.sam"),
    ("MOD_WC_REGULARCOMBAT", "mod",  "MOD/Mark_TDK_Knight_Wing_Commander_Amiga/wing_commander_regularcombat.mod"),
    ("MOD_WC_OVERALLVICTORY","mod",  "MOD/Mark_TDK_Knight_Wing_Commander_Amiga/wing_commander_overallvictory.mod"),
]

NAMES = {
    "TFMX_APIDYA_MEADOW":    "Apidya - Ongame 1 - Meadow (TFMX)",
    "SAM_APIDYA_MEADOW":     "Apidya - Ongame 1 - Meadow (Samples .sam)",
    "TFMX_APIDYA_TECHNO":    "Apidya - Ongame 4 - Techno (TFMX)",
    "SAM_APIDYA_TECHNO":     "Apidya - Ongame 4 - Techno (Samples .sam)",
    "TFMX_APIDYA_BOSSPANIC": "Apidya - Ongame 5 - Bosspanic (TFMX)",
    "SAM_APIDYA_BOSSPANIC":  "Apidya - Ongame 5 - Bosspanic (Samples .sam)",
    "TFMX_T2_LOAD":          "Turrican 2 - Load (TFMX)",
    "SAM_T2_LOAD":           "Turrican 2 - Load (Samples .sam)",
    "TFMX_T2_DESERT":        "Turrican 2 - World 1 - Desert (TFMX)",
    "SAM_T2_DESERT":         "Turrican 2 - World 1 - Desert (Samples .sam)",
    "TFMX_T2_CAVE":          "Turrican 2 - World 2 - Cave (TFMX)",
    "SAM_T2_CAVE":           "Turrican 2 - World 2 - Cave (Samples .sam)",
    "TFMX_T3_LOADER":        "Turrican 3 - Loader (TFMX)",
    "SAM_T3_LOADER":         "Turrican 3 - Loader (Samples .sam)",
    "TFMX_T3_FACTORY":       "Turrican 3 - World 1 - Factory (TFMX)",
    "SAM_T3_FACTORY":        "Turrican 3 - World 1 - Factory (Samples .sam)",
    "TFMX_T3_DUNGEONS":      "Turrican 3 - World 2 - Dungeons (TFMX)",
    "SAM_T3_DUNGEONS":       "Turrican 3 - World 2 - Dungeons (Samples .sam)",
    "MOD_WC_REGULARCOMBAT":  "Wing Commander - Regular Combat (MOD)",
    "MOD_WC_OVERALLVICTORY": "Wing Commander - Overall Victory (MOD)",
}


def main():
    out = os.path.join(ROOT, "src", "embedded_music.h")
    lines = []
    lines.append("/* ============================================================================")
    lines.append(" * embedded_music.h - GENERIERT von tools/gen_embedded_music.py")
    lines.append(" * NICHT HANDEDITIEREN. Tracks aendern -> Generator laufen lassen.")
    lines.append(" *")
    lines.append(" * In-EXE-Musik: Chris Huelsbeck TFMX Master-Soundtracks mit Multi-Subsongs")
    lines.append(" * (Turrican 2, Turrican 3, Apidya) + Wing Commander MODs.")
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
