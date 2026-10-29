#!/usr/bin/env python3
"""
build_msvc.py - MSVC-Build mit Live-Output & Ergebnis-Report (Windows)

Startet build_msvc.bat (self-contained: ruft selbst vcvars64.bat auf),
streamt den Output live und reportet am Ende Exit-Code + EXE-Groessen.

Aufruf:   python tools/build_msvc.py
Option:   --timeout <sec>   Abbruch nach N Sekunden (Default: 600)

Nuetzlich fuer Agenten/Terminals ohne GUI:
    python tools/build_msvc.py > build_out.txt 2>&1
"""
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BAT = os.path.join(ROOT, "build_msvc.bat")

EXES = ["strudelforce.exe", "strudelforce_game.exe", "test_suite.exe"]


def main():
    timeout = 600
    if "--timeout" in sys.argv:
        i = sys.argv.index("--timeout")
        if i + 1 < len(sys.argv):
            timeout = int(sys.argv[i + 1])

    if not os.path.isfile(BAT):
        print("FEHLER: build_msvc.bat nicht gefunden:", BAT)
        return 1

    t0 = time.time()
    try:
        proc = subprocess.Popen(
            ["cmd", "/c", BAT],
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
    except OSError as e:
        print("FEHLER: Build nicht startbar:", e)
        return 1

    print("=== build_msvc.py: starte Build (Timeout %d s) ===" % timeout, flush=True)
    deadline = time.time() + timeout
    for line in proc.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
        if time.time() > deadline:
            proc.kill()
            print("\nFEHLER: Build Timeout nach %d s" % timeout, flush=True)
            return 2
    rc = proc.wait()
    dt = time.time() - t0

    print()
    print("=== BUILD-REPORT ===")
    print("Exit-Code : %d (%s)" % (rc, "OK" if rc == 0 else "FEHLER"))
    print("Dauer     : %.1f s" % dt)
    for name in EXES:
        p = os.path.join(ROOT, name)
        if os.path.isfile(p):
            st = os.stat(p)
            print("%-24s %10d Bytes  (neue Build: %s)" %
                  (name, st.st_size,
                   "JA" if (time.time() - st.st_mtime) < dt + 5 else "NEIN!"))
        else:
            print("%-24s FEHLT!" % name)
    if rc == 0:
        for name in EXES:
            p = os.path.join(ROOT, name)
            if os.path.isfile(p) and os.stat(p).st_mtime < t0 - 1:
                print("WARNUNG: %s ist ALTER Build (nicht neu erzeugt)!" % name)
                return 3
    return rc


if __name__ == "__main__":
    sys.exit(main())
