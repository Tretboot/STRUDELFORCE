#!/usr/bin/env python3
"""build_handoff.py - assemble the complete workspace + run all tests + zip, in ONE process.

Kiwi usage (single tool call!):
  python3 /mnt/data/pkg/tools/build_handoff.py /mnt/data/STRUDELFORCE_Claude_Handoff.zip /mnt/data/pkg /mnt/data/STRUDELFORCE_v3

Local usage: not needed (the zip already is the workspace); use tools/kiwi_handoff.py instead.
"""
import sys, os, shutil, zipfile, subprocess, json

SRC_ZIP, OVERLAY, OUT = [os.path.abspath(x) for x in sys.argv[1:4]]
TEST_DATA = ["TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.tfx",
             "TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.sam",
             "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx",
             "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.sam"]

def run(cmd, cwd):
    print("$", " ".join(cmd)); r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    print(r.stdout[-4000:], r.stderr[-2000:]); return r.returncode

if os.path.exists(OUT): shutil.rmtree(OUT)
os.makedirs(OUT)
z = zipfile.ZipFile(SRC_ZIP)
for n in z.namelist():                         # 1) legacy workspace minus the 18 MB module library
    if n.startswith("TFMX Professional/") or n.endswith("/"): continue
    z.extract(n, OUT)
os.makedirs(os.path.join(OUT, "tests", "data"), exist_ok=True)
for n in TEST_DATA:                            # 2) small regression corpus
    open(os.path.join(OUT, "tests", "data", os.path.basename(n)), "wb").write(z.read(n))

for r, ds, fs in os.walk(OVERLAY):             # 3) overlay new/changed files
    for f in fs:
        if "__pycache__" in r: continue
        src = os.path.join(r, f); rel = os.path.relpath(src, OVERLAY)
        dst = os.path.join(OUT, rel); os.makedirs(os.path.dirname(dst), exist_ok=True); shutil.copy(src, dst)

rc = run([sys.executable, "tools/gen_sources.py", "."], OUT)           # 4) .h / Makefile / patches
rc |= run([sys.executable, "tools/level_tool.py", "header", "src/level_data.h"], OUT)
song = {"format": "sfsong/1", "title": "selftest", "bpm": 125, "steps_per_cycle": 16, "cycles": 4,
        "tracks": [{"name": "bass", "pattern": "<c2 eb2 g2 f2> [g2 c3 eb3 g3]*2 <ab1 bb1 c2 d2>", "instrument": "saw", "gain": 50},
                   {"name": "kick", "pattern": "c3 ~ c3 ~", "instrument": "kick", "gain": 64}]}
json.dump(song, open(os.path.join(OUT, "tests", "data", "selftest_song.sfsong.json"), "w"), indent=1)
rc_tests = run([sys.executable, "tools/selftest.py"], OUT)              # 5) tests
rc_scan = run([sys.executable, "tools/tfmx_oracle.py", "--scan", "tests/data", "4"], OUT)
open(os.path.join(OUT, "tests", "LAST_KIWI_TEST_RUN.txt"), "w").write(
    "selftest rc=%d scan rc=%d gen rc=%d\n(see captains log for full output)\n" % (rc_tests, rc_scan, rc))
ok = run([sys.executable, "tools/kiwi_handoff.py", "pack", OUT, OUT + ".zip"], OUT)  # 6) manifest+zip+verify
print("BUILD_DONE tests=%d pack=%d" % (rc_tests, ok))
