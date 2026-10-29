#!/usr/bin/env python3
"""STRUDELFORCE .h-Datei Waechter (Sandbox-Wipe-Workaround).

Konvention: .h-Dateien werden in der Sandbox gewiped -> als .h.c gespeichert.
  guard    <root>   *.h -> *.h.c (skip, wenn .h.c schon existiert)
  restore  <root>   *.h.c -> *.h (vor Handoff/Export)
  check    <root>   meldet fremde .h (ungenardet)
  bootstrap <zip> <root>   ZIP entpacken + guard
Regeln:
  - guard ueberschreibt niemals ein vorhandenes .h.c (meldet CONFLICT).
  - restore ueberschreibt niemals ein vorhandenes .h (meldet CONFLICT).
"""
import os, sys, zipfile

def guard(root):
    n = c = 0
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            if f.endswith(".h"):
                p = os.path.join(dp, f)
                t = p + ".c"
                if os.path.exists(t):
                    if os.path.getsize(p) == os.path.getsize(t):
                        os.remove(p); n += 1
                    else:
                        print("CONFLICT (groessenunterschied):", p); c += 1
                else:
                    os.rename(p, t); n += 1
    print("guard: %d gesichert, %d Konflikte" % (n, c))

def restore(root):
    n = c = 0
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            if f.endswith(".h.c"):
                p = os.path.join(dp, f)
                t = p[:-2]
                if os.path.exists(t):
                    print("CONFLICT (vorhanden, nicht ueberschrieben):", t); c += 1
                else:
                    os.rename(p, t); n += 1
    print("restore: %d zurueckbenannt, %d Konflikte" % (n, c))

def check(root):
    bad = []
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            if f.endswith(".h"):
                bad.append(os.path.join(dp, f))
    if bad:
        print("Fremde .h (ungenardet): %d" % len(bad))
        for b in bad: print("  ", b)
    else:
        print("check: ok, keine fremden .h")

def bootstrap(zpath, root):
    os.makedirs(root, exist_ok=True)
    with zipfile.ZipFile(zpath) as z:
        z.extractall(root)
    print("entpackt: %s" % zpath)
    guard(root)

def main():
    if len(sys.argv) < 3:
        print(__doc__); return
    cmd = sys.argv[1]
    if cmd == "guard": guard(sys.argv[2])
    elif cmd == "restore": restore(sys.argv[2])
    elif cmd == "check": check(sys.argv[2])
    elif cmd == "bootstrap" and len(sys.argv) == 4: bootstrap(sys.argv[2], sys.argv[3])
    else: print(__doc__)

if __name__ == "__main__":
    main()
