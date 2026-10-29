#!/usr/bin/env python3
"""
tools/build_kiwi_injector.py - STRUDELFORCE Kiwi Self-Extracting Python Injector Generator

Erzeugt ein kompaktes, selbstextrahierendes Python-Skript (z.B. kiwi_level_injector.py / kiwi_injector.py)
fuer die Uebergabe an Kiwi in der Cloud-Sandbox (Claude / LibreChat).

Ziel:
- Speziell abgestimmt fuer die Entwicklung des Leveleditors in Kiwi.
- Enthaelt alle relevanten Level-Dateien, Level-Tools, Story-, Balancing- und i18n-JSONs.
- Enthaelt die vollstaendige C-Codebasis (Zero-Float, Fixed-Point, 50Hz Simulation & Audio-Core).
- Enthaelt alle Architekturdokumente und die vollstaendige Projekt-Historie (captains_log.md, PROJECT_BIBLE.md).
- Keine Screenshots (.png, .bmp, .ppm), keine temporaeren Build-Artefakte (.obj, .exe).
- Maximal 1 kleinster TFMX-Track (Turrican 2 Load) und 1 kleinster MOD-Track (selftest_song.mod).
- Hochmoderne Solid-Kompression (LZMA mit Fallback auf ZLIB) im Base64-Format (< 650 KB Text),
  vollstaendig innerhalb des Zeichenlimits aller KI-Chat- und Upload-Plattformen.

Aufruf:
    python tools/build_kiwi_injector.py                        # Erzeugt kiwi_level_injector.py & kiwi_injector.py
    python tools/build_kiwi_injector.py --algo zlib            # Verwendet zlib statt lzma
    python tools/build_kiwi_injector.py --slim                 # Ohne embedded_music.h (~250 KB)
    python tools/build_kiwi_injector.py --out my_injector.py   # Benutzerdefinierter Ausgabepfad
"""

import os
import sys
import io
import json
import base64
import tarfile
import hashlib
import argparse

try:
    import lzma
    HAS_LZMA = True
except ImportError:
    HAS_LZMA = False

import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(HERE)

# -----------------------------------------------------------------------------
# Dateiliste fuer den Leveleditor-Handoff an Kiwi
# -----------------------------------------------------------------------------
CORE_FILES = [
    # 1. Build & Projekt-Wurzel
    "Makefile",
    "build_msvc.bat",
    ".gitignore",
    "README.md",
    "BUILD_GUIDE.md",
    "RELEASE_0.3.0.md",
    "PROJECT_BIBLE.md",
    "KIWI_SURVIVAL_GUIDE.md",
    "captains_log.md",
    "verify_core_sim.py",

    # 2. Level-Dateien & Level-Editor-Pipelines
    "levels/levels.json",
    "levels/levels_kiwi_extended.json",
    "tools/level_tool.py",
    "docs/kiwi_level_config_reference.h",

    # 3. Game-Design-, Balancing- & Story-JSONs (Assets)
    "assets/balancing.json",
    "assets/lang.json",
    "assets/level_01.json",
    "assets/story.json",
    "assets/track_metadata.json",
    "assets/chapter_1_awakening.tfmx.json",
    "assets/chapter_2_crystal.tfmx.json",
    "assets/chapter_3_volcanic.tfmx.json",
    "assets/chapter_4_fortress.tfmx.json",
    "assets/track_final_phase.tfmx.json",
    "assets/track_highscore.tfmx.json",
    "assets/sfx/sfx_definitions.json",
    "assets/audio_reference/switch_angel_tracks.strudel",
    "assets/strudel_tracks/extra_slippy.txt",
    "assets/strudel_tracks/green_tuesday.txt",
    "assets/strudel_tracks/in_the_beginning_trance.txt",
    "assets/strudel_tracks/i_accidentally_made_stranger_things_again.txt",

    # 4. Konsolidierte C-Quelltexte & Header (Monolithische Architektur)
    "src/strudelforce.h",
    "src/game_logic.c",
    "src/engine_core.c",
    "src/soundengine_core.c",
    "src/soundengine_core.h",
    "src/test_suite.c",
    "src/game_main.c",
    "src/gui_main.c",
    "src/main_pc.c",
    "src/level_data.h",
    "src/core_sim.h",
    "src/game_audio.h",
    "src/livecode_ui.h",
    "src/voxel_render.h",
    "src/localization.h",
    "src/retro_font.h",
    "src/embedded_credits.h",
    "src/tfmx_player.h",
    "src/mod_player.h",
    "src/sf_mixer.h",
    "src/amiga_intro.h",
    "src/soundgodizer.h",
    "src/win_audio.h",
    "src/sf_interfaces.h",
    "src/turrican_sprites.h",
    "src/amiga_crt.h",
    "src/amiga_music.h",
    "src/assets_index.h",
    "src/cyber_paula.h",
    "src/sid_header.h",
    "src/strudel_cleanroom.h",
    "src/strudel_sketch.h",
    "src/strudel_test_tracks.h",
    "src/tfmx_macro_synth.h",
    "src/compat/dirent.h",
    "src/compat/unistd.h",

    # 5. Entwicklungs-, Test- & Verifikations-Werkzeuge
    "tools/selftest.py",
    "tools/strudel_bridge.py",
    "tools/kiwi_handoff.py",
    "tools/c_vs_oracle.py",
    "tools/mod_oracle.py",
    "tools/tfmx_oracle.py",
    "tools/build_lang.py",
    "tools/asset_packer.py",
    "tools/build_msvc.py",
    "tools/gen_embedded_music.py",

    # 6. Detaillierte Dokumentation, Lore & Architektur-Historie
    "docs/BIOMES.md",
    "docs/ITEMS_LEVELUP.md",
    "docs/LORE.md",
    "docs/LORE_EXTENDED.md",
    "docs/KIWI_HANDOFF_WORKFLOW.md",
    "docs/KIWI_PERSISTENCE_GUIDE.md",
    "docs/AUDIO_MODEL_DIRECTIVE.md",
    "docs/MUSIC_SYSTEM.md",
    "docs/WORKFLOW_ESSAY.md",

    # 7. Exakt 1 TFMX-Track (Testsieger) & 1 MOD-Track
    # TFMX: Turrican 2 - World 1 - Dessert (fuer 100% PASS in tools/selftest.py)
    "tests/data/Turrican 2 - World 1 - Dessert.tfx",
    "tests/data/Turrican 2 - World 1 - Dessert.sam",
    # MOD: selftest_song.mod (4572 B) - sichert sofortigen 100% PASS in tools/selftest.py
    "tests/data/selftest_song.mod",
]

