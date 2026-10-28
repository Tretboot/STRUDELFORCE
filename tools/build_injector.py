#!/usr/bin/env python3
"""
tools/build_injector.py - Generiert das selbstentpackende kiwi_injector.py Skript.
"""
import os
import sys
import base64
import zlib
import hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

PACK_FILES = [
    "PROJECT_BIBLE.md",
    "Makefile",
    "build_msvc.bat",
    "docs/AUDIO_MODEL_DIRECTIVE.md",
    "src/strudelforce.h",
    "src/game_logic.c",
    "src/soundengine_core.c",
    "src/engine_core.c",
    "src/test_suite.c",
    "src/game_main.c",
    "src/gui_main.c",
    "src/main_pc.c",
    "src/embedded_music.h",
    "src/retro_font.h",
    "src/core_sim.h",
    "src/game_audio.h",
    "src/soundengine_core.h",
    "src/tfmx_player.h",
    "src/mod_player.h",
    "src/sf_mixer.h",
    "src/livecode_ui.h",
    "src/voxel_render.h",
    "src/soundgodizer.h",
    "src/win_audio.h",
    "src/amiga_intro.h",
    "src/sf_interfaces.h",
    "assets/audio_reference/switch_angel_tracks.strudel",
    "assets/audio_reference/turrican2_load.tfx",
    "assets/audio_reference/turrican2_load.sam",
    "assets/audio_reference/turrican2_world1.tfx",
    "assets/audio_reference/turrican2_world1.sam",
    "assets/audio_reference/turrican2_world2.tfx",
    "assets/audio_reference/turrican2_world2.sam",
    "assets/audio_reference/ref1_wing_commander_patrol.mod",
    "assets/audio_reference/ref2_wing_commander_combat.mod",
    "levels/levels.json",
    "tests/data/selftest_song.mod",
    "tests/data/Turrican 2 - World 1 - Dessert.tfx",
    "tests/data/Turrican 2 - World 1 - Dessert.sam",
    "tools/strudel_bridge.py",
    "tools/mod_oracle.py",
    "tools/tfmx_oracle.py",
    "tools/level_tool.py",
    "tools/kiwi_handoff.py",
    "tools/selftest.py",
    "verify_core_sim.py"
]

def main():
    payload_entries = []
    total_raw = 0
    total_b64 = 0

    print("=== Generiere kiwi_injector.py ===")
    for rel_path in PACK_FILES:
        full_path = os.path.join(ROOT, rel_path)
        if not os.path.exists(full_path):
            print(f"FEHLER: Datei fehlt: {full_path}")
            sys.exit(1)
        raw_bytes = open(full_path, "rb").read()
        raw_size = len(raw_bytes)
        sha = hashlib.sha256(raw_bytes).hexdigest()
        comp = zlib.compress(raw_bytes, 9)
        b64 = base64.b64encode(comp).decode("ascii")
        total_raw += raw_size
        total_b64 += len(b64)
        print(f"  Passe ein: {rel_path} ({raw_size} B -> {len(b64)} B64)")
        payload_entries.append((rel_path.replace("\\", "/"), raw_size, sha, b64))

    injector_code = []
    injector_code.append('#!/usr/bin/env python3')
    injector_code.append('"""')
    injector_code.append('kiwi_injector.py - STRUDELFORCE Self-Extracting Handoff Injector')
    injector_code.append('')
    injector_code.append('Automatische Entpackungs- und Bereitstellungs-Routine fuer die Cloud-Sandbox.')
    injector_code.append('Enthaelt die defragmentierte, monolithische C-Codebasis, alle Audio-Referenzen,')
    injector_code.append('die PROJECT_BIBLE.md und die Kompositions-Direktive fuer das Audio-Modell.')
    injector_code.append('')
    injector_code.append('Aufruf:')
    injector_code.append('    python kiwi_injector.py             # Entpackt in das aktuelle Verzeichnis')
    injector_code.append('    python kiwi_injector.py <ziel_dir>  # Entpackt in ein Ziel-Verzeichnis')
    injector_code.append('    python kiwi_injector.py --test      # Entpackt und fuehrt Verifikation aus')
    injector_code.append('"""')
    injector_code.append('import os')
    injector_code.append('import sys')
    injector_code.append('import base64')
    injector_code.append('import zlib')
    injector_code.append('import hashlib')
    injector_code.append('')
    injector_code.append('# (Dateipfad, Unkomprimierte Groesse, SHA256, Base64-zlib-Payload)')
    injector_code.append('ARCHIVE = {')
    for rel_path, raw_sz, sha, b64 in payload_entries:
        injector_code.append(f'    "{rel_path}": ({raw_sz}, "{sha}", "{b64}"),')
    injector_code.append('}')
    injector_code.append('')
    injector_code.append('''
def extract_all(target_dir="."):
    print("=" * 70)
    print("STRUDELFORCE — KIWI SELF-EXTRACTING INJECTOR")
    print(f"Zielverzeichnis: {os.path.abspath(target_dir)}")
    print("=" * 70)

    extracted_count = 0
    total_bytes = 0

    for rel_path, (expected_sz, expected_sha, b64_payload) in ARCHIVE.items():
        out_path = os.path.join(target_dir, rel_path)
        out_dir = os.path.dirname(out_path)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)

        raw_data = zlib.decompress(base64.b64decode(b64_payload.encode("ascii")))
        actual_sha = hashlib.sha256(raw_data).hexdigest()

        if actual_sha != expected_sha:
            print(f"  [FEHLER] Checksummen-Fehler bei {rel_path}!")
            sys.exit(1)

        with open(out_path, "wb") as f:
            f.write(raw_data)

        extracted_count += 1
        total_bytes += len(raw_data)
        print(f"  [OK] {rel_path:<48} ({len(raw_data):>6} Bytes)")

    print("-" * 70)
    print(f"ERFOLG: {extracted_count} Dateien entpackt ({total_bytes} Bytes total).")
    print("=" * 70)
    print("\\nNaechste Schritte fuer die Cloud-Sandbox:")
    print("  1. Lies die Dokumentation: cat PROJECT_BIBLE.md")
    print("  2. Lies die Audio-Direktive: cat docs/AUDIO_MODEL_DIRECTIVE.md")
    print("  3. Fuehre die Testsuite aus: python tools/selftest.py")
    print("  4. Kompiliere das Spiel: make all (GCC) oder build_msvc.bat (Windows)")
    print("=" * 70)

def main():
    target = "."
    run_tests = False
    args = sys.argv[1:]

    for arg in args:
        if arg in ("--test", "-t"):
            run_tests = True
        elif not arg.startswith("-"):
            target = arg

    extract_all(target)

    if run_tests:
        print("\\n=== Fuehre automatische Selftest-Verifikation aus ===")
        import subprocess
        selftest_script = os.path.join(target, "tools", "selftest.py")
        if os.path.exists(selftest_script):
            res = subprocess.run([sys.executable, selftest_script])
            sys.exit(res.returncode)

if __name__ == "__main__":
    main()
''')

    out_file = os.path.join(ROOT, "kiwi_injector.py")
    with open(out_file, "w", encoding="utf-8") as f:
        f.write("\n".join(injector_code))

    out_size = os.path.getsize(out_file)
    print(f"ERFOLG: {out_file} erstellt ({out_size} Bytes).")
    print(f"Kompressions-Rate: {total_raw} Bytes Rohdaten -> {total_b64} Bytes Base64 ({total_b64*100.0/total_raw:.1f}%)")

if __name__ == "__main__":
    main()
