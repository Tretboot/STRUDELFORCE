#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
STRUDELFORCE Level Toolkit v2 (levelkit/lv2.py)
================================================
Single-File, headless (PIL optional fuer Previews), C89-tauglicher Export.

- v1-Chunk-Format (16x12 ASCII, level_data.h-kompatibel) + v2-Metadaten
  (Musik-Zonen als X-Bereiche, Cutscenes, Kampagnen-Graph) in levels_v2.json
- Physik: 1:1-Twin von src/game_logic.c (16.16 fix, y-up, Fuess-Modell,
  ECHTE Konstanten: GRAVITY 0.45 / JUMP 2.6 / MAX_FALL 8.0 / MAX_SPEED 2.4 /
  ACC 0.25 / FRICTION 0.80). 1 Tile = 1 World-Unit.
- Bot: greedy Right-Walker = Spielbarkeits-Untergrenze (Bot gewinnt -> Level ok)
- Generator: seedete Segmente, Difficulty-Rampe, nur bot-sichere Geometrie
- Export: levels.json (v1-pur), levels_v2.json (v2), src/level_data.h.c (+LvMusicZone)
- Preview: PNG (PIL), Tile-Palette
- Editor: Text-CLI (headless) + optionale pygame-GUI

WICHTIG (aus Erfahrung):
- Sprungreichweite der C-Physik ist GROSS (Apex 7.5 Units, ~28 Units Flug).
  Design-Regeln: Waende <=6 hoch, Hazard-Runs <=16, Gaps beliebig (Weltboden).
- Bot fluegt schmale Plattformen ueber -> Plattformen sind Loot-Optionen,
  der Pflichtweg laeuft ueber den Boden.

CLI:
  python3 lv2.py selftest          # gen + Bot-Test alles + Export + Preview
  python3 lv2.py gen [--count N] [--seed S]
  python3 lv2.py test [--level i] [--all]
  python3 lv2.py export
  python3 lv2.py preview [--level i] [--out F]
  python3 lv2.py edit [--level i]
  python3 lv2.py reach