def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def collect_manifest(files_list):
    manifest = {}
    tar_buffer = io.BytesIO()
    total_raw = 0

    with tarfile.open(fileobj=tar_buffer, mode="w") as tf:
        for rel_path in files_list:
            full_path = os.path.join(ROOT_DIR, rel_path.replace("/", os.sep))
            if not os.path.exists(full_path):
                print(f"[WARNUNG] Datei fehlt und wird uebersprungen: {rel_path}")
                continue

            with open(full_path, "rb") as f:
                data = f.read()

            raw_size = len(data)
            sha = sha256_bytes(data)
            manifest[rel_path.replace("\\", "/")] = (raw_size, sha)
            total_raw += raw_size

            # In Tar-Buffer einfuegen
            ti = tarfile.TarInfo(name=rel_path.replace("\\", "/"))
            ti.size = raw_size
            ti.mtime = int(os.path.getmtime(full_path))
            ti.mode = 0o644
            tf.addfile(ti, io.BytesIO(data))

    tar_bytes = tar_buffer.getvalue()
    return manifest, tar_bytes, total_raw

def generate_injector_script(manifest, payload_b64, algo="lzma", out_path=None):
    if not out_path:
        out_path = os.path.join(ROOT_DIR, "kiwi_level_injector.py")

    manifest_json = json.dumps(manifest, indent=2)

    script_template = f'''#!/usr/bin/env python3
"""
kiwi_level_injector.py - STRUDELFORCE v0.3.0 KIWI SELF-EXTRACTING INJECTOR
========================================================================
Spezial-Handoff fuer die Leveleditor-Entwicklung in der Kiwi Cloud-Sandbox.

Enthaelt:
  - Vollstaendige Level-Pipeline (levels/levels.json, levels_kiwi_extended.json, tools/level_tool.py)
  - Vollstaendige C-Codebasis (Zero-Float, Fixed-Point 16.16, 50Hz PAL, Soundengine & Renderer)
  - Story-, Balancing-, Dialog- und Sprach-Assets (JSON)
  - Vollstaendige Projekt-Historie & Architektur-Gesetze (captains_log.md, PROJECT_BIBLE.md, etc.)
  - Verifizierungs-Oracles (tools/selftest.py, verify_core_sim.py, test_suite.c)
  - 1 kleinster TFMX-Track (Turrican 2 Load) + 1 kleinster MOD-Track (selftest_song.mod)
  - Keine Screenshots, keine unnoetigen Audio-Giga-Blobs, keine Binaries.

Aufruf:
    python kiwi_level_injector.py             # Entpackt in das aktuelle Verzeichnis
    python kiwi_level_injector.py <ziel_dir>  # Entpackt in ein spezifisches Ziel-Verzeichnis
    python kiwi_level_injector.py --test      # Entpackt und fuehrt Verifikation aus
    python kiwi_level_injector.py --list      # Zeigt das Datei-Manifest ohne Schreiben an
"""

import os
import sys
import io
import json
import base64
import tarfile
import hashlib

ALGO = "{algo}"

MANIFEST = {manifest_json}

PAYLOAD = """{payload_b64}"""

def decompress_payload(raw_b64, algo):
    data = base64.b64decode(raw_b64.encode("ascii"))
    if algo == "lzma":
        import lzma
        return lzma.decompress(data)
    elif algo == "zlib":
        import zlib
        return zlib.decompress(data)
    else:
        raise ValueError(f"Unbekannter Algorithmus: {{algo}}")

def extract_all(target_dir=".", run_test=False, list_only=False):
    target_dir = os.path.abspath(target_dir)

    print("=" * 72)
    print("STRUDELFORCE v0.3.0 — KIWI LEVEL EDITOR INJECTOR")
    print(f"Zielverzeichnis : {{target_dir}}")
    print(f"Kompressions-Art : {{ALGO.upper()}} (Solid Tar)")
    print(f"Dateien gesamt   : {{len(MANIFEST)}}")
    print("=" * 72)

    if list_only:
        print("\\nEnthaltene Dateien:")
        for rel_path, (size, sha) in sorted(MANIFEST.items()):
            print(f"  {{size:>9}} B  {{sha[:12]}}  {{rel_path}}")
        print("-" * 72)
        return 0

    print("\\n[1/3] Dekomprimiere In-Memory Solid-Archiv...")
    tar_bytes = decompress_payload(PAYLOAD, ALGO)
    print(f"      Archiv dekomprimiert: {{len(tar_bytes):,}} Bytes Tar-Daten.")

    print("\\n[2/3] Entpacke und verifiziere Dateien (Zero-Chat-Dump)...")
    tar_buf = io.BytesIO(tar_bytes)
    extracted_count = 0
    total_written = 0

    with tarfile.open(fileobj=tar_buf, mode="r:*") as tf:
        for member in tf.getmembers():
            if not member.isfile():
                continue
            rel_name = member.name.replace("\\\\", "/")
            dest_path = os.path.join(target_dir, rel_name.replace("/", os.sep))
            dest_dir = os.path.dirname(dest_path)
            if dest_dir:
                os.makedirs(dest_dir, exist_ok=True)

            file_data = tf.extractfile(member).read()

            # Checksummen-Pruefung
            if rel_name in MANIFEST:
                exp_sz, exp_sha = MANIFEST[rel_name]
                act_sha = hashlib.sha256(file_data).hexdigest()
                if act_sha != exp_sha:
                    print(f"  [FEHLER] Checksummen-Diskrepanz bei: {{rel_name}}!")
                    print(f"    Erwartet: {{exp_sha}}")
                    print(f"    Erhalten: {{act_sha}}")
                    return 1

            with open(dest_path, "wb") as out_f:
                out_f.write(file_data)

            extracted_count += 1
            total_written += len(file_data)

            # Selektiver Statusbericht (wichtige Meilensteine)
            if rel_name.startswith("levels/") or rel_name in ("src/strudelforce.h", "src/game_logic.c", "tools/level_tool.py"):
                print(f"  [OK] {{rel_name:<46}} ({{len(file_data):>7}} B)")

    print(f"\\n[OK] {{extracted_count}} Dateien erfolgreich geschrieben ({{total_written:,}} Bytes).")

    # [3/3] Nuetzliche Pfad-Aliase fuer nahtlose Selftests
    aliases = [
        ("tests/data/Turrican 2 - World 1 - Dessert.tfx", "TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.tfx"),
        ("tests/data/Turrican 2 - World 1 - Dessert.sam", "TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.sam"),
    ]
    for src_rel, dst_rel in aliases:
        s_p = os.path.join(target_dir, src_rel.replace("/", os.sep))
        d_p = os.path.join(target_dir, dst_rel.replace("/", os.sep))
        if os.path.exists(s_p) and not os.path.exists(d_p):
            os.makedirs(os.path.dirname(d_p), exist_ok=True)
            import shutil
            shutil.copyfile(s_p, d_p)

    print("-" * 72)
    print("Naechste Schritte fuer die Leveleditor-Entwicklung in Kiwi:")
    print("  1. Level-Integritaet testen : python tools/level_tool.py validate")
    print("  2. Selftest ausfuehren      : python tools/selftest.py")
    print("  3. Simulation verifizieren  : python verify_core_sim.py --seed 1234 --ticks 2500")
    print("  4. 50Hz Terminal-Game bauen : gcc -std=c99 -O2 -Isrc src/game_main.c src/game_logic.c src/engine_core.c src/soundengine_core.c -lm -o strudelforce_game")
    print("=" * 72)

    if run_test:
        print("\\n=== Fuehre automatische Selftest-Verifikation aus ===")
        import subprocess
        selftest_py = os.path.join(target_dir, "tools", "selftest.py")
        if os.path.exists(selftest_py):
            res = subprocess.run([sys.executable, os.path.join("tools", "selftest.py")], cwd=target_dir)
            if res.returncode != 0:
                print(f"Selftest beendet mit Code {{res.returncode}}")
                return res.returncode
        sim_py = os.path.join(target_dir, "verify_core_sim.py")
        if os.path.exists(sim_py):
            res2 = subprocess.run([sys.executable, "verify_core_sim.py", "--seed", "1234", "--ticks", "2500"], cwd=target_dir)
            return res2.returncode

    return 0

def main():
    target = "."
    run_test = False
    list_only = False

    args = sys.argv[1:]
    for arg in args:
        if arg in ("--test", "-t"):
            run_test = True
        elif arg in ("--list", "-l"):
            list_only = True
        elif not arg.startswith("-"):
            target = arg

    code = extract_all(target_dir=target, run_test=run_test, list_only=list_only)
    sys.exit(code)

if __name__ == "__main__":
    main()
'''

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(script_template)

    out_size = os.path.getsize(out_path)
    return out_size

