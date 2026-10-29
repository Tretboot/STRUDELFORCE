#!/usr/bin/env python3
"""selftest.py - zero-gcc test suite (runs in Kiwi AND locally)

  python3 tools/selftest.py            # all
  python3 tools/selftest.py mod|strudel|tfmx|levels|lint|paths
"""
import sys, os, json, re, tempfile, shutil, zipfile, struct
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "audio"))
sys.path.insert(0, os.path.join(HERE, "kiwi"))
sys.path.insert(0, os.path.join(HERE, "generators"))
sys.path.insert(0, os.path.join(HERE, "analysis"))
FAILS = []
def check(name, cond, info=""):
    print("  [%s] %s %s" % ("OK" if cond else "FAIL", name, info))
    if not cond: FAILS.append(name)

def t_strudel():
    import strudel_bridge as S
    ev = S.events("c2 [c2 eb2] <f2 g2> ~", 0)
    check("strudel events count", len(ev) == 5, str(len(ev)))
    check("strudel alternation", S.events("<c3 d3>", 1)[0][2] == 50)
    check("strudel *n", len(S.events("c3*4")) == 4)
    check("note midi c4=60", S.note_to_midi("c4") == 60)

def t_mod():
    import strudel_bridge as S, mod_oracle as M
    song = {"format": "sfsong/1", "title": "great bath", "bpm": 125, "steps_per_cycle": 16, "cycles": 4,
            "tracks": [{"name": "bass", "pattern": "<c2 eb2 g2 f2> [g2 c3 eb3 g3]*2 <ab1 bb1 c2 d2>", "instrument": "saw", "gain": 50},
                       {"name": "lead", "pattern": "[c4 ~ eb4 g4] [~ f4 ~ eb4]", "instrument": "square", "gain": 30},
                       {"name": "kick", "pattern": "c3 ~ c3 ~", "instrument": "kick", "gain": 64},
                       {"name": "hat", "pattern": "[~ c5]*4", "instrument": "noise", "gain": 24}]}
    data = S.song_to_mod(song)
    m = M.Mod(data); m.start(16574); pcm = m.render(16574 * 9)  # FIX v3.1: Song endet ~7.7s, 6s reichen nicht fuer den D-Break-Loop
    peak = max(abs(x) for x in pcm)
    check("strudel->mod loads", m.nch == 4 and m.songlen == 1, "ch=%d len=%d" % (m.nch, m.songlen))
    check("mod renders audio", peak > 1000, "peak=%d" % peak)
    check("mod note events", len(m.events) > 40, "events=%d" % len(m.events))
    check("mod loops (D-break)", m.loops >= 1, "loops=%d" % m.loops)
    for bad in (b"x" * 2000, data[:1083]):
        try: M.Mod(bad); check("mod rejects garbage", False)
        except M.LoadError: check("mod rejects garbage", True)
    return data

def t_tfmx():
    import tfmx_oracle as T, strudel_bridge as S
    zp = os.path.join(ROOT, "..", "STRUDELFORCE_Claude_Handoff.zip")
    samples = os.path.join(ROOT, "tests", "data")
    tfx = os.path.join(samples, "Turrican 2 - World 1 - Dessert.tfx")
    if not os.path.exists(tfx): check("tfmx test data present", False, tfx); return
    t = T.Tfmx(open(tfx, "rb").read(), open(tfx[:-4] + ".sam", "rb").read()); t.start(0)
    pcm = t.render(16574 * 5)
    check("tfmx T2 renders", max(map(abs, pcm)) > 2000 and t.oob == 0, "oob=%d" % t.oob)
    song = S.tfmx_to_song(tfx, 0, 4)
    check("tfmx->strudel tracks", len(song["tracks"]) >= 2, str(len(song["tracks"])))
    data = S.song_to_mod(song)
    import mod_oracle as M
    m = M.Mod(data); m.start(); check("tfmx->strudel->mod roundtrip", max(map(abs, m.render(16574 * 3))) > 500)

