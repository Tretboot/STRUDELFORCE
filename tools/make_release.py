#!/usr/bin/env python3
"""make_release.py - sauberes Release-Paket (nur Relevantes) in release_out/.

Erzeugt:
  strudelforce-v<VER>-source.zip        git archive HEAD (Pfade mit `export-ignore`
                                        in .gitattributes bleiben draussen)
  strudelforce-v<VER>-windows-x64.zip   Binaries + README + Release-Notes
  strudelforce-v<VER>-amiga.zip         strudelforce.adf (+ .amiga), falls gebaut
  SHA256SUMS.txt

NIE enthalten: TFMX Professional/, MOD/, Kickstart/, archive/, ref/, tmp/ (lizenziert
bzw. lokal). Aufruf (Projekt-Root):  python tools/make_release.py [--no-windows]
Die Version wird aus STRUDELFORCE_VERSION in src/strudelforce.h gelesen.
"""
import hashlib
import os
import re
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "release_out")

WIN_FILES = ["strudelforce.exe", "strudelforce_game.exe"]
DOC_FILES = ["README.md", "LICENSE"]


def version():
    h = open(os.path.join(ROOT, "src", "strudelforce.h"), encoding="utf-8", errors="replace").read()
    m = re.search(r'#define\s+STRUDELFORCE_VERSION\s+"([^"]+)"', h)
    if not m:
        sys.exit("STRUDELFORCE_VERSION nicht gefunden")
    return m.group(1)


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def notes_for(ver):
    p = os.path.join(ROOT, "releases", "RELEASE_NOTES_v%s.md" % ver)
    return p if os.path.isfile(p) else None


def make_source(ver):
    dst = os.path.join(OUT, "strudelforce-v%s-source.zip" % ver)
    subprocess.check_call(["git", "archive", "--format=zip", "--prefix=strudelforce-%s/" % ver,
                           "-o", dst, "HEAD"], cwd=ROOT)
    return dst


def make_zip(name, entries):
    dst = os.path.join(OUT, name)
    n = 0
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
        for src, arc in entries:
            if os.path.isfile(src):
                z.write(src, arc)
                n += 1
            else:
                print("  [uebersprungen, fehlt] " + src)
    return dst if n else None


def main():
    ver = version()
    os.makedirs(OUT, exist_ok=True)
    made = [make_source(ver)]
    if "--no-windows" not in sys.argv:
        ent = [(os.path.join(ROOT, f), f) for f in WIN_FILES + DOC_FILES]
        n = notes_for(ver)
        if n:
            ent.append((n, "RELEASE_NOTES.md"))
        made.append(make_zip("strudelforce-v%s-windows-x64.zip" % ver, ent))
    adf = os.path.join(ROOT, "strudelforce.adf")
    if os.path.isfile(adf):
        made.append(make_zip("strudelforce-v%s-amiga.zip" % ver,
                             [(adf, "strudelforce.adf"),
                              (os.path.join(ROOT, "amiga", "out", "a.exe"), "strudelforce.amiga"),
                              (os.path.join(ROOT, "README.md"), "README.md")]))
    made = [m for m in made if m]
    with open(os.path.join(OUT, "SHA256SUMS.txt"), "w", encoding="utf-8", newline="\n") as f:
        for m in made:
            f.write("%s  %s\n" % (sha256(m), os.path.basename(m)))
    for m in made:
        print("%9.1f KB  %s" % (os.path.getsize(m) / 1024.0, os.path.basename(m)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

