#!/usr/bin/env python3
"""
gen_embedded_music.py - Erzeugt src/embedded_music.h (GENERIERT, nicht handeditieren!)

Packt ausgewaehlte TFMX/MOD-Tracks IN die EXE (static const Arrays), damit:
  1. das Intro (Apidya Meadow, TFMX + Original-.sam-Samples) ohne Disk-Dateien laeuft
  2. die Jukebox einen Fallback hat, wenn die Ordner-Library leer/fehlend ist
  3. spaeter der M68K/ADF-Target dieselbe Technik (INCBIN-Style) nutzen kann

Tracks (bewusst klein halten!):
  - TFMX: Apidya - Ongame 1 - Meadow (.tfx + .sam = Original-Amiga-Samples)
  - MOD : Wing Commander - Regular Combat (Action)
  - MOD : Wing Commander - Overall Victory (Sieges-Fanfare)

Hinweis: Die 3 Lost-Patrol-Dateien (MOD/Lost_Patrol/*.mod) sind KEIN Standard-
ProTracker-Format (kein M.K./M!K!-Sig bei 1080) und werden vom Player abgelehnt.

Aufruf:  python tools/gen_embedded_music.py
Output:  src/embedded_music.h
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (Symbol-Präfix, Typ, Pfad relativ zum Workspace-Root)
TRACKS = [
    ("TFMX_APIDYA_MEADOW", "tfmx", "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx"),
    ("SAM_APIDYA_MEADOW",  "sam",  "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.sam"),
    ("MOD_WC_REGULARCOMBAT", "mod", "MOD/Mark_TDK_Knight_Wing_Commander_Amiga/wing_commander_regularcombat.mod"),
    ("MOD_WC_OVERALLVICTORY", "mod", "MOD/Mark_TDK_Knight_Wing_Commander_Amiga/wing_commander_overallvictory.mod"),
]

NAMES = {
    "TFMX_APIDYA_MEADOW": "Apidya - Ongame 1 - Meadow (TFMX)",
    "SAM_APIDYA_MEADOW": "Apidya - Ongame 1 - Meadow (Samples .sam)",
    "MOD_WC_REGULARCOMBAT": "Wing Commander - Regular Combat (MOD)",
    "MOD_WC_OVERALLVICTORY": "Wing Commander - Overall Victory (MOD)",
}


def main():
    out = os.path.join(ROOT, "src", "embedded_music.h")
    lines = []
    lines.append("/* ============================================================================")
    lines.append(" * embedded_music.h - GENERIERT von tools/gen_embedded_music.py")
    lines.append(" * NICHT HANDEDITIEREN. Tracks aendern -> Generator laufen lassen.")
    lines.append(" *")
    lines.append(" * In-EXE-Musik (PC-GUI): Intro (Apidya Meadow + Original-Samples),")
    lines.append(" * Jukebox-Fallback. static const -> kein Heap, Amiga-INCBIN-kompatibel.")
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