def t_levels():
    import level_tool as L
    db = L.load(); e = L.validate(db)
    check("levels validate", not e, "; ".join(e[:3]))
    a = L.rl_build(db, 1337, 2, 8); b = L.rl_build(db, 1337, 2, 8)
    check("roguelike deterministic", a == b)
    check("roguelike start/arena", "start" in db["chunks"][a[0]]["flags"] and "arena" in db["chunks"][a[-1]]["flags"])
    ok = True
    for seed in range(1, 300):
        for biome in range(4):
            s = L.rl_build(db, seed, biome, 10)
            for x, y in zip(s, s[1:]):
                if db["chunks"][x]["exits"].get("r") != db["chunks"][y]["exits"].get("l"): ok = False
            if sum("chest" in db["chunks"][k]["flags"] for k in s) > 1: ok = False
    check("roguelike seams+chest rule (1200 runs)", ok)

def c_lint(path):
    """tiny C89 smell linter: // comments, decl after statement (heuristic), unbalanced braces"""
    src = open(path, encoding="utf-8", errors="replace").read()
    nc = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    nc = re.sub(r'"(\\.|[^"\\])*"', '""', nc); nc = re.sub(r"'(\\.|[^'\\])'", "''", nc)
    issues = []
    if "//" in nc: issues.append("// comment")
    if nc.count("{") != nc.count("}"): issues.append("braces %d/%d" % (nc.count("{"), nc.count("}")))
    if nc.count("(") != nc.count(")"): issues.append("parens")
    if re.search(r"\bfor\s*\(\s*(int|uint\w+|size_t)\s", nc): issues.append("for(int ...) is C99")
    return issues

def t_lint():
    files = [os.path.join(ROOT, "src", f) for f in ("soundengine_core.c", "engine_core.c", "game_logic.c", "strudelforce.h")]
    for f in files:
        if os.path.exists(f): iss = c_lint(f); check("c89 lint " + os.path.basename(f), not iss, ",".join(iss))
        else: check("exists " + os.path.basename(f), False)
    try:
        import pycparser  # noqa
        check("pycparser available", True)
    except ImportError:
        print("  [SKIP] pycparser not installed - real parse happens locally (make c89_check)")

def t_paths():
    """paths with spaces, umlauts, deep dirs, mdat./smpl. naming, shared .sam -> real files on disk"""
    import tfmx_oracle as T
    src = os.path.join(ROOT, "tests", "data")
    tmp = tempfile.mkdtemp(prefix="sf path test ")
    try:
        deep = os.path.join(tmp, "Müsik Ordner", "TFMX (Pro)", "World 1")
        os.makedirs(deep)
        shutil.copy(os.path.join(src, "Turrican 2 - World 1 - Dessert.tfx"), os.path.join(deep, "mdat.T2 Wüste"))
        shutil.copy(os.path.join(src, "Turrican 2 - World 1 - Dessert.sam"), os.path.join(deep, "smpl.T2 Wüste"))
        mods = list(T.iter_modules(tmp))
        check("path: mdat./smpl. pair found in umlaut+space dir", len(mods) == 1 and mods[0][2] is not None)
        t = T.Tfmx(mods[0][1], mods[0][2]); t.start(0)
        check("path: plays from deep path", max(map(abs, t.render(16574))) > 0)
        zp = os.path.join(tmp, "pack test.zip")
        with zipfile.ZipFile(zp, "w") as z:
            z.write(os.path.join(deep, "mdat.T2 Wüste"), "a b/mdat.T2 Wüste"); z.write(os.path.join(deep, "smpl.T2 Wüste"), "a b/smpl.T2 Wüste")
        check("path: zip with spaces/umlauts", len(list(T.iter_modules(zp))) == 1)
        import kiwi_handoff as K
        root = os.path.join(tmp, "hand off"); os.makedirs(os.path.join(root, "src"))
        open(os.path.join(root, "src", "x.h"), "w").write("/* h */\n"); open(os.path.join(root, "Makefile"), "w").write("all:\n")
        ok = K.pack(root, os.path.join(tmp, "hand off.zip"))
        check("path: kiwi pack/verify incl. .h + Makefile", ok)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

if __name__ == "__main__":
    which = sys.argv[1:] or ["strudel", "mod", "tfmx", "levels", "lint", "paths"]
    for w in which:
        print("==", w); globals()["t_" + w]()
    print("SELFTEST %s fails=%d %s" % ("OK" if not FAILS else "FAIL", len(FAILS), FAILS))
    sys.exit(1 if FAILS else 0)