"""
import json, os, sys, random

# ================= Konstanten (1:1 game_logic.c) =================
CHUNK_W, CHUNK_H = 16, 12
FPS = 50
SOLID    = set("#BMO?=")
ONEWAY   = set("-")
HAZARD   = set("^")
WATER    = set("~")
WALKABLE = SOLID | ONEWAY | WATER
LEGEND   = WALKABLE | HAZARD | set(".*HCPwtfGUoDSE")

FIX = 65536
def f16(v): return int(v * FIX)
def fm(a, b): return (a * b) >> 16
GRAV     = int(0.45 * FIX)
JUMP     = int(2.6 * FIX)
MAXFALL  = int(8.0 * FIX)
MAXSPEED = int(2.4 * FIX)
ACC      = int(0.25 * FIX)
FRICTION = int(0.80 * FIX)
HALF_W   = int(1.0 * FIX)
STOP_EPS = int(0.02 * FIX)

MAX_STEP_UP   = 6    # Hoechste kletterbare Mauer (Apex 7.5, Sicherheitsrand)
MAX_HAZARD_RUN= 16   # Hoechste spruengbare Hazard-Reihe
MAX_DITCH     = 8    # Design-Limit fuer Senken (Boden faengt, nur Optik)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ================= Modell =================
class Level:
    def __init__(self, name, chunks, music_zones=None, biome=0, palette=0,
                 seed=0, music_type=0, music_file="user_music/strudel_force.strudel",
                 time_limit=240, notes=""):
        self.name = name
        self.chunks = chunks              # list[12 strings], top-first, je 16 breit
        self.music_zones = music_zones or []   # [x0, x1, subsong]
        self.biome = biome; self.palette = palette
        self.seed = seed
        self.music_type = music_type
        self.music_file = music_file
        self.time_limit = time_limit
        self.notes = notes

    @property
    def W(self):
        return len(self.chunks) * CHUNK_W

    def t(self, x, y):
        """Tile bei (x, y-up: 0=Boden). Außerhalb: links/rechts Luft, unten Boden."""
        if y < 0: return '#'
        if y >= CHUNK_H: return '.'
        c, cx = divmod(x, CHUNK_W)
        if c < 0 or c >= len(self.chunks): return '.'
        return self.chunks[c][CHUNK_H - 1 - y][cx]

    def walkable(self, x, y): return self.t(x, y) in WALKABLE
    def solid(self, x, y):    return self.t(x, y) in SOLID
    def hazard(self, x, y):   return self.t(x, y) in HAZARD
    def water(self, x, y):    return self.t(x, y) in WATER

    def ground_top(self, x):
        """Hoechste begehbaere Oberflaeche in Spalte x (y), oder None."""
        for y in range(CHUNK_H - 1, -1, -1):
            if self.walkable(x, y):
                return y
        return None

    def find_char(self, ch):
        for c, rows in enumerate(self.chunks):
            for r, row in enumerate(rows):
                for cx, t in enumerate(row):
                    if t == ch:
                        return c * CHUNK_W + cx, CHUNK_H - 1 - r
        return None

    def spawn(self):
        p = self.find_char('P')
        return p if p else (2, 0)

    def exit_pos(self):
        p = self.find_char('E')
        return p if p else (self.W - 2, 0)

    def entities(self):
        """(kind, x, fuess_y) fuer w/t/f/G/U/o/C/D/S."""
        out = []
        for c, rows in enumerate(self.chunks):
            for r, row in enumerate(rows):
                for cx, t in enumerate(row):
                    if t in "wtfGUoCDS":
                        x = c * CHUNK_W + cx
                        y = CHUNK_H - 1 - r
                        g = self.ground_below(x, r)
                        out.append((t, x, g))
        return out

    def ground_below(self, x, r):
        """Oberflaeche (y) der ersten begehbaeren Zeile unter Chunk-Zeile r."""
        c, cx = divmod(x, CHUNK_W)
        for rr in range(r + 1, CHUNK_H):
            if self.chunks[c][rr][cx] in WALKABLE:
                return CHUNK_H - 1 - rr
        return 0

    def pickups(self):
        out = []
        for c, rows in enumerate(self.chunks):
            for r, row in enumerate(rows):
                for cx, t in enumerate(row):
                    if t == '*':
                        out.append((c * CHUNK_W + cx, CHUNK_H - 1 - r))
        return out

    def platforms(self):
        """Nur-Obenflaeche-Regel + Run-Merge. (x, y, w, kind), top-first Reihenfolge
        (1:1 zu level_engine.c)."""
        plats = []
        for r in range(CHUNK_H):
            c = 0
            W = self.W
            while c < W:
                t = self.t(c, CHUNK_H - 1 - r)
                above = self.solid(c, CHUNK_H - r)
                y = CHUNK_H - 1 - r
                if t in SOLID and not above:
                    run = 1
                    while c + run < W and self.t(c + run, y) in SOLID \
                            and not self.solid(c + run, y + 1):
                        run += 1
                    plats.append((c, y, run, 0)); c += run; continue
                if t in ONEWAY:
                    run = 1
                    while c + run < W and self.t(c + run, y) in ONEWAY:
                        run += 1
                    plats.append((c, y, run, 0)); c += run; continue
                if t in HAZARD or t in WATER:
                    run = 1
                    while c + run < W and self.t(c + run, y) in (HAZARD | WATER):
                        run += 1
                    plats.append((c, y, run, 2)); c += run; continue
                c += 1
        return plats

    def kill_tiles(self):
        out = set()
        for c, rows in enumerate(self.chunks):
            for r, row in enumerate(rows):
                for cx, t in enumerate(row):
                    if t in HAZARD:
                        out.add((c * CHUNK_W + cx, CHUNK_H - 1 - r))
        return out

    def to_dict(self):
        return {
            "name": self.name, "biome": self.biome, "palette": self.palette,
            "seed": self.seed, "music_type": self.music_type,
            "music_file": self.music_file, "time_limit": self.time_limit,
            "notes": self.notes,
            "music_zones": [list(z) for z in self.music_zones],
            "chunks": list(self.chunks),
        }

# ================= Physik-Twin (1:1 game_logic.c) =================
class Sim:
    def __init__(self, L):
        self.L = L
        sx, sy = L.spawn()
        self.px = f16(sx + 1)
        self.py = f16(L.ground_below(sx, CHUNK_H - 1 - sy)) if sy > 0 else 0
        self.pvx = 0; self.pvy = 0
        self.on_ground = 1
        self.dead = False; self.won = False
        self.plats = [(f16(x), f16(y), f16(w), k) for (x, y, w, k) in L.platforms()]
        self.kill = L.kill_tiles()
        ex, ey = L.exit_pos()
        self.exit_x = ex
        self.max_px = self.px
        self.stuck_ticks = 0

    def step(self, right, jump):
        L = self.L
        if right:
            self.pvx += ACC
            if self.pvx > MAXSPEED: self.pvx = MAXSPEED
        else:
            self.pvx = fm(self.pvx, FRICTION)
            if 0 < self.pvx < STOP_EPS: self.pvx = 0
        if jump and self.on_ground:
            self.pvy = JUMP; self.on_ground = 0
        self.pvy -= GRAV
        if self.pvy < -MAXFALL: self.pvy = -MAXFALL
        self.px += self.pvx
        self.py += self.pvy
        if self.py <= 0:
            self.py = 0
            if self.pvy < 0: self.pvy = 0
            self.on_ground = 1
        else:
            self.on_ground = 0
            if self.pvy <= 0:
                feet = self.py; prev = self.py - self.pvy
                for (px, py, pw, kind) in self.plats:
                    if self.px >= px and self.px <= px + pw:
                        if prev >= py and feet <= py + f16(0.45):
                            self.py = py
                            if self.pvy < 0: self.pvy = 0
                            self.on_ground = 1
                            break
        if self.px < HALF_W:
            self.px = HALF_W
            if self.pvx < 0: self.pvx = 0
        if self.px > f16(L.W) - HALF_W:
            self.px = f16(L.W) - HALF_W
            if self.pvx > 0: self.pvx = 0
        # Kill-Check: Spieler-Box px+-1, py..py+2
        x0 = int(self.px - HALF_W); x1 = int(self.px + HALF_W)
        y0 = int(self.py); y1 = y0 + 2
        for ty in range(y0, y1 + 1):
            for tx in range(x0, x1 + 1):
                if (tx, ty) in self.kill:
                    self.dead = True
                    return
        if int(self.px) >> 16 >= self.exit_x:
            self.won = True
            return
        if self.px > self.max_px:
            self.max_px = self.px
            self.stuck_ticks = 0
        else:
            self.stuck_ticks += 1

def bot_input(sim):
    """Greedy: immer rechts; springen bei Mauer (Kopfhoehe), Hazard voraus,
    Step-Up an der Kante. Wasser: laufen (Oberflaeche begehbar)."""
    L = sim.L
    cx = int(sim.px); cy = int(sim.py)
    jump = False
    if sim.on_ground:
        # 1) Hazard in Koerperhoehe voraus -> springen
        if (L.hazard(cx + 1, cy) or L.hazard(cx + 1, cy + 1)
                or L.hazard(cx + 2, cy) or L.hazard(cx + 2, cy + 1)):
            jump = True
        # 2) Solid-Mauer in Kopfhohe -> springen
        elif (L.solid(cx + 1, cy + 1) or L.solid(cx + 2, cy + 1)
                or L.solid(cx + 1, cy + 2) or L.solid(cx + 2, cy + 2)):
            jump = True
        # 3) Step-Up an der Kante -> springen (max MAX_STEP_UP)
        else:
            g = L.ground_top(cx)
            if g is None: g = 0
            gn = L.ground_top(cx + 1)
            if gn is None: gn = 0
            if gn >= g + 1 and gn - g <= MAX_STEP_UP + 1:
                if sim.px >= f16(cx + 1) - f16(0.3):
                    jump = True
    return True, jump

def run_bot(L, max_ticks=5000):
    sim = Sim(L)
    t = 0
    while t < max_ticks:
        right, jump = bot_input(sim)
        sim.step(right, jump)
        if sim.won or sim.dead:
            break
        if sim.stuck_ticks > 400:
            break
        t += 1
    if sim.won:
        return dict(win=True, frames=t, reason="Exit erreicht")
    if sim.dead:
        return dict(win=False, frames=t, reason="Hazard (Spikes) getroffen bei x=%d" % int(sim.px))
    if sim.stuck_ticks > 400:
        return dict(win=False, frames=t, reason="Stuck bei x=%d (max x=%d)" % (int(sim.px), int(sim.max_px)))
    return dict(win=False, frames=t, reason="Timeout nach %d Ticks bei x=%d" % (t, int(sim.px)))

# ================= Reach-Oracle =================
def reach_report():
    """Simuliert echte Sprungwerte (C-Physik) und gibt Design-Limits aus."""
    h = 0.0; v = 2.6; g = 0.45
    t_apex = v / g
    apex = v * t_apex - 0.5 * g * t_apex * t_apex
    t_land = 2 * t_apex
    dist = t_land * 2.4
    # Hazard-Run: Fuess > 1.0 Unit ueber Grund, Waende 1 Unit hoch
    import math
    lo = (v - math.sqrt(v * v - 4 * 0.5 * g * 1.0)) / g
    hi = (v + math.sqrt(v * v - 4 * 0.5 * g * 1.0)) / g
    hazard_run = (hi - lo) * 2.4
    return dict(apex=round(apex, 2), dist=round(dist, 2),
                hazard_run=round(hazard_run, 1),
                max_step_up=MAX_STEP_UP, max_hazard_run=MAX_HAZARD_RUN, max_ditch=MAX_DITCH)

# ================= Validierung =================
def static_checks(L):
    errs, warns = [], []
    if len(L.chunks) < 1 or len(L.chunks) > 24:
        errs.append("Chunk-Anzahl %d ausserhalb 1..24" % len(L.chunks))
    for ci, rows in enumerate(L.chunks):
        if len(rows) != CHUNK_H:
            errs.append("Chunk %d: %d Zeilen (12 erwartet)" % (ci, len(rows))); continue
        for r, row in enumerate(rows):
            if len(row) != CHUNK_W:
                errs.append("Chunk %d Zeile %d: Laenge %d (16 erwartet)" % (ci, r, len(row)))
                continue
            for cx, t in enumerate(row):
                if t not in LEGEND:
                    errs.append("Chunk %d (%d,%d): ungueltiges Tile '%s'" % (ci, cx, r, t))
    sp = L.find_char('P')
    if not sp: errs.append("Kein Spawn 'P'")
    if L.chunks and L.find_char('P') and L.find_char('P') != sp:
        pass
    nP = sum(row.count('P') for rows in L.chunks for row in rows)
    if nP > 1: errs.append("%d Spawns 'P' (1 erwartet)" % nP)
    if not L.find_char('E'): errs.append("Kein Exit 'E'")
    # Gap-Breite (Senken; Weltboden faengt, Design-Limit)
    run = 0
    for tx in range(1, L.W - 1):
        g = L.ground_top(tx)
        if g is None:   # komplett ohne Boden = echte Senke
            run += 1
            if run > MAX_DITCH + 2:
                errs.append("Lücke >%d Tiles bei Spalte %d" % (MAX_DITCH + 2, tx))
        else:
            run = 0
    # Kletterbarkeit: Solid-Runs > MAX_STEP_UP ueber lokalem Boden
    for tx in range(L.W):
        g = L.ground_top(tx)
        if g is None: continue
        for y in range(g + 1, CHUNK_H):
            if L.solid(tx, y) and (y - g) > MAX_STEP_UP:
                errs.append("Unkletterbare Mauer bei Spalte %d (Hoehe %d > %d)" % (tx, y - g, MAX_STEP_UP))
                break
    # Hazard-Runs auf Bodenhoehe
    run = 0
    for tx in range(1, L.W - 1):
        g = L.ground_top(tx)
        hz = L.hazard(tx, g) if g is not None else False
        if hz:
            run += 1
            if run > MAX_HAZARD_RUN:
                errs.append("Hazard-Run >%d bei Spalte %d" % (MAX_HAZARD_RUN, tx))
        else:
            run = 0
    # Musik-Zonen
    zs = sorted(L.music_zones)
    for i, (x0, x1, sub) in enumerate(zs):
        if not (0 <= x0 < x1 <= L.W):
            errs.append("Musik-Zone %d ausserhalb [0,%d]" % (i, L.W))
        if i and zs[i - 1][1] > x0:
            errs.append("Musik-Zonen %d/%d ueberlappen" % (i - 1, i))
    ents = L.entities()
    if len([e for e in ents if e[0] in "wtfG"]) > 32:
        errs.append("Mehr als 32 Gegner")
    if sum(1 for e in ents if e[0] == 'C') > 1:
        warns.append("Mehr als 1 Truhe 'C' (v1-Originals duldets)")
    return errs, warns

def validate(L, run_bot_test=True):
    errs, warns = static_checks(L)
    rep = dict(ok=(len(errs) == 0), errors=errs, warnings=warns, bot=None)
    if run_bot_test and not errs:
        rep["bot"] = run_bot(L)
        if not rep["bot"]["win"]:
            rep["ok"] = False
            rep["errors"].append("Bot nicht durch: %s nach %d Ticks" % (rep["bot"]["reason"], rep["bot"]["frames"]))
    return rep

# ================= Generator =================
def generate(seed, n_chunks=6, difficulty=1):
    rng = random.Random(seed)
    W = n_chunks * CHUNK_W
    grid = [['.'] * W for _ in range(CHUNK_H)]
    st = dict(x=8, last=None)

    def put(x, r, ch):
        if 0 <= x < W and 0 <= r < CHUNK_H:
            grid[r][x] = ch

    def floor(x0, x1):
        for x in range(x0, min(x1, W)):
            grid[CHUNK_H - 1][x] = '#'
            grid[CHUNK_H - 2][x] = '#'

    # Boden
    floor(0, W)
    put(2, CHUNK_H - 2, 'P')

    while st['x'] < W - 10:
        r = rng.random()
        if r < 0.28:
            n = rng.randint(3, 6)
            # flat: evtl Loot/Enemy
            if rng.random() < 0.5:
                put(st['x'] + n // 2, CHUNK_H - 2, '*')
            if rng.random() < 0.15 + 0.35 * (st['x'] / W):
                put(st['x'] + n // 2, CHUNK_H - 2, 'w')
            st['x'] += n; st['last'] = 'flat'
        elif r < 0.42:
            if st['last'] in ('pool', 'ditch'):
                floor(st['x'], st['x'] + 2); st['x'] += 2
            n = rng.randint(2, 6)  # <=6: bot-sicher (2x 3er-Sprung)
            for xx in range(st['x'], st['x'] + n):
                grid[CHUNK_H - 1][xx] = '.'
                grid[CHUNK_H - 2][xx] = '.'
            if rng.random() < 0.4:
                put(st['x'] + n // 2, CHUNK_H - 1, '*')
            st['x'] += n; st['last'] = 'ditch'
        elif r < 0.58:
            n = rng.randint(3, 6)
            for xx in range(st['x'], st['x'] + n):
                grid[CHUNK_H - 2][xx] = '^'
            if rng.random() < 0.5:
                py = CHUNK_H - 6
                for xx in range(st['x'] + 1, st['x'] + n - 1):
                    grid[py][xx] = '#'
                put(st['x'] + n // 2, py - 1, '*')
            st['x'] += n + 1; st['last'] = 'spikes'
        elif r < 0.70:
            n = rng.randint(4, 8)
            for xx in range(st['x'], st['x'] + n):
                for rr in (CHUNK_H - 3, CHUNK_H - 2):
                    grid[rr][xx] = '~'
            if rng.random() < 0.4:
                put(st['x'] + n // 2, CHUNK_H - 4, 'f')
            st['x'] += n; st['last'] = 'pool'
        elif r < 0.82:
            h = rng.randint(1, 2)
            w = rng.randint(2, 4)
            for xx in range(st['x'], st['x'] + w):
                for rr in range(CHUNK_H - 1 - h, CHUNK_H - 1):
                    grid[rr][xx] = '#'
            st['x'] += w + 2; st['last'] = 'steps'
        else:
            n = rng.randint(4, 7)
            py = CHUNK_H - rng.randint(5, 6)
            for xx in range(st['x'] + 1, st['x'] + n - 1):
                grid[py][xx] = '#'
            put(st['x'] + n // 2, py - 1, rng.choice('*D'))
            if rng.random() < 0.2 + 0.3 * (st['x'] / W):
                put(st['x'] + n // 2, CHUNK_H - 2, 'w')
            st['x'] += n; st['last'] = 'loot'
    # End-Zone: clean, Exit
    floor(st['x'], W)
    for xx in range(st['x'], W):
        for rr in range(CHUNK_H - 7, CHUNK_H - 1):
            if grid[rr][xx] in ('^', 'w', 't', 'f', 'G'):
                grid[rr][xx] = '.'
    put(W - 4, CHUNK_H - 2, 'E')
    # Gegner-Verteilung (Difficulty-Rampe)
    for _ in range(2 + difficulty):
        xx = rng.randint(10, W - 12)
        if grid[CHUNK_H - 2][xx] == '.':
            grid[CHUNK_H - 2][xx] = rng.choice(['w', 'w', 'f', 't', 'w'])
    # Truhe (max 1): Wand mit 2 leeren Feldern drueber
    if rng.random() < 0.4:
        placed = False
        for r in range(1, CHUNK_H - 1):
            if placed: break
            for xx in range(8, W - 10):
                if grid[r][xx] == '#' and grid[r + 1][xx] == '#' and \
                   grid[r - 1][xx] == '.' and grid[r - 2][xx] == '.' and \
                   grid[r - 1][xx - 1] == '.':
                    grid[r - 1][xx] = 'C'
                    placed = True
                    break
    # Musik-Zonen
    n_z = rng.randint(3, 5)
    zw = W // n_z
    zones = []
    for i in range(n_z):
        x0 = i * zw
        x1 = W if i == n_z - 1 else (i + 1) * zw
        zones.append([x0, x1, rng.randint(0, 3)])
    chunks = []
    for c in range(n_chunks):
        rows = []
        for r in range(CHUNK_H):
            rows.append(''.join(grid[r][c * CHUNK_W:(c + 1) * CHUNK_W]))
        chunks.append(rows)
    name = "Gen-%d" % seed
    return Level(name, chunks, zones, biome=rng.randint(0, 2), palette=rng.randint(0, 2),
                 seed=seed, time_limit=220 + 20 * difficulty,
                 notes="generator seed=%d difficulty=%d" % (seed, difficulty))

# ================= Handgemachte Levels =================
def _L(name, biome, palette, chunks, zones, notes=""):
    return Level(name, chunks, zones, biome=biome, palette=palette,
                 music_file="user_music/strudel_force.strudel", notes=notes)

HANDMADE = [
    _L("Strudel-Bad", 1, 1, [
        ["................","................","................","................",
         "................","................","................","................",
         "................","................","P.....*....*....","################"],
        ["................","................","................","................",
         "................","......f.........","................","................",
         "..~~~~~~........","..~~~~~~........","..~~~~~~......w.","################"],
        ["................","................","................",".....C..........",
         "....####........","..##............","..##............","................",
         "................","................","....w.....*.....","################"],
        ["................","................","................","................",
         "........f.......","................","................","........*.......",
         "..~~~~~~~~~~....","..~~~~~~~~~~....","..~~~~~~~~~~..*.","################"],
        ["................","................","................","......D.........",
         "......D.D.D.....","......####......","................","................",
         "................","................",".....^^^^^^.....","################"],
        ["................","................","................","................",
         "................","..........t.....","................","................",
         "................","................","..........E.....","################"],
    ], [[0, 16, 0], [16, 48, 1], [48, 80, 2], [80, 96, 3]],
       "handgemacht: Wasser, Pfosten, Kisten, Spikes mit Plattform"),

    _L("Magma-Schacht", 2, 2, [
        ["................","................","................","................",
         "................","................","................","................",
         "................","................","P....w..........","################"],
        ["................","................","................","................",
         "................","................","................","................",
         "................","................","....*....w......","####....########"],
        ["................","................","................","................",
         "................","....t.........f.","....####........","................",
         "................","##..............","##......*....w..","################"],
        ["................","................","................","......D.........",
         "......####......","................","................","................",
         "................","................","......^^^^^^....","################"],
        ["................","................","................","................",
         "................","................","................","................",
         "..~~~~~~........","..~~~~~~........","..~~~~~~....G...","################"],
        ["................","................","................","................",
         "................","................","................","................",
         "................","................","..........E.....","################"],
    ], [[0, 32, 1], [32, 64, 2], [64, 96, 0]],
       "handgemacht: Senke, Treppen, Plattform-Turm, Magma-Becken, Tank"),

    _L("Zitadelle-Wache", 2, 2, [
        ["................","................","................","................",
         "................","................","................","................",
         "................","................","P........*......","################"],
        ["................","................","................",".........---....",
         "................","....---.........","................","................",
         "........---.....","................","..........w.....","################"],
        ["................","................","................",".....D..DD......",
         "....####........","................","..........---...","..---...........",
         "................","................","..........w.....","################"],
        ["................","................","................","................",
         "................","................","................","....--..--..--..",
         "................","................","..t........t....","################"],
        ["................","................","................","................",
         "................","......*....*....","................","................",
         "..~~~~~~~.......","..~~~~~~~....w..","..........E.....","################"],
        ["................","................","................","................",
         "................","................","................","................",
         "................","................","................","################"],
    ], [[0, 32, 3], [32, 64, 2], [64, 96, 1]],
       "handgemacht: Onway-Treppen, Plattform-Loot, Turret-Gasse, Becken"),
]

# ================= Reparatur (Original-Chunks an C-Physik anpassen) =================
def repair_level(L):
    """Senkt unkletterbare Mauer-Runs auf MAX_STEP_UP (v1-Chunks waren fuer
    die alte Physik gebaut). Gibt (L, verandert) zurueck."""
    changed = False
    chunks = [list(row) for rows in L.chunks for row in rows]  # flach pro Chunk
    out = []
    for ci in range(len(L.chunks)):
        rows = [list(L.chunks[ci][r]) for r in range(CHUNK_H)]
        for cx in range(CHUNK_W):
            # hoechste begehbar von unten
            g = None
            for y in range(CHUNK_H - 1, -1, -1):
                if rows[CHUNK_H - 1 - y][cx] in WALKABLE:
                    g = y; break
            if g is None: continue
            for y in range(g + 1, CHUNK_H):
                r = CHUNK_H - 1 - y
                if rows[r][cx] in SOLID and (y - g) > MAX_STEP_UP:
                    rows[r][cx] = '.'
                    changed = True
        out.append([''.join(row) for row in rows])
    L2 = Level(L.name, out, [list(z) for z in L.music_zones], L.biome, L.palette,
               L.seed, L.music_type, L.music_file, L.time_limit, L.notes)
    return L2, changed

# ================= Export =================
ROGUE_ASSEMBLER = """/* Roguelike assembler - deterministic (Xorshift32). Mirror: tools/level_tool.py rl_build() */
static uint32_t lv_xorshift(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x; }
static int lv_find_flag(uint8_t f) { int i; for (i = 0; i < LV_NUM_CHUNKS; i++) if (LV_CHUNKS[i].flags & f) return i; return 0; }
static int lv_rl_build(uint32_t seed, uint8_t biome, int len, uint8_t *out) {
    int n = 0, prev, k, tries, chest = 0, start = lv_find_flag(LV_F_START), arena = lv_find_flag(LV_F_ARENA);
    if (len < 2) len = 2;
    if (len > LV_MAX_CHUNKS) len = LV_MAX_CHUNKS;
    if (!seed) seed = 0x5F0CEu;
    out[n++] = (uint8_t)start; prev = start;
    while (n < len - 1) {
        k = start;
        for (tries = 0; tries < 32; tries++) {
            const LvChunk *a = &LV_CHUNKS[prev], *b;
            k = (int)(lv_xorshift(&seed) % (uint32_t)LV_NUM_CHUNKS); b = &LV_CHUNKS[k];
            if (b->flags & (LV_F_ARENA | LV_F_START)) continue;
            if ((b->flags & LV_F_CHEST) && chest) continue;
            if (!((b->biome_mask >> biome) & 1)) continue;
            if (a->exit_r == LV_EXIT_NONE || a->exit_r != b->exit_l) continue;
            if (b->difficulty > 2 + n / 2) continue;
            if (k == prev) continue;
            break;
        }
        if (tries == 32) {
            int j; k = start;
            for (j = 0; j < LV_NUM_CHUNKS; j++) {
                const LvChunk *b = &LV_CHUNKS[j];
                if ((b->flags & (LV_F_ARENA | LV_F_START)) || j == prev) continue;
                if (((b->flags & LV_F_CHEST) && chest) || !((b->biome_mask >> biome) & 1)) continue;
                if (LV_CHUNKS[prev].exit_r != b->exit_l) continue;
                k = j; break;
            }
        }
        if (LV_CHUNKS[k].flags & LV_F_CHEST) chest = 1;
        out[n++] = (uint8_t)k; prev = k;
    }
    out[n++] = (uint8_t)arena;
    return n;
}"""

def _parse_original_header(path):
    """Liest Chunk-Metadaten (Exits/Flags) aus dem bestehenden level_data.h.c."""
    import re
    meta = {}
    if not os.path.exists(path):
        return meta
    s = open(path, encoding="utf-8").read()
    for m in re.finditer(r'\{"([^"]+)",\s*\{([^}]*)\}\s*\},\s*(\d+),(\d+),(\d+),(\d+),\s*(\d+),(\d+),(\d+)\}', s):
        meta[m.group(1)] = dict(
            exit_l=int(m.group(3)), exit_r=int(m.group(4)),
            exit_u=int(m.group(5)), exit_d=int(m.group(6)),
            difficulty=int(m.group(7)), biome_mask=int(m.group(8)),
            flags=int(m.group(9)))
    return meta

def _load_original_levels(path):
    """Liest Levels + Chunks aus levels.json (v1: chunks = LISTE mit
    {id, rows, exits:{l,r,u,d}, flags:[...], difficulty, biome_mask}).
    Gibt (levels, chunks) zurueck; (None, None) bei kaputter/fehlender Datei."""
    try:
        data = json.load(open(path, encoding="utf-8"))
        cl = data.get("chunks")
        if not isinstance(cl, list) or not cl or not isinstance(cl[0], dict):
            return None, None
    except Exception:
        return None, None
    FLAG_MAP = {"start": 8, "chest": 4, "arena": 1, "end": 16}
    chunks = {}
    for cdata in cl:
        cid = cdata.get("id")
        rows = cdata.get("rows")
        if not cid or not isinstance(rows, list) or len(rows) != CHUNK_H:
            continue
        ex = cdata.get("exits", {})
        fl = 0
        for f in cdata.get("flags", []):
            fl |= FLAG_MAP.get(f, 0)
        chunks[cid] = dict(rows=rows,
            exit_l=ex.get("l", 9), exit_r=ex.get("r", 9),
            exit_u=ex.get("u", 255), exit_d=ex.get("d", 255),
            difficulty=cdata.get("difficulty", 2),
            biome_mask=cdata.get("biome_mask", 15), flags=fl)
    levels = []
    for lv in data.get("levels", []):
        m = lv.get("music", {})
        levels.append(dict(
            name=lv["name"], biome=lv.get("biome", 0), palette=lv.get("palette", 0),
            music_type=MUSIC_TYPE_MAP.get(m.get("type", "strudel"), 0),
            music_file=m.get("file", "user_music/strudel_force.strudel"),
            music_subsong=m.get("subsong", 0),
            time_limit=lv.get("time_limit", 240),
            seq=lv.get("seq", []),
            chunk_rows={cid: chunks[cid]["rows"] for cid in lv.get("seq", []) if cid in chunks}))
    return levels, chunks

MUSIC_TYPE_MAP = {"strudel": 0, "tfmx": 1, "mod": 2, "sid": 3}

def export_all(levels, out_root=None):
    """Haengt Level an levels.json an (v1-List-Format, Originals bleiben
    unangetastet) und regeneriert levels_v2.json (Zonen) + src/level_data.h.c.
    Idempotent: Level mit vorhandenem Namen werden uebersprungen."""
    root = out_root or ROOT
    jpath = os.path.join(root, "levels", "levels.json")
    try:
        data = json.load(open(jpath, encoding="utf-8"))
        if not isinstance(data.get("chunks"), list) or not isinstance(data.get("levels"), list):
            raise ValueError("falsches Format")
    except Exception:
        data = {"format": "sflevels/1", "chunk_w": CHUNK_W, "chunk_h": CHUNK_H,
                "legend": {".": "air", "#": "solid", "B": "brick", "?": "bonus", "-": "oneway",
                           "^": "spike", "~": "water", "H": "ladder", "*": "crystal",
                           "C": "chest", "P": "spawn", "E": "exit", "w": "walker",
                           "f": "flyer", "t": "turret", "U": "powerup", "O": "pipe",
                           "M": "metal", "=": "conveyor", "o": "owl/eye-enemy",
                           "D": "diamond", "S": "spring", "G": "boss-core"},
                "biomes": ["chrome_tempest", "great_bath", "mechanical_hive", "dream_world"],
                "palettes": ["chrome_tempest", "great_bath", "mechanical_hive", "dream_world"],
                "chunks": [], "levels": []}
    zpath = os.path.join(root, "levels", "levels_v2.json")
    v2zones = {}
    try:
        for e in json.load(open(zpath, encoding="utf-8")).get("levels", []):
            v2zones[e["name"]] = e.get("music_zones")
    except Exception:
        pass
    FLAG_VAL = {"start": 8, "chest": 4, "arena": 1, "end": 16}
    id_by_rows = {}
    for c in data["chunks"]:
        if not c.get("flags"):
            id_by_rows.setdefault(tuple(c["rows"]), c["id"])
    known_ids = {c["id"] for c in data["chunks"]}
    seqs = []
    # 1) Original-Levels (unveraeindert, Zonen aus v2-Datei oder Default)
    for lv in data["levels"]:
        seq = [cid for cid in lv.get("seq", []) if cid in known_ids]
        if not seq:
            continue
        m = lv.get("music", {})
        seqs.append(dict(name=lv["name"], biome=lv.get("biome", 0), palette=lv.get("palette", 0),
                         music_type=MUSIC_TYPE_MAP.get(m.get("type", "strudel"), 0),
                         music_file=m.get("file", "user_music/strudel_force.strudel"),
                         time_limit=lv.get("time_limit", 240), seq=list(seq),
                         zones=v2zones.get(lv["name"]) or _default_zones(len(seq) * CHUNK_W),
                         notes=""))
    # 2) Neue Level (Dedupe: Name + Chunk-Rows)
    existing_names = {lv["name"] for lv in data["levels"]}
    n_new = 0
    for L in levels:
        if L.name in existing_names:
            continue
        seq = []
        for ci, rows in enumerate(L.chunks):
            key = tuple(rows)
            cid = id_by_rows.get(key)
            if cid is None:
                cid = "v2_%03d" % len(data["chunks"])
                fl = (["start"] if ci == 0 else []) + \
                     (["arena", "end"] if ci == len(L.chunks) - 1 else []) + \
                     (["chest"] if any("C" in row for row in rows) else [])
                data["chunks"].append({"id": cid, "biome_mask": 15,
                                       "difficulty": min(5, 1 + ci // 2),
                                       "exits": {"l": 9, "r": 9}, "flags": fl,
                                       "rows": list(rows)})
                id_by_rows[key] = cid
            seq.append(cid)
        data["levels"].append({"name": L.name, "biome": L.biome, "palette": L.palette,
                               "music": {"type": {0: "strudel", 1: "tfmx", 2: "mod", 3: "sid"}[L.music_type],
                                         "file": L.music_file},
                               "time_limit": L.time_limit, "vertical": False,
                               "seq": list(seq)})
        existing_names.add(L.name)
        n_new += 1
        seqs.append(dict(name=L.name, biome=L.biome, palette=L.palette,
                         music_type=L.music_type, music_file=L.music_file,
                         time_limit=L.time_limit, seq=list(seq),
                         zones=L.music_zones, notes=L.notes))
    # Zurueckschreiben (Originals unveraendert)
    os.makedirs(os.path.join(root, "levels"), exist_ok=True)
    json.dump(data, open(jpath, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    # levels_v2.json (Zonen fuer alle Level)
    v2 = {"format": "sflevels/2", "levels": []}
    for lv in seqs:
        v2["levels"].append({"name": lv["name"],
                             "music_zones": [list(z) for z in lv["zones"]],
                             "notes": lv.get("notes", ""), "cutscenes": [], "next": None})
    json.dump(v2, open(zpath, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    # level_data.h.c
    idx_by_id = {c["id"]: i for i, c in enumerate(data["chunks"])}
    h = []
    h.append("/* level_data.h - GENERATED by levelkit/lv2.py (v2). DO NOT EDIT. */")
    h.append("#ifndef LEVEL_DATA_H")
    h.append("#define LEVEL_DATA_H")
    h.append("#include <stdint.h>")
    h.append("#define LV_CHUNK_W 16")
    h.append("#define LV_CHUNK_H 12")
    h.append("#define LV_MAX_CHUNKS 24")
    h.append("#define LV_EXIT_NONE 255")
    h.append("#define LV_F_ARENA 1")
    h.append("#define LV_F_CHEST 4")
    h.append("#define LV_F_START 8")
    h.append("#define LV_F_END 16")
    h.append("typedef struct { const char *id; const char *rows[LV_CHUNK_H];")
    h.append("                 uint8_t exit_l, exit_r, exit_u, exit_d, difficulty, biome_mask, flags; } LvChunk;")
    h.append("typedef struct { const char *name; uint8_t biome, palette, vertical, music_type; const char *music_file;")
    h.append("                 uint8_t music_subsong, n_chunks; const uint8_t *seq; uint16_t time_limit; } LvLevel;")
    h.append("enum { LV_MUSIC_STRUDEL = 0, LV_MUSIC_TFMX = 1, LV_MUSIC_MOD = 2, LV_MUSIC_SID = 3 };")
    h.append("static const LvChunk LV_CHUNKS[] = {")
    for c in data["chunks"]:
        ex = c.get("exits", {})
        fl = 0
        for f in c.get("flags", []):
            fl |= FLAG_VAL.get(f, 0)
        rs = ",".join('"%s"' % r for r in c["rows"])
        h.append(' {"%s", {%s}, %d,%d,%d,%d, %d,%d,%d},' % (
            c["id"], rs, ex.get("l", 9), ex.get("r", 9), ex.get("u", 255), ex.get("d", 255),
            c.get("difficulty", 2), c.get("biome_mask", 15), fl))
    h.append("};")
    h.append("#define LV_NUM_CHUNKS %d" % len(data["chunks"]))
    for i, lv in enumerate(seqs):
        h.append("static const uint8_t LV_SEQ_%d[] = {%s};" % (
            i, ",".join(str(idx_by_id[cid]) for cid in lv["seq"])))
    h.append("static const LvLevel LV_LEVELS[] = {")
    for i, lv in enumerate(seqs):
        h.append(' {"%s", %d, %d, 0, %d, "%s", 0, %d, LV_SEQ_%d, %d},' % (
            lv["name"], lv["biome"], lv["palette"], lv["music_type"], lv["music_file"],
            len(lv["seq"]), i, lv["time_limit"]))
    h.append("};")
    h.append("#define LV_NUM_LEVELS %d" % len(seqs))
    h.append("/* v2: Musik-Zonen (X-Bereiche in Tiles, 0..level_w) */")
    h.append("typedef struct { uint16_t x0, x1; uint8_t subsong, flags; } LvMusicZone;")
    for i, lv in enumerate(seqs):
        h.append("static const LvMusicZone LV_MZ_%d[] = {" % i)
        for z in lv["zones"]:
            h.append(" {%d, %d, %d, 0}," % (z[0], z[1], z[2]))
        h.append("};")
    h.append("static const LvMusicZone *LV_MZONES[LV_NUM_LEVELS] = {")
    h.append(" " + ",".join("LV_MZ_%d" % i for i in range(len(seqs))) + "};")
    h.append("static const uint8_t LV_MZONE_COUNT[LV_NUM_LEVELS] = {")
    h.append(" " + ",".join(str(len(lv["zones"])) for lv in seqs) + "};")
    h.append(ROGUE_ASSEMBLER)
    h.append("#endif")
    os.makedirs(os.path.join(root, "src"), exist_ok=True)
    open(os.path.join(root, "src", "level_data.h.c"), "w", encoding="utf-8").write("\n".join(h) + "\n")
    return dict(chunks=len(data["chunks"]), levels=len(data["levels"]), new_levels=n_new)

def _default_zones(W):
    n = 3
    zw = max(1, W // n)
    out = []
    for i in range(n):
        x0 = i * zw
        x1 = W if i == n - 1 else (i + 1) * zw
        out.append([x0, x1, i % 4])
    return out

# ================= Preview (PIL optional) =================
PALETTES = [
    dict(sky=(20, 24, 48),  solid=(90, 110, 140),  top=(150, 170, 200),  accent=(80, 200, 255),  hazard=(255, 80, 60),  water=(40, 90, 220)),
    dict(sky=(16, 40, 44),  solid=(70, 120, 110),  top=(120, 180, 160),  accent=(120, 255, 200), hazard=(255, 120, 60), water=(30, 140, 160)),
    dict(sky=(40, 18, 18),  solid=(120, 70, 60),   top=(180, 120, 90),   accent=(255, 160, 40),  hazard=(255, 60, 30),  water=(200, 80, 20)),
]

def preview_png(L, out, scale=2):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("PIL fehlt - Preview uebersprungen")
        return None
    pal = PALETTES[L.palette % len(PALETTES)]
    W = L.W
    img = Image.new("RGB", (W * 16 * scale, CHUNK_H * 16 * scale), pal["sky"])
    d = ImageDraw.Draw(img)
    s = 16 * scale
    for r in range(CHUNK_H):
        for c in range(W):
            t = L.t(c, CHUNK_H - 1 - r)
            if t == '.':
                continue
            x0 = c * s; y0 = r * s
            if t in SOLID:
                d.rectangle([x0, y0, x0 + s - 1, y0 + s - 1], fill=pal["solid"])
                above = L.t(c, CHUNK_H - r)
                if above not in SOLID:
                    d.rectangle([x0, y0, x0 + s - 1, y0 + s // 4], fill=pal["top"])
            elif t in ONEWAY:
                d.rectangle([x0, y0, x0 + s - 1, y0 + s // 3], fill=pal["top"])
            elif t in HAZARD:
                d.polygon([(x0, y0 + s), (x0 + s // 2, y0), (x0 + s, y0 + s)], fill=pal["hazard"])
            elif t in WATER:
                d.rectangle([x0, y0, x0 + s - 1, y0 + s - 1], fill=pal["water"])
                d.line([x0, y0 + s // 4, x0 + s - 1, y0 + s // 4], fill=pal["top"], width=max(1, scale))
            elif t == '*':
                d.ellipse([x0 + s // 3, y0 + s // 3, x0 + 2 * s // 3, y0 + 2 * s // 3], fill=(255, 220, 80))
            elif t == 'D':
                d.polygon([(x0 + s // 2, y0), (x0 + s, y0 + s // 2), (x0 + s // 2, y0 + s), (x0, y0 + s // 2)], fill=(120, 255, 255))
            elif t == 'P':
                d.rectangle([x0 + s // 4, y0 + s // 6, x0 + 3 * s // 4, y0 + s - 1], fill=(90, 160, 255))
            elif t == 'E':
                d.rectangle([x0 + s // 2, y0, x0 + 2 * s // 3, y0 + s - 1], fill=(255, 255, 255))
                d.rectangle([x0 + 2 * s // 3, y0, x0 + s, y0 + s // 3], fill=(255, 60, 60))
            elif t in "wtfG":
                col = {"w": (255, 90, 90), "t": (255, 160, 40), "f": (120, 220, 255), "G": (200, 60, 200)}[t]
                d.rectangle([x0 + s // 5, y0 + s // 5, x0 + 4 * s // 5, y0 + 4 * s // 5], fill=col)
            elif t == 'C':
                d.rectangle([x0 + s // 5, y0 + s // 3, x0 + 4 * s // 5, y0 + 4 * s // 5], fill=(220, 170, 60))
    # Musik-Zonen-Overlay (dezente Bänder oben)
    for (x0, x1, sub) in L.music_zones:
        d.rectangle([x0 * s, 0, x1 * s - 1, max(2, s // 8)], fill=(255, 200 + 30 * (sub % 2), 120))
    if out:
        os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
        img.save(out)
    return out

def tiles_png(out, scale=4):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return None
    demo = Level("demo", [
        ["................","................","................","....####........",
         "....*D*....C....","................","..........---...","................",
         "..~~^^~~..........","..~~^^~~..w.....","P....t....f.....","################"],
    ], [[0, 16, 0]], palette=0)
    return preview_png(demo, out, scale=scale)

# ================= Editor (Text-CLI, headless) =================
def edit(L, save_path):
    print("STRUDELFORCE Level-Editor (Text-CLI) - %s" % L.name)
    print("Befehle: h=Hilfe g=Grid s=set m=zone t=test p=preview e=export q[ y]=Quit")
    while True:
        cmd = input("lv> ").strip().split()
        if not cmd:
            continue
        c = cmd[0]
        if c == 'h':
            print("g                     Grid anzeigen")
            print("s <chunk> <x> <y> <ch>  Tile setzen (y: 0=oben .. 11=unten)")
            print("m <x0> <x1> <subsong>  Musik-Zone setzen (Tiles)")
            print("t                     Bot-Test")
            print("p [datei]             Preview PNG")
            print("e                     Export (json + header)")
            print("q [y]                 Beenden (y=speichern)")
        elif c == 'g':
            for ci, rows in enumerate(L.chunks):
                print("Chunk %d" % ci)
                print("     " + "0123456789ABCDEF")
                for r, row in enumerate(rows):
                    print("%2d   %s" % (r, row))
        elif c == 's' and len(cmd) == 5:
            ci, x, y = int(cmd[1]), int(cmd[2]), int(cmd[3])
            ch = cmd[4]
            if 0 <= ci < len(L.chunks) and 0 <= x < CHUNK_W and 0 <= y < CHUNK_H:
                rows = list(L.chunks[ci][y])
                rows[x] = ch
                L.chunks[ci] = ''.join(rows)
                print("gesetzt (%d,%d,%d)=%s" % (ci, x, y, ch))
        elif c == 'm' and len(cmd) == 4:
            x0, x1, sub = int(cmd[1]), int(cmd[2]), int(cmd[3])
            L.music_zones = [z for z in L.music_zones if not (z[1] > x0 and z[0] < x1)]
            L.music_zones.append([x0, x1, sub])
            L.music_zones.sort()
            print("Zonen:", L.music_zones)
        elif c == 't':
            rep = validate(L)
            print("OK" if rep["ok"] else "FEHLER")
            for e in rep["errors"]: print("  -", e)
            if rep["bot"]: print("  Bot:", rep["bot"])
        elif c == 'p':
            out = cmd[1] if len(cmd) > 1 else os.path.join(ROOT, "assets", "preview_edit.png")
            print(preview_png(L, out) or "Fehler")
        elif c == 'e':
            print(export_all([L]))
        elif c == 'q':
            if len(cmd) > 1 and cmd[1] == 'y':
                export_all([L])
                print("gespeichert")
            break
        else:
            print("Unbekannt - 'h' fuer Hilfe")

# ================= Level-Sammlung =================
def collect_levels(gen_count=0, seed_base=1000):
    """Original-9 (repariert) + 3 handgemacht + N generiert."""
    levels = []
    try:
        data = json.load(open(os.path.join(ROOT, "levels", "levels.json"), encoding="utf-8"))
        chunks = {c["id"]: c for c in data.get("chunks", []) if isinstance(c, dict)}
        hm = {L.name for L in HANDMADE}
        for lv in data.get("levels", []):
            if lv.get("name") in hm or str(lv.get("name", "")).startswith("Gen-"):
                continue  # v2-Level: frisch generiert bzw. doppelt
            rows_all = []
            ok = True
            for cid in lv.get("seq", []):
                if cid in chunks:
                    rows_all.append(chunks[cid]["rows"])
                else:
                    ok = False; break
            if not ok:
                continue
            L = Level(lv["name"], rows_all,
                      biome=lv.get("biome", 0), palette=lv.get("palette", 0),
                      music_type=MUSIC_TYPE_MAP.get(lv.get("music", {}).get("type", "strudel"), 0),
                      music_file=lv.get("music", {}).get("file", "user_music/strudel_force.strudel"),
                      time_limit=lv.get("time_limit", 240))
            L, _ = repair_level(L)
            W = L.W
            if not L.music_zones:
                n = 3
                zw = max(1, W // n)
                L.music_zones = [[i * zw, (W if i == n - 1 else (i + 1) * zw), i % 4] for i in range(n)]
            levels.append(L)
    except Exception as e:
        print("Original-Levels nicht ladbar: %s" % e)
    levels.extend(HANDMADE)
    for i in range(gen_count):
        levels.append(generate(seed_base + i, n_chunks=random.Random(seed_base + i).randint(5, 8),
                               difficulty=1 + (i % 3)))
    return levels

# ================= CLI =================
def main():
    args = sys.argv[1:]
    cmd = args[0] if args else "selftest"
    if cmd == "reach":
        print(json.dumps(reach_report(), indent=1))
    elif cmd == "gen":
        n = int(args[args.index("--count") + 1]) if "--count" in args else 5
        s = int(args[args.index("--seed") + 1]) if "--seed" in args else 1000
        for i in range(n):
            L = generate(s + i)
            rep = validate(L)
            print("%-14s %s  bot=%s" % (L.name, "OK" if rep["ok"] else "ERR", rep["bot"]))
    elif cmd == "test":
        levels = collect_levels(gen_count=10)
        sel = None
        if "--level" in args:
            sel = int(args[args.index("--level") + 1])
            levels = [levels[sel]]
        wins = 0
        for i, L in enumerate(levels):
            rep = validate(L)
            b = rep["bot"]
            if b and b["win"]: wins += 1
            tag = "OK " if rep["ok"] else "ERR"
            bot = ("win %4d" % b["frames"]) if b and b["win"] else (b["reason"][:40] if b else "-")
            print("%2d %-28s %s  %s" % (i, L.name[:28], tag, bot))
        print("Bot-Winrate: %d/%d" % (wins, len(levels)))
    elif cmd == "export":
        levels = collect_levels(gen_count=5)
        print(export_all(levels))
    elif cmd == "preview":
        levels = collect_levels(gen_count=0)
        i = int(args[args.index("--level") + 1]) if "--level" in args else len(levels) - 1
        out = args[args.index("--out") + 1] if "--out" in args else os.path.join(ROOT, "assets", "preview_gen7.png")
        print(preview_png(levels[i], out) or "Fehler")
    elif cmd == "tiles":
        out = os.path.join(ROOT, "assets", "tiles_v2.png")
        print(tiles_png(out) or "Fehler")
    elif cmd == "edit":
        levels = collect_levels(gen_count=0)
        i = int(args[args.index("--level") + 1]) if "--level" in args else 0
        edit(levels[i], os.path.join(ROOT, "levels", "levels_v2.json"))
    elif cmd == "selftest":
        print("== Reach-Oracle (C-Physik) ==")
        print(json.dumps(reach_report()))
        levels = collect_levels(gen_count=25)
        wins, errs = 0, 0
        for i, L in enumerate(levels):
            rep = validate(L)
            b = rep["bot"]
            if b and b["win"]: wins += 1
            if not rep["ok"]: errs += 1
            tag = "OK " if rep["ok"] else "ERR"
            bot = ("win %4d" % b["frames"]) if b and b["win"] else (b["reason"][:44] if b else "-")
            print("%2d %-28s %s  %s" % (i, L.name[:28], tag, bot))
        print("Bot-Winrate: %d/%d  (Static-Errors: %d)" % (wins, len(levels), errs))
        print("== Export ==")
        print(export_all(levels))
        print("== Previews ==")
        print(preview_png(levels[-1], os.path.join(ROOT, "assets", "preview_gen7.png")) or "-")
        print(tiles_png(os.path.join(ROOT, "assets", "tiles_v2.png")) or "-")
    else:
        print(__doc__)

if __name__ == "__main__":
    main()
