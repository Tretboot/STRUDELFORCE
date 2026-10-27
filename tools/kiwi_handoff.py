#!/usr/bin/env python3
"""kiwi_handoff.py - loss-proof handoffs between Kiwi Connect sandboxes and local VS Code

  kiwi_handoff.py probe  [dir]        # write test files with many extensions; run 'probe-check' in the NEXT call
  kiwi_handoff.py probe-check [dir]   # report which extensions survived the call boundary
  kiwi_handoff.py manifest <root>     # write MANIFEST.json (path, size, sha256) for every file
  kiwi_handoff.py verify   <root|zip> # compare against MANIFEST.json -> missing / changed / extra
  kiwi_handoff.py pack     <root> [out.zip]   # manifest + verify + zip in ONE step (one tool call!)
  kiwi_handoff.py unpack   <zip> <dest>       # extract + verify (local or next Kiwi session)

Rules encoded here (see docs/KIWI_PERSISTENCE_GUIDE.md):
  * never rely on files with non-persisted extensions surviving a tool call
  * produce + verify + zip inside the SAME call; the .zip is the only artefact that matters
  * every handoff carries MANIFEST.json so the receiver can prove completeness
"""
import sys, os, json, hashlib, zipfile, time

PROBE_EXT = [".c", ".h", ".py", ".md", ".txt", ".json", ".zip", ".png", ".wav", ".mod", ".tfx", ".sam",
             ".sid", ".strudel", ".adf", ".raw", ".yml", ".toml", ".ini", ".sh", ".bat", ".js", ".csv", ""]
SKIP_DIRS = {".git", "__pycache__", "build", ".kiwi_probe",
             "archive",                 # lokale Backups/Branch-Archive (post-Merge)
             "TFMX Professional"}       # Asset-Bibliothek: lokal Referenz, nie ins Handoff-ZIP
SKIP_EXT = {".wav", ".raw", ".adf", ".amiga", ".exe", ".obj", ".o"}

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 16), b""): h.update(b)
    return h.hexdigest()

def walk(root, exclude=None):
    # exclude: Liste von Pfaden (abs oder relativ), die NICHT gelistet werden duerfen.
    # WICHTIG: pack() muss seine eigene Ausgabedatei hier ausscheiden, sonst landet
    # das ZIP im Walk und es entsteht ein endloses Self-Referenz-ZIP (7GB+ beobachtet).
    excl = set()
    for e in (exclude or []):
        try: excl.add(os.path.abspath(e))
        except Exception: pass
    for r, ds, fs in os.walk(root):
        ds[:] = sorted(d for d in ds if d not in SKIP_DIRS)
        for f in sorted(fs):
            p = os.path.join(r, f); rel = os.path.relpath(p, root).replace("\\", "/")
            if rel == "MANIFEST.json" or os.path.splitext(f)[1].lower() in SKIP_EXT: continue
            if os.path.abspath(p) in excl: continue
            yield rel, p

def manifest(root, exclude=None):
    files = {rel: {"size": os.path.getsize(p), "sha256": sha(p)} for rel, p in walk(root, exclude)}
    m = {"format": "kiwi-manifest/1", "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
         "count": len(files), "files": files}
    json.dump(m, open(os.path.join(root, "MANIFEST.json"), "w"), indent=1, sort_keys=True)
    return m

def verify(target):
    if zipfile.is_zipfile(target):
        z = zipfile.ZipFile(target); names = z.namelist()
        pre = os.path.commonprefix(names) if names else ""
        pre = pre[:pre.rfind("/") + 1] if "/" in pre else ""
        m = json.loads(z.read(pre + "MANIFEST.json"))
        have = {n[len(pre):]: hashlib.sha256(z.read(n)).hexdigest() for n in names if not n.endswith("/")}
    else:
        m = json.load(open(os.path.join(target, "MANIFEST.json")))
        have = {rel: sha(p) for rel, p in walk(target)}
    missing = [f for f in m["files"] if f not in have]
    changed = [f for f in m["files"] if f in have and have[f] != m["files"][f]["sha256"]]
    extra = [f for f in have if f not in m["files"] and f != "MANIFEST.json"]
    return missing, changed, extra, m["count"]

def pack(root, out=None):
    root = os.path.abspath(root)
    out = out or os.path.join(os.path.dirname(root), os.path.basename(root) + ".zip")
    out_abs = os.path.abspath(out)
    m = manifest(root, exclude=[out_abs])
    base = os.path.basename(root)
    with zipfile.ZipFile(out_abs, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(os.path.join(root, "MANIFEST.json"), base + "/MANIFEST.json")
        for rel, p in walk(root, exclude=[out_abs]): z.write(p, base + "/" + rel)
    missing, changed, extra, n = verify(out_abs)
    ok = not missing and not changed
    print("PACK %s files=%d missing=%d changed=%d extra=%d -> %s" % (out, n, len(missing), len(changed), len(extra), "OK" if ok else "FAIL"))
    return ok

if __name__ == "__main__":
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(0)
    cmd = a[0]
    if cmd == "probe":
        d = os.path.join(a[1] if len(a) > 1 else ".", ".kiwi_probe"); os.makedirs(d, exist_ok=True)
        for e in PROBE_EXT: open(os.path.join(d, "probe" + e if e else "Makefile"), "w").write("probe\n")
        print("probe files written to", d, "- run probe-check in the NEXT tool call")
    elif cmd == "probe-check":
        d = os.path.join(a[1] if len(a) > 1 else ".", ".kiwi_probe")
        res = {(e or "Makefile"): os.path.exists(os.path.join(d, "probe" + e if e else "Makefile")) for e in PROBE_EXT}
        print("KEPT:   ", " ".join(k for k, v in res.items() if v))
        print("DROPPED:", " ".join(k for k, v in res.items() if not v))
        json.dump(res, open(os.path.join(a[1] if len(a) > 1 else ".", "kiwi_probe_result.json"), "w"), indent=1)
    elif cmd == "manifest": print("files", manifest(a[1])["count"])
    elif cmd == "verify":
        mi, ch, ex, n = verify(a[1])
        for f in mi: print("  MISSING", f)
        for f in ch: print("  CHANGED", f)
        print("VERIFY files=%d missing=%d changed=%d extra=%d" % (n, len(mi), len(ch), len(ex))); sys.exit(1 if mi or ch else 0)
    elif cmd == "pack": sys.exit(0 if pack(a[1], a[2] if len(a) > 2 else None) else 1)
    elif cmd == "unpack":
        zipfile.ZipFile(a[1]).extractall(a[2]); mi, ch, ex, n = verify(a[1])
        print("UNPACK %s -> %s missing=%d changed=%d" % (a[1], a[2], len(mi), len(ch)))
