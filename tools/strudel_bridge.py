#!/usr/bin/env python3
"""strudel_bridge.py - one song text format for all STRUDELFORCE / SGC games

sfsong/1 (JSON) = Strudel mini-notation tracks + optional module sources (TFMX/MOD/SID).

  strudel_bridge.py tomod   song.sfsong.json out.mod        # Strudel -> ProTracker MOD (4ch, synth samples)
  strudel_bridge.py tfmx2s  file.tfx [--sub N] [--bars 8]  # TFMX note trace -> sfsong JSON (stdout)
  strudel_bridge.py sgc     <SGC-Command checkout dir>      # harvest note("...")/s("...") strings -> sfsong
  strudel_bridge.py events  "c2 [c2 eb2] <f2 g2> ~"         # debug: print parsed events

Mini-notation subset (matches src/strudel_cleanroom.h scope):
  notes c2 eb3 f#4, rests ~ or -, [sub sequence], <alternation per cycle>, x*n repeat, x!n replicate.
"""
import sys, os, re, json, struct, math, random

NOTE = {"c":0,"d":2,"e":4,"f":5,"g":7,"a":9,"b":11}

def note_to_midi(tok):
    m = re.fullmatch(r"([a-gA-G])([#sb]*)(-?\d)?", tok)
    if not m: return None
    n = NOTE[m.group(1).lower()]
    for ch in m.group(2): n += 1 if ch in "#s" else -1
    octv = int(m.group(3)) if m.group(3) else 3
    return (octv + 1) * 12 + n

# ---------------- mini-notation parser ----------------
def tokenize(s):
    return re.findall(r"\[|\]|<|>|\*\d+|!\d+|[^\s\[\]<>*!]+", s)

def parse(tokens, i=0, close=None):
    items = []
    while i < len(tokens):
        t = tokens[i]
        if t == close: return items, i + 1
        if t == "[": node, i = parse(tokens, i + 1, "]"); node = ("seq", node)
        elif t == "<": node, i = parse(tokens, i + 1, ">"); node = ("alt", node)
        else: node = ("atom", t); i += 1
        while i < len(tokens) and tokens[i][0] in "*!":
            n = int(tokens[i][1:]); i += 1
            if tokens[i-1][0] == "*": node = ("seq", [node] * n)
            else: items.extend([node] * (n - 1))
        items.append(node)
    return items, i

def events(pattern, cycle=0):
    """-> list of (start, dur, midi|None) with start/dur as fractions of one cycle"""
    tree, _ = parse(tokenize(pattern))
    out = []
    def walk(node, t0, dur):
        kind = node[0]
        if kind == "atom":
            tok = node[1]
            out.append((t0, dur, None if tok in ("~", "-", "_") else note_to_midi(tok)))
        elif kind == "seq":
            if not node[1]: return
            d = dur / len(node[1])
            for k, ch in enumerate(node[1]): walk(ch, t0 + k * d, d)
        elif kind == "alt":
            if node[1]: walk(node[1][cycle % len(node[1])], t0, dur)
    walk(("seq", tree), 0.0, 1.0)
    return out

def grid(pattern, cycle, steps):
    g = [None] * steps
    for st, du, n in events(pattern, cycle):
        k = int(round(st * steps))
        if n is not None and 0 <= k < steps: g[k] = n
    return g

# ---------------- Strudel -> MOD ----------------
PT_PER = [856,808,762,720,678,640,604,570,538,508,480,453,
          428,404,381,360,340,320,302,285,269,254,240,226,
          214,202,190,180,170,160,151,143,135,127,120,113]   # C-1..B-3 (MIDI 36..71 here)

def midi_to_period(n):
    while n < 36: n += 12
    while n > 71: n -= 12
    return PT_PER[n - 36]

