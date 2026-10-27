#!/usr/bin/env python3
"""c_vs_oracle.py - LOCAL ONLY: bit-compare the C players against the python oracles.

Needs ./test_audio_mix (make test_audio_mix). For each case it renders N seconds with C (--raw)
and with the oracle, then reports first differing sample + max abs diff.
Goal: 0 differences. Any diff => C and oracle diverged -> fix whichever is wrong, keep them in sync.
"""
import os, sys, struct, subprocess, glob
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import tfmx_oracle as T, mod_oracle as M, strudel_bridge as S

EXE = os.path.join(ROOT, "test_audio_mix" + (".exe" if os.name == "nt" else ""))
SECS = 5

def c_render(args):
    out = os.path.join(ROOT, "_c_out.raw")
    r = subprocess.run([EXE] + args + ["--secs", str(SECS), "--raw", out], capture_output=True, text=True)
    if r.returncode: print(r.stdout, r.stderr); return None
    d = open(out, "rb").read(); os.remove(out)
    return list(struct.unpack("<%dh" % (len(d) // 2), d))

def compare(name, a, b):
    if a is None: print("  [FAIL] %s: C render failed" % name); return False
    n = min(len(a), len(b)); diff = [i for i in range(n) if a[i] != b[i]]
    mx = max((abs(a[i] - b[i]) for i in diff), default=0)
    ok = not diff and len(a) == len(b)
    print("  [%s] %s samples=%d diffs=%d first=%s maxdiff=%d" % ("OK" if ok else "FAIL", name, n, len(diff), diff[0] if diff else "-", mx))
    return ok

def main():
    if not os.path.exists(EXE): print("build first: make test_audio_mix"); return 2
    ok = True
    cases = sorted(glob.glob(os.path.join(ROOT, "TFMX Professional", "Turrican 2", "*.tfx")))[:3] + \
            sorted(glob.glob(os.path.join(ROOT, "TFMX Professional", "Apidya", "*.tfx")))[:2] + \
            sorted(glob.glob(os.path.join(ROOT, "tests", "data", "*.tfx")))
    for tfx in cases:
        sam = tfx[:-4] + ".sam"
        t = T.Tfmx(open(tfx, "rb").read(), open(sam, "rb").read() if os.path.exists(sam) else None)
        t.start(0, 16574); ref = t.render(16574 * SECS)
        ok &= compare(os.path.basename(tfx), c_render(["--tfmx", tfx]), ref)
    modp = os.path.join(ROOT, "tests", "data", "selftest_song.mod")
    if not os.path.exists(modp):
        import json
        open(modp, "wb").write(S.song_to_mod(json.load(open(os.path.join(ROOT, "tests", "data", "selftest_song.sfsong.json")))))
    m = M.Mod(open(modp, "rb").read()); m.start(16574)
    ok &= compare("selftest_song.mod", c_render(["--mod", modp]), m.render(16574 * SECS))
    print("C_VS_ORACLE", "OK" if ok else "FAIL"); return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
