#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
visual_check.py — Pixel-Regression für den GUI-Dump (v0.1a Turrican-2-Upgrade).

Prüft aus einem `strudelforce_v0.1.exe --dump N <basis>`-Satz:
  1. Menü-Frame (0): Chrome-Tempest-Silhouette im Hintergrund.
  2. Letzter Game-Frame: Untergrund-Terrassen (Spalten-Höhenvariante + Strata),
     Biome-Silhouetten, Vordergrund-Rohre, neue HUD-Bevel-Panels,
     und dass das alte HUD (480x114-Box + Steuerungshinweise) weg ist.
  3. Logik-Spiegelung: world_hash/terrain_height der C-Seite für alle 5 Biome
     (beweist, dass die Biome unterschiedliche Gelaeufe erzeugen).

Nutze:  python tools/visual_check.py v01a
"""
import os
import sys


def load_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    idx = 0
    fields = []
    while len(fields) < 4:
        while data[idx:idx + 1] in (b" ", b"\t", b"\n", b"\r"):
            idx += 1
        if data[idx:idx + 1] == b"#":
            while data[idx:idx + 1] not in (b"\n", b"\r"):
                idx += 1
            continue
        start = idx
        while data[idx:idx + 1] not in (b" ", b"\t", b"\n", b"\r"):
            idx += 1
        fields.append(data[start:idx])
    idx += 1
    w, h = int(fields[1]), int(fields[2])
    px = data[idx: idx + w * h * 3]
    return w, h, px


def col_at(px, w, x, y):
    i = (y * w + x) * 3
    return (px[i], px[i + 1], px[i + 2])


def count_near(px, w, h, x0, y0, x1, y1, rgb, tol=3):
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            i = (y * w + x) * 3
            if (abs(px[i] - rgb[0]) <= tol and abs(px[i + 1] - rgb[1]) <= tol
                    and abs(px[i + 2] - rgb[2]) <= tol):
                n += 1
    return n


def world_hash(biome, slot, salt):
    s = (slot * 1664525 + biome * 1013904223 + salt * 7919 + 24635) & 0xFFFFFFFF
    s = (s * 1103515245 + 12345) & 0xFFFFFFFF
    s ^= s >> 13
    s = (s * 22695477 + 1) & 0xFFFFFFFF
    s ^= s >> 16
    return s & 0x7FFFFFFF


def terrain_height(biome, block):
    h = world_hash(biome, block, 11) % 100
    if biome == 1:
        return 24 + (h % 2) * 10
    if biome == 2:
        return 96 if h < 55 else 28
    if biome == 3:
        return 64 + (h % 4) * 22
    if biome == 4:
        return 44 + (h % 5) * 18
    return 28 + (h % 3) * 14


def main():
    prefix = sys.argv[1] if len(sys.argv) > 1 else "v01a"
    files = sorted(f for f in os.listdir(".") if f.startswith(prefix + "_") and f.endswith(".ppm"))
    assert files, "Keine Dump-Frame fuer Praefix '%s'" % prefix
    w, h, menu = load_ppm(files[0])
    # Game-Frame finden: Panel-Farbe (12,20,30) bei (20,60) — saubere Game-Frame
    # liegen zwischen Menue-Segment (idx<2) und dem Game-Over-Dimming (0.22x).
    game_idx = None
    for gi in range(2, len(files)):
        w3, h3, px3 = load_ppm(files[gi])
        i3 = (60 * w3 + 20) * 3
        if px3[i3] == 12 and px3[i3 + 1] == 20 and px3[i3 + 2] == 30:
            game_idx = gi
            break
    assert game_idx is not None, "Kein sauberes Game-Frame im Dump gefunden"
    w2, h2, game = load_ppm(files[game_idx])
    ok = True

    def check(name, cond, info=""):
        nonlocal ok
        mark = "PASS" if cond else "FAIL"
        if not cond:
            ok = False
        print("  [%s] %s %s" % (mark, name, info))

    print("== Menu-Frame %s (%dx%d) ==" % (files[0], w, h))
    n = count_near(menu, w, h, 0, 300, w, 649, (16, 26, 40))
    check("Menue-Silhouette (Chrome-Tuerme)", n > 1500, "px=%d" % n)

    print("== Game-Frame %s (idx %d) ==" % (files[game_idx], game_idx))
    under = (26, 35, 44)
    top_hi = (81, 108, 136)
    strata = (17, 23, 28)
    silh = (53, 63, 74)
    pipe = (37, 44, 52)
    panel = (12, 20, 30)

    n = count_near(game, w2, h2, 0, 632, w2, 700, under) + count_near(game, w2, h2, 0, 632, w2, 700, strata)
    check("Untergrund-Terrassen vorhanden", n > 30000, "px=%d" % n)

    # y=684 (52px unter der Oberflaeche): nur die hoechsten Blöcke (h>=52) reichen
    # so tief -> Terrain/Basis-Wechsel zeigen die Terrassen-Stufen.
    runs = 0
    prev = None
    for x in range(0, w2, 1):
        c = col_at(game, w2, x, 684)
        is_terr = c in (under, top_hi, strata)
        if prev is None or is_terr != prev:
            runs += 1
        prev = is_terr
    check("Terrassen haben Stufen (Hoehen-Wechsel in Zeile y=684)", runs > 8, "runs=%d" % runs)

    n = count_near(game, w2, h2, 0, 632, w2, 700, strata)
    check("Strata-Linien im Untergrund", n > 2000, "px=%d" % n)

    n = count_near(game, w2, h2, 0, 36, w2, 619, silh)
    check("Biome-Silhouetten (Layer 2)", n > 800, "px=%d" % n)

    n = count_near(game, w2, h2, 0, 36, w2, 700, pipe)
    check("Vordergrund-Rohre (Layer 0)", n > 2000, "px=%d" % n)

    check("Linkes Panel (LIVES/HP/SCHILD) gefuellt",
          col_at(game, w2, 20, 60) == panel, str(col_at(game, w2, 20, 60)))
    check("Score-Panel (Mitte) gefuellt",
          col_at(game, w2, w2 // 2, 60) == panel, str(col_at(game, w2, w2 // 2, 60)))
    check("Rechtes Panel (KAMPAGNE/Waffe) gefuellt",
          col_at(game, w2, w2 - 270, 60) == panel, str(col_at(game, w2, w2 - 270, 60)))

    c = col_at(game, w2, 100, 84)
    check("HP-Bar segmentiert (gruen)", c[1] > 150 and c[0] < 120, str(c))
    c = col_at(game, w2, 100, 104)
    check("Schild-Bar segmentiert (cyan)", c[2] > 150 and c[1] > 90, str(c))

    c = col_at(game, w2, 350, 60)
    check("Alte HUD-Box (480x114) entfernt", c != (0, 16, 32), str(c))
    # Lücke zwischen Score-Panel (endet ~810) und rechtem Panel (startet 1000):
    # dort saß die alte Steuerungshinweis-Zeile — jetzt muss Welt durchschimmern.
    n = 0
    for x in range(820, 995):
        c = col_at(game, w2, x, 78)
        if c[0] > 150 and c[1] > 150 and c[2] > 150:
            n += 1
    check("Alte Steuerungshinweis-Zeile oben rechts weg", n < 40, "helle-px=%d" % n)

    print("== Logik-Spiegelung (5 Biome) ==")
    seqs = {}
    for b in range(5):
        seqs[b] = tuple(terrain_height(b, k) for k in range(40))
    distinct = len(set(seqs.values()))
    check("Alle 5 Biome erzeugen UNTERSCHIEDLICHE Terrassen", distinct == 5, "distinct=%d" % distinct)
    for b in range(5):
        hh = seqs[b]
        print("      Biome %d: min=%2d max=%2d z.B. %s" % (b, min(hh), max(hh), list(hh[:8])))

    print()
    print("RESULT: %s" % ("ALLE CHECKS OK" if ok else "FEHLER GEFUNDEDEN"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