def synth(kind, length=64):
    rnd = random.Random(1234)
    if kind == "saw":    return bytes(((i * 256 // length) - 128) & 0xFF for i in range(length))
    if kind == "square": return bytes((100 if i < length // 2 else -100) & 0xFF for i in range(length))
    if kind == "sine":   return bytes(int(110 * math.sin(2 * math.pi * i / length)) & 0xFF for i in range(length))
    if kind == "noise":  # decaying noise hit, one-shot
        return bytes(int(rnd.randint(-120, 120) * math.exp(-i / 500.0)) & 0xFF for i in range(2000))
    if kind == "kick":
        return bytes(int(120 * math.exp(-i / 900.0) * math.sin(2 * math.pi * (60 * i / 8000.0 + 40 * math.exp(-i / 300.0)))) & 0xFF for i in range(2400))
    raise ValueError(kind)

def song_to_mod(song):
    tracks = song["tracks"][:4]
    steps = int(song.get("steps_per_cycle", 16))
    cycles = int(song.get("cycles", 4))
    rows_total = steps * cycles
    npat = (rows_total + 63) // 64
    insts = []
    for t in tracks:
        kind = t.get("instrument", "saw")
        if kind not in insts: insts.append(kind)
    hdr = bytearray(song.get("title", "strudelforce")[:20].encode("latin-1").ljust(20, b"\0"))
    datas = []
    for i in range(31):
        if i < len(insts):
            d = synth(insts[i]); datas.append(d)
            loop = insts[i] not in ("noise", "kick")
            hdr += insts[i].encode().ljust(22, b"\0") + struct.pack(">HBBHH", len(d) // 2, 0, 64,
                                                                    0, (len(d) // 2) if loop else 1)
        else:
            hdr += b"\0" * 22 + struct.pack(">HBBHH", 0, 0, 0, 0, 1)
    hdr += bytes([npat, 127]) + bytes(range(npat)) + bytes(128 - npat) + b"M.K."
    pats = bytearray(npat * 1024)
    for ch, t in enumerate(tracks):
        si = insts.index(t.get("instrument", "saw")) + 1
        vol = max(0, min(64, int(t.get("gain", 48))))
        for cyc in range(cycles):
            g = grid(t["pattern"], cyc, steps)
            for k, n in enumerate(g):
                if n is None: continue
                row = cyc * steps + k
                o = (row // 64) * 1024 + (row % 64) * 16 + ch * 4
                per = midi_to_period(n)
                pats[o:o+4] = bytes([(si & 0xF0) | (per >> 8), per & 0xFF, ((si & 15) << 4) | 0xC, vol])
    # tempo: 16 steps per cycle, cycle length from bpm -> speed 6 / F-command bpm on row 0 ch0 if free
    bpm = int(song.get("bpm", 125))
    if pats[2] & 0x0F == 0 and pats[3] == 0:
        pats[2] = (pats[2] & 0xF0) | 0xF; pats[3] = max(32, min(255, bpm))
    if rows_total % 64:   # pattern break after last used row
        row = rows_total; o = (row // 64) * 1024 + (row % 64 - 1) * 16 + 3 * 4
        pats[o+2] = (pats[o+2] & 0xF0) | 0xD; pats[o+3] = 0
    return bytes(hdr) + bytes(pats) + b"".join(datas)

# ---------------- TFMX -> Strudel ----------------
def tfmx_to_song(path, sub=0, bars=8):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import tfmx_oracle as T
    d = open(path, "rb").read(); sam = None
    if path.lower().endswith(".tfx") and os.path.exists(path[:-4] + ".sam"): sam = open(path[:-4] + ".sam", "rb").read()
    t = T.Tfmx(d, sam); t.start(sub); ev = []
    orig = t.note_port
    def spy(b0, b1, b2, b3):
        if b0 < 0xC0: ev.append((t.jiffy, b2 & 7, (b0 & 0x3F) + 24))
        return orig(b0, b1, b2, b3)
    t.note_port = spy
    rowj = max(1, t.speed + 1); steps = 16
    t.render(int(16574 * 0.02 * rowj * steps * bars) + 400)
    voices = {}
    for j, v, n in ev:
        voices.setdefault(v, {})[j // rowj] = n
    names = ["c","c#","d","eb","e","f","f#","g","ab","a","bb","b"]
    tracks = []
    for v in sorted(voices):
        cyc = []
        for b in range(bars):
            row = [voices[v].get(b * steps + k) for k in range(steps)]
            cyc.append("[" + " ".join("~" if n is None else "%s%d" % (names[n % 12], n // 12 - 1) for n in row) + "]")
        tracks.append({"name": "voice%d" % v, "pattern": "<" + " ".join(cyc) + ">", "instrument": "saw", "gain": 40})
    return {"format": "sfsong/1", "title": os.path.basename(path), "bpm": 125, "steps_per_cycle": 16,
            "cycles": bars, "tracks": tracks,
            "sources": [{"type": "tfmx", "path": path, "subsong": sub, "gain": 256}],
            "note": "pitch/rhythm only; timbre lives in the TFMX macros. MIDI = TFMX note + 24."}

# ---------------- SGC-Command harvest ----------------
SGC_RX = re.compile(r"""\b(note|n|s|sound)\(\s*["'`]([^"'`]+)["'`]\s*\)""")
def sgc_harvest(root):
    tracks = []
    for r, _, fs in os.walk(root):
        for f in sorted(fs):
            if not f.lower().endswith((".js", ".ts", ".strudel", ".txt", ".json", ".py", ".md")): continue
            try: src = open(os.path.join(r, f), encoding="utf-8", errors="replace").read()
            except OSError: continue
            for k, (fn, pat) in enumerate(SGC_RX.findall(src)):
                inst = "noise" if fn in ("s", "sound") else "saw"
                tracks.append({"name": "%s#%d" % (f, k), "pattern": pat, "instrument": inst, "gain": 40,
                               "origin": os.path.relpath(os.path.join(r, f), root), "kind": fn})
    return {"format": "sfsong/1", "title": "SGC-Command import", "bpm": 125, "steps_per_cycle": 16,
            "cycles": 4, "tracks": tracks, "sources": []}

if __name__ == "__main__":
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(0)
    if a[0] == "tomod":
        open(a[2], "wb").write(song_to_mod(json.load(open(a[1])))); print("wrote", a[2])
    elif a[0] == "tfmx2s":
        sub = int(a[a.index("--sub")+1]) if "--sub" in a else 0
        bars = int(a[a.index("--bars")+1]) if "--bars" in a else 8
        print(json.dumps(tfmx_to_song(a[1], sub, bars), indent=1))
    elif a[0] == "sgc":
        print(json.dumps(sgc_harvest(a[1]), indent=1))
    elif a[0] == "events":
        for c in range(2): print(c, events(a[1], c))