def build_injector(algo="lzma", include_embedded_music=True, out_filename=None):
    if algo == "lzma" and not HAS_LZMA:
        print("[WARNUNG] Python lzma-Modul nicht verfuegbar. Schalte um auf zlib.")
        algo = "zlib"

    files_list = list(CORE_FILES)
    if include_embedded_music:
        files_list.append("src/embedded_music.h")

    print("=" * 72)
    print("STRUDELFORCE — BUILDING KIWI SELF-EXTRACTING INJECTOR")
    print(f"Kompression : {algo.upper()} (Solid Tar)")
    print(f"Dateien     : {len(files_list)}")
    print(f"Profile     : {'STANDARD (inkl. embedded_music.h)' if include_embedded_music else 'SLIM (ohne embedded_music.h)'}")
    print("=" * 72)

    manifest, tar_bytes, total_raw = collect_manifest(files_list)

    # Solid Kompression
    if algo == "lzma":
        compressed_bytes = lzma.compress(tar_bytes, preset=9)
    else:
        compressed_bytes = zlib.compress(tar_bytes, level=9)

    b64_payload = base64.b64encode(compressed_bytes).decode("ascii")

    print(f"Rohgroesse (ungepackt) : {total_raw / 1024:,.1f} KB ({total_raw:,} Bytes)")
    print(f"Solid Tar-Groesse      : {len(tar_bytes) / 1024:,.1f} KB")
    print(f"Komprimiert ({algo.upper()}) : {len(compressed_bytes) / 1024:,.1f} KB ({len(compressed_bytes):,} Bytes)")
    print(f"Base64 Zeichen         : {len(b64_payload):,} Zeichen")

    if not out_filename:
        out_filename = "kiwi_level_injector.py"

    out_path = os.path.join(ROOT_DIR, out_filename)
    final_sz = generate_injector_script(manifest, b64_payload, algo=algo, out_path=out_path)

    print(f"\n[OK] Injector-Skript erstellt: {out_filename}")
    print(f"     Dateigroesse: {final_sz / 1024:.1f} KB ({final_sz:,} Bytes)")
    print("=" * 72)

    # Zusaetzlich kiwi_injector.py aktualisieren, wenn out_filename kiwi_level_injector.py war
    if out_filename == "kiwi_level_injector.py":
        import shutil
        legacy_path = os.path.join(ROOT_DIR, "kiwi_injector.py")
        shutil.copyfile(out_path, legacy_path)
        print(f"[OK] Auch als 'kiwi_injector.py' bereitgestellt ({final_sz:,} Bytes).")

    return final_sz

def main():
    parser = argparse.ArgumentParser(description="Build Kiwi Self-Extracting Injector")
    parser.add_argument("--algo", choices=["lzma", "zlib"], default="lzma", help="Kompressions-Algorithmus (Default: lzma)")
    parser.add_argument("--slim", action="store_true", help="Ohne src/embedded_music.h (~250 KB)")
    parser.add_argument("--out", type=str, default="kiwi_level_injector.py", help="Ausgabedatei")
    args = parser.parse_args()

    build_injector(algo=args.algo, include_embedded_music=not args.slim, out_filename=args.out)

if __name__ == "__main__":
    main()
