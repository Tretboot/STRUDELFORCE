#!/usr/bin/env python3
"""
gen_embedded_music.py - Erzeugt src/embedded_music.h (GENERIERT, nicht handeditieren!)

Kuratierter, sparsamer Amiga & PC Soundtrack (v0.3.1):
  1. Turrican 3: World 1 Factory (Chris Huelsbeck Action Master)
  2. BiFi Adventure (Chris Huelsbeck Amiga Anthem)
  3. Cybersty (Action Synth)
  4. Mad TV: Ongame 6 (Amiga-Klassiker)
  5. Software Manager: Titel (22 Subsongs - fuer Kampagne Level 5, Credits & Bonus)
  6. Airwolf 2 (Chiptune MOD)
  7. Ocean Loader (Legendary Loading / Demoscene MOD)
  8. 3 Amiga Cracktro Mini-MODs: Intro 45, Intro 31, Intro 75

Total Audio-Budget: ~395 KB -> 100% Amiga Floppy- & PC-Paritaet!
"""
import os
import sys

def get_root():
    cur = os.path.dirname(os.path.abspath(__file__))
    while cur and not os.path.isfile(os.path.join(cur, "strudelforce.h")) and not os.path.isdir(os.path.join(cur, "src")):
        parent = os.path.dirname(cur)
        if parent == cur: break
        cur = parent
    return cur

ROOT = get_root()

TRACKS = [
    ("TFMX_T3W1",         "tfmx", "TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.tfx"),
    ("SAM_T3W1",          "sam",  "TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.sam"),
    ("TFMX_BIFI",         "tfmx", "TFMX Professional/BiFi Adventure/Bifi Adventure.tfx"),
    ("SAM_BIFI",          "sam",  "TFMX Professional/BiFi Adventure/Bifi Adventure.sam"),
    ("TFMX_CYBER",        "tfmx", "TFMX Professional/Cybersty/Cybersty.tfx"),
    ("SAM_CYBER",         "sam",  "TFMX Professional/Cybersty/Cybersty.sam"),
    ("TFMX_MADTV",        "tfmx", "TFMX Professional/Mad TV/Mad TV - Ongame 6.tfx"),
    ("SAM_MADTV",         "sam",  "TFMX Professional/Mad TV/Mad TV - Ongame 6.sam"),
    ("TFMX_SOFTMAN",      "tfmx", "TFMX Professional/Software Manager/Software Manager - Titel.tfx"),
    ("SAM_SOFTMAN",       "sam",  "TFMX Professional/Software Manager/Software Manager - Titel.sam"),
    ("MOD_AIRWOLF2",      "mod",  "MOD/Airwolf 2.mod"),
    ("MOD_OCEAN_LOADER",  "mod",  "MOD/Ocean Loader.mod"),
    ("MOD_INTRO45",       "mod",  "MOD/Intro Number 45.mod"),
    ("MOD_INTRO31",       "mod",  "MOD/Intro Number 31.mod"),
    ("MOD_INTRO75",       "mod",  "MOD/Intro Number 75.mod"),
]

NAMES = {
    "TFMX_T3W1":         "Turrican 3 - World 1 Factory (TFMX)",
    "SAM_T3W1":          "Turrican 3 - World 1 Factory (Samples .sam)",
    "TFMX_BIFI":         "BiFi Adventure (TFMX)",
    "SAM_BIFI":          "BiFi Adventure (Samples .sam)",
    "TFMX_CYBER":        "Cybersty (TFMX)",
    "SAM_CYBER":         "Cybersty (Samples .sam)",
    "TFMX_MADTV":        "Mad TV - Ongame 6 (TFMX)",
    "SAM_MADTV":         "Mad TV - Ongame 6 (Samples .sam)",
    "TFMX_SOFTMAN":      "Software Manager - Titel (TFMX)",
    "SAM_SOFTMAN":       "Software Manager - Titel (Samples .sam)",
    "MOD_AIRWOLF2":      "Airwolf 2 (Chiptune MOD)",
    "MOD_OCEAN_LOADER":  "Ocean Loader (Demoscene MOD)",
    "MOD_INTRO45":       "Intro Number 45 (Cracktro MOD)",
    "MOD_INTRO31":       "Intro Number 31 (Cracktro MOD)",
    "MOD_INTRO75":       "Intro Number 75 (Cracktro MOD)",
}

def main():
    out = os.path.join(ROOT, "src", "embedded_music.h")
    lines = []
    lines.append("/* ============================================================================")
    lines.append(" * embedded_music.h - GENERIERT von tools/generators/gen_embedded_music.py")
    lines.append(" * NICHT HANDEDITIEREN. Tracks aendern -> Generator laufen lassen.")
    lines.append(" *")
    lines.append(" * In-EXE-Musik: Chris Huelsbeck TFMX Master-Soundtracks mit Multi-Subsongs")
    lines.append(" * (Turrican 3, BiFi Adventure, Cybersty, Mad TV, Software Manager) + Cracktro MODs.")
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
            print("FEHLT: %s" % path)
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
