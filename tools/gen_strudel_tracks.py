#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_strudel_tracks.py — erzeugt src/strudel_test_tracks.h aus
assets/strudel_tracks/*.txt (Switch-Angel-Referenz-Tracks fuer den
Strudel-Sketch-v2-Acceptance-Test, test_strudel_sketch.c).

Kein GCC n鰐ig. Aufruf:  python tools/gen_strudel_tracks.py
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "assets", "strudel_tracks")
OUT = os.path.join(ROOT, "src", "strudel_test_tracks.h")

# (Datei, C-Name, Anzeige-Titel)
TRACKS = [
    ("in_the_beginning_trance.txt", "STU_TRACK_IN_THE_BEGINNING",
     "In the Beginning There was Trance (Switch Angel)"),
    ("green_tuesday.txt", "STU_TRACK_GREEN_TUESDAY",
     "Green Tuesday (Switch Angel)"),
    ("i_accidentally_made_stranger_things_again.txt", "STU_TRACK_STRANGER_THINGS",
     "I accidentally made Stranger Things Again (Switch Angel)"),
    ("extra_slippy.txt", "STU_TRACK_EXTRA_SLIPPY",
     "Extra Slippy (Switch Angel)"),
]


def to_c_string(text):
    """Ganzen Track als eine C-String-Literal-Kette (eine Zeile pro Literal)."""
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    parts = []
    for line in text.split("\n"):
        esc = line.replace("\\", "\\\\").replace('"', '\\"')
        parts.append('"%s\\n"' % esc)
    return " ".join(parts)


def main():
    out_lines = [
        "/*",
        " * strudel_test_tracks.h — ERZEUGT von tools/gen_strudel_tracks.py (NICHT handeditieren!)",
        " * Switch-Angel-Referenz-Sketches als C-Strings fuer test_strudel_sketch.c.",
        " * Quelle: assets/strudel_tracks/*.txt",
        " */",
        "#ifndef STRUDEL_TEST_TRACKS_H",
        "#define STRUDEL_TEST_TRACKS_H",
        "",
    ]
    for fname, cname, title in TRACKS:
        path = os.path.join(SRC_DIR, fname)
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
        out_lines.append("/* %s (%d Bytes) */" % (title, len(text.encode("utf-8"))))
        out_lines.append("static const char *%s =\n    %s;" % (cname, to_c_string(text)))
        out_lines.append("")
    out_lines.append("#endif /* STRUDEL_TEST_TRACKS_H */")
    out_lines.append("")
    with open(OUT, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(out_lines))
    print("gen", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
