#!/usr/bin/env python3
"""
curate_audio.py - Replaces bulky audio collections with curated, ultra-lean library (v0.3.1).
Keeps 16 classic/chip MODs and 4 action TFMX tracks requested by the user.
Total payload size: ~447 KB (fits comfortably on an 880 KB Amiga Floppy Disk).
"""
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEGACY_DIR = os.path.join(ROOT, "archive", "audio_legacy")
MOD_DIR = os.path.join(ROOT, "MOD")
TFMX_DIR = os.path.join(ROOT, "TFMX Professional")

CURATED_MODS = [
    ("archive/audio_legacy/MOD/Pop Songs/- TV and Movie/Airwolf 2.Mod", "MOD/Airwolf 2.mod"),
    ("archive/audio_legacy/MOD/Demos/MOD.ocean loader.Mod", "MOD/Ocean Loader.mod"),
    ("archive/audio_legacy/MOD/Demos/Kefrens/mod.Karate Of Zycho.Mod", "MOD/MD7 Karate of Zycho.mod"),
    ("archive/audio_legacy/MOD/Pop Songs/- House, Acid, Trance/The Prodigy/The Prodigy - Everybody in the Place (everybody in space mix).mod", "MOD/Everybody in Space.mod"),
    ("archive/audio_legacy/MOD/Cracktro/Mod.Intro Number 45.Mod", "MOD/Intro Number 45.mod"),
    ("archive/audio_legacy/MOD/Demos/Mod.P60a.Mod", "MOD/P60a.mod"),
    ("archive/audio_legacy/MOD/Demos/Mod.3C Minichip.Mod", "MOD/3C Minichip.mod"),
    ("archive/audio_legacy/MOD/Cracktro/Mod.Intro Number 75.Mod", "MOD/Intro Number 75.mod"),
    ("archive/audio_legacy/MOD/Demos/Mod.Micro Tune.Mod", "MOD/Micro Tune.mod"),
    ("archive/audio_legacy/MOD/Demos/Mod.Orgasmic Chipper 02.Mod", "MOD/Orgasmic Chipper 02.mod"),
    ("archive/audio_legacy/MOD/Demos/Mod.Techn. Perfection.Mod", "MOD/Techn Perfection.mod"),
    ("archive/audio_legacy/MOD/Pop Songs/- TV and Movie/Knight rider Theme (midi).mod", "MOD/Knight Rider.mod"),
    ("archive/audio_legacy/MOD/Demos/Mod.Shipmania 07.Mod", "MOD/Shipmania 07.mod"),
    ("archive/audio_legacy/MOD/Pop Songs/- TV and Movie/miami_vice c64.mod", "MOD/Miami Vice.mod"),
    ("archive/audio_legacy/MOD/Cracktro/Mod.Intro Number 31.Mod", "MOD/Intro Number 31.mod"),
    ("archive/audio_legacy/MOD/Demos/Mod.Very Small.Mod", "MOD/Very Small.mod"),
]

CURATED_TFMX = [
    ("archive/audio_legacy/TFMX Professional/BiFi Adventure/Bifi Adventure.tfx", "TFMX Professional/BiFi Adventure/Bifi Adventure.tfx"),
    ("archive/audio_legacy/TFMX Professional/BiFi Adventure/Bifi Adventure.sam", "TFMX Professional/BiFi Adventure/Bifi Adventure.sam"),
    ("archive/audio_legacy/TFMX Professional/Cybersty.tfx", "TFMX Professional/Cybersty/Cybersty.tfx"),
    ("archive/audio_legacy/TFMX Professional/Cybersty.sam", "TFMX Professional/Cybersty/Cybersty.sam"),
    ("archive/audio_legacy/TFMX Professional/Mad TV/Mad TV - Ongame 6.tfx", "TFMX Professional/Mad TV/Mad TV - Ongame 6.tfx"),
    ("archive/audio_legacy/TFMX Professional/Mad TV/Mad TV - Ongame 6.sam", "TFMX Professional/Mad TV/Mad TV - Ongame 6.sam"),
    ("archive/audio_legacy/TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.tfx", "TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.tfx"),
    ("archive/audio_legacy/TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.sam", "TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.sam"),
]

def main():
    print("=== STRUDELFORCE Audio Curation v0.3.1 ===")
    os.makedirs(LEGACY_DIR, exist_ok=True)

    # 1. Staging curated files into temporary store
    staged_mods = []
    for src_rel, dst_rel in CURATED_MODS:
        src = os.path.join(ROOT, src_rel)
        if not os.path.exists(src):
            print(f"[FEHLER] MOD Quelle nicht gefunden: {src}")
            return 1
        staged_mods.append((src, dst_rel))

    staged_tfmx = []
    for src_rel, dst_rel in CURATED_TFMX:
        src = os.path.join(ROOT, src_rel)
        if not os.path.exists(src):
            print(f"[FEHLER] TFMX Quelle nicht gefunden: {src}")
            return 1
        staged_tfmx.append((src, dst_rel))

    print(f"Staging {len(staged_mods)} MODs and {len(staged_tfmx)} TFMX files...")

    # Backup legacy collections if not already backed up
    legacy_mod = os.path.join(LEGACY_DIR, "MOD")
    legacy_tfmx = os.path.join(LEGACY_DIR, "TFMX Professional")
    if not os.path.exists(legacy_mod) and os.path.exists(MOD_DIR):
        print(f"Archiviere altes MOD-Verzeichnis nach {legacy_mod}...")
        shutil.move(MOD_DIR, legacy_mod)
    else:
        if os.path.exists(MOD_DIR):
            shutil.rmtree(MOD_DIR)

    if not os.path.exists(legacy_tfmx) and os.path.exists(TFMX_DIR):
        print(f"Archiviere altes TFMX-Verzeichnis nach {legacy_tfmx}...")
        shutil.move(TFMX_DIR, legacy_tfmx)
    else:
        if os.path.exists(TFMX_DIR):
            shutil.rmtree(TFMX_DIR)

    os.makedirs(MOD_DIR, exist_ok=True)
    os.makedirs(TFMX_DIR, exist_ok=True)

    total_bytes = 0
    # Copy curated MODs
    for src, dst_rel in staged_mods:
        dst = os.path.join(ROOT, dst_rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        sz = os.path.getsize(dst)
        total_bytes += sz
        print(f"  [MOD]  {os.path.basename(dst):30s} ({sz:6d} B)")

    # Copy curated TFMX
    for src, dst_rel in staged_tfmx:
        dst = os.path.join(ROOT, dst_rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        sz = os.path.getsize(dst)
        total_bytes += sz
        print(f"  [TFMX] {os.path.basename(dst):30s} ({sz:6d} B)")

    print("=" * 60)
    print(f"FERTIG: {len(staged_mods)} MODs + {len(staged_tfmx)//2} TFMX Tracks bereitgestellt.")
    print(f"Gesamtgroesse: {total_bytes} Bytes ({total_bytes/1024:.1f} KB)")
    print(f"Amiga Floppy Budget (880 KB): {total_bytes * 100 // 901120}% belegt.")
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())
