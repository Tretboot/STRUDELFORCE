#!/usr/bin/env python3
"""build_amiga_adf.py — STRUDELFORCE Amiga Floppy Disk (ADF) Pack & Validator.

Erstellt ein bootfaehiges, vollstaendiges 880 KB Amiga Floppy Image (`strudelforce.adf`)
mittels `amitools.tools.xdftool`.
Enthaelt:
  - Bootblock (Standard Exec/DOS Bootblock via `boot install`)
  - s/startup-sequence (startet `strudelforce.amiga`)
  - music/ (Turrican 3 World 1, Bifi Adventure, Cybersty, Mad TV)
  - strudelforce.amiga (68k Binary; falls kein gcc vorhanden, valider Hunk-Stub)
"""
import os
import sys
import struct
import subprocess
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "amiga_dist")
ADF_PATH = os.path.join(ROOT, "strudelforce.adf")
EXE_ROOT = os.path.join(ROOT, "strudelforce.amiga")
EXE_DIST = os.path.join(DIST, "strudelforce.amiga")

MINIMAL_HUNK = (
    struct.pack(
        '>IIIIIIIII',
        0x000003F3,  # HUNK_HEADER
        0,           # string table ends (null)
        1,           # table size = 1 hunk
        0,           # first hunk = 0
        0,           # last hunk = 0
        1,           # hunk 0 size = 1 longword (4 bytes)
        0x000003E9,  # HUNK_CODE
        1,           # code length = 1 longword
        0x70004E75,  # moveq #0, d0; rts
    )
    + struct.pack('>I', 0x000003F2)  # HUNK_END
)

STARTUP = (
    "; STRUDELFORCE startup-sequence (Boot-Disk, nur Kickstart noetig)\n"
    "; Bewusst OHNE echo/run: auf der Disk liegt kein C:-Verzeichnis.\n"
    "strudelforce.amiga\n"
)
AMIGA_EXE = os.path.join(ROOT, "amiga", "out", "a.exe")   # make -C amiga

def run_stage_music() -> bool:
    """Musik ist optional (lizenzierte TFMX-Dateien liegen nur lokal; CI hat sie nicht)."""
    if not os.path.isdir(os.path.join(ROOT, "TFMX Professional")):
        print("[ADF] Keine TFMX-Bibliothek -> Disk ohne Musik")
        shutil.rmtree(os.path.join(DIST, "music"), ignore_errors=True)
        return True
    stage_script = os.path.join(ROOT, "tools", "audio", "stage_amiga_music.py")
    if not os.path.isfile(stage_script):
        stage_script = os.path.join(ROOT, "tools", "stage_amiga_music.py")
    return subprocess.run([sys.executable, stage_script], cwd=ROOT).returncode == 0

def ensure_amiga_exe() -> bool:
    """Das echte Spiel muss gebaut sein - KEIN Hunk-Stub mehr (der fuehrte zu 'nur Workbench')."""
    if not os.path.isfile(AMIGA_EXE):
        print("[FEHLER] %s fehlt - zuerst `make -C amiga` (m68k-amiga-elf-gcc)." % AMIGA_EXE)
        return False
    os.makedirs(DIST, exist_ok=True)
    shutil.copy2(AMIGA_EXE, EXE_DIST)
    shutil.copy2(AMIGA_EXE, EXE_ROOT)
    print("[ADF] strudelforce.amiga = %d Bytes" % os.path.getsize(AMIGA_EXE))
    return True
def build_adf() -> int:
    print("=" * 60)
    print("STRUDELFORCE v0.3.1 - Amiga ADF Disk Pack")
    print("=" * 60)

    # 1. Staging
    if not run_stage_music():
        print("[FEHLER] stage_amiga_music fehlgeschlagen!")
        return 1

    # 2. Executable + startup-sequence
    if not ensure_amiga_exe():
        return 1
    os.makedirs(os.path.join(DIST, "s"), exist_ok=True)
    with open(os.path.join(DIST, "s", "startup-sequence"), "w", newline="\n") as f:
        f.write(STARTUP)

    # 3. Format and pack via xdftool (OFS + boot1x fuer maximale KS 1.3 / A500-Kompatibilitaet)
    cmd = [
        sys.executable, "-m", "amitools.tools.xdftool",
        "-f", ADF_PATH,
        "format", "StrudelOS", "ofs",
        "+", "boot", "install", "boot1x",
        "+", "pack", DIST,
        "+", "list"
    ]
    print(f"[ADF] Erzeuge {ADF_PATH} (Volume: StrudelOS)...")
    res = subprocess.run(cmd, cwd=ROOT)
    if res.returncode != 0:
        print("[FEHLER] xdftool pack fehlgeschlagen!")
        return res.returncode

    # 4. Versioniertes ADF in amiga/builds/ ablegen
    builds_dir = os.path.join(ROOT, "amiga", "builds")
    os.makedirs(builds_dir, exist_ok=True)
    versioned_adf = os.path.join(builds_dir, "strudelforce_v0.3.1.adf")
    shutil.copy2(ADF_PATH, versioned_adf)
    print(f"[ADF] Versioniertes Image gesichert: {versioned_adf}")

    # 4. Check size
    size = os.path.getsize(ADF_PATH)
    print(f"\n[OK] strudelforce.adf erfolgreich erstellt! ({size} Bytes)")
    if size != 901120:
        print(f"[WARNUNG] Dateigroesse ist {size}, erwartet wurden exakt 901120 Bytes (880 KB)!")
        return 2

    print("[SUCCESS] Amiga-Floppy-Limit (880 KB) eingehalten.")
    return 0

if __name__ == "__main__":
    sys.exit(build_adf())

