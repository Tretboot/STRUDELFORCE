#!/usr/bin/env python3
"""
build_smart_injector.py - Generator for kiwi_injector_v5.py
Compresses all payload files with zlib (level 9) before Base64 encoding.
Contains complete C codebase, tools, JSONs, docs, and the 8 whitelisted TFMX tracks.
Target size: well below LibreChat's 1,000,000 character limit (~890 KB).
"""
import os
import sys
import base64
import zlib
import hashlib

ROOT_DIR = os.path.abspath(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_FILE = os.path.join(ROOT_DIR, "kiwi_injector_v5.py")

AUDIO_WHITELIST = [
    # Turrican 2: "Load", "World 1 - Dessert", "World 2 - Cave"
    ("TFMX Professional/Turrican 2/Turrican 2 - Load.tfx", "TFMX Professional/Turrican 2/Turrican 2 - Load.sam"),
    ("TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.tfx", "TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.sam"),
    ("TFMX Professional/Turrican 2/Turrican 2 - World 2 - Cave.tfx", "TFMX Professional/Turrican 2/Turrican 2 - World 2 - Cave.sam"),
    # Turrican 3: "Loader", "World 1 - Factory", "World 2 - Dungeons"
    ("TFMX Professional/Turrican 3/Turrican 3 - Loader.tfx", "TFMX Professional/Turrican 3/Turrican 3 - Loader.sam"),
    ("TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.tfx", "TFMX Professional/Turrican 3/Turrican 3 - World 1 - Factory.sam"),
    ("TFMX Professional/Turrican 3/Turrican 3 - World 2 - Dungeons.tfx", "TFMX Professional/Turrican 3/Turrican 3 - World 2 - Dungeons.sam"),
    # Apidya: "Ongame 1 - Meadow", "Ongame 5 - Bosspanic"
    ("TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx", "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.sam"),
    ("TFMX Professional/Apidya/Apidya - Ongame 5 - Bosspanic.tfx", "TFMX Professional/Apidya/Apidya - Ongame 5 - Bosspanic.sam"),
]

def collect_files():
    files = []

    # 1. Root files
    root_candidates = [
        "Makefile", "build_msvc.bat", ".gitignore", "README.md",
        "PROJECT_BIBLE.md", "KIWI_SURVIVAL_GUIDE.md", "verify_core_sim.py",
        "verify_audio_matrix.py", "verify_headless.py", "verify_livecode.py"
    ]
    for fn in root_candidates:
        fp = os.path.join(ROOT_DIR, fn)
        if os.path.exists(fp):
            files.append(fn)

    # 2. src/ (all C source and headers)
    src_dir = os.path.join(ROOT_DIR, "src")
    for fn in sorted(os.listdir(src_dir)):
        if fn.endswith((".c", ".h")):
            files.append(f"src/{fn}")

    # 3. docs/ (essential active architectural & design documents)
    active_docs = [
        "AUDIO_MODEL_DIRECTIVE.md", "KIWI_PERSISTENCE_GUIDE.md", "LORE.md",
        "BIOMES.md", "ITEMS_LEVELUP.md", "MUSIC_SYSTEM.md", "BUILD.md",
        "AMIGA_HARDWARE_TARGET.md"
    ]
    for fn in active_docs:
        p = f"docs/{fn}"
        if os.path.exists(os.path.join(ROOT_DIR, p)):
            files.append(p)

    # 4. levels/ (all game level JSONs)
    levels_dir = os.path.join(ROOT_DIR, "levels")
    if os.path.exists(levels_dir):
        for fn in sorted(os.listdir(levels_dir)):
            if fn.endswith(".json"):
                files.append(f"levels/{fn}")

    # 5. tests/ (minimal test fixture data)
    tests_fixtures = [
        "tests/LAST_KIWI_TEST_RUN.txt",
        "tests/data/selftest_song.mod",
        "tests/data/selftest_song.sfsong.json"
    ]
    for p in tests_fixtures:
        if os.path.exists(os.path.join(ROOT_DIR, p)):
            files.append(p)

    # 6. tools/ (test runners, verification oracles and code generators)
    needed_tools = [
        "selftest.py", "c_vs_oracle.py", "tfmx_oracle.py", "mod_oracle.py",
        "level_tool.py", "strudel_bridge.py", "c_parse_check.py", "gen_sources.py",
        "gen_embedded_music.py", "stage_amiga_music.py", "build_lang.py", "tfmx_importer.py",
        "kiwi_handoff.py"
    ]
    for fn in needed_tools:
        p = f"tools/{fn}"
        if os.path.exists(os.path.join(ROOT_DIR, p)):
            files.append(p)

    # 7. assets/ (JSONs, strudel tracks - excluding heavy non-whitelisted wav/mod audio)
    assets_dir = os.path.join(ROOT_DIR, "assets")
    if os.path.exists(assets_dir):
        for root, dirs, fnames in os.walk(assets_dir):
            for fn in sorted(fnames):
                if fn.endswith((".json", ".strudel", ".txt")):
                    rel_p = os.path.relpath(os.path.join(root, fn), ROOT_DIR).replace("\\", "/")
                    files.append(rel_p)

    # 8. Whitelisted Audio (8 tracks = 16 files)
    for tfx, sam in AUDIO_WHITELIST:
        for p in (tfx, sam):
            fp = os.path.join(ROOT_DIR, p.replace("/", os.sep))
            if os.path.exists(fp):
                files.append(p)
            else:
                print(f"[FEHLER] Whitelist-Audiodatei nicht gefunden: {p}")
                sys.exit(1)

    return sorted(list(set(files)))

def build_smart_injector():
    files = collect_files()
    archive_dict = {}

    total_raw = 0
    total_comp = 0

    print("=" * 72)
    print("STRUDELFORCE — BUILDING SMART INJECTOR V5 (ZLIB LEVEL 9 + BASE64)")
    print(f"Ziel-Skript: {OUT_FILE}")
    print(f"Dateien gesamt: {len(files)} (inkl. 8 whitelisted TFMX-Tracks / 16 Dateien)")
    print("=" * 72)

    for rel in files:
        fp = os.path.join(ROOT_DIR, rel.replace("/", os.sep))
        with open(fp, "rb") as f:
            raw_bytes = f.read()

        raw_sz = len(raw_bytes)
        sha = hashlib.sha256(raw_bytes).hexdigest()
        
        # 1. ZLIB Kompression (Level 9)
        compressed_data = zlib.compress(raw_bytes, level=9)
        
        # 2. Base64-Kodierung der komprimierten Daten
        b64_string = base64.b64encode(compressed_data).decode("ascii")

        archive_dict[rel] = (raw_sz, sha, b64_string)
        total_raw += raw_sz
        total_comp += len(compressed_data)

    print(f"Rohgroesse: {total_raw / 1024:.1f} KB, Zlib-Komprimiert: {total_comp / 1024:.1f} KB")

    with open(OUT_FILE, "w", encoding="utf-8") as out:
        out.write('#!/usr/bin/env python3\n')
        out.write('"""\n')
        out.write('kiwi_injector_v5.py - STRUDELFORCE v0.2.0 Smart Monolithic Injector\n')
        out.write('Zlib-komprimierter Base64-Payload (< 1 Million Zeichen fuer LibreChat).\n')
        out.write('Enthaelt die vollstaendige C-Codebasis, Tools, Docs, JSONs und die 8 Whitelist-Soundtracks:\n')
        out.write('  - Turrican 2: Load, World 1 - Dessert, World 2 - Cave\n')
        out.write('  - Turrican 3: Loader, World 1 - Factory, World 2 - Dungeons\n')
        out.write('  - Apidya: Ongame 1 - Meadow, Ongame 5 - Bosspanic\n\n')
        out.write('Aufruf:\n')
        out.write('    python kiwi_injector_v5.py             # Entpackt ins aktuelle Verzeichnis\n')
        out.write('    python kiwi_injector_v5.py <ziel_dir>  # Entpackt in Ziel-Verzeichnis\n')
        out.write('    python kiwi_injector_v5.py --test      # Entpackt und fuehrt Verifikation aus\n')
        out.write('"""\n')
        out.write('import os\nimport sys\nimport shutil\nimport base64\nimport zlib\nimport hashlib\n\n')
        out.write('# (Dateipfad, Unkomprimierte Groesse, SHA256, Base64-zlib-Payload)\n')
        out.write('ARCHIVE = {\n')
        for rel, (sz, sha, b64) in archive_dict.items():
            out.write(f'    {repr(rel)}: ({sz}, {repr(sha)}, {repr(b64)}),\n')
        out.write('}\n\n')
        out.write('''def extract_all(target_dir="."):
    print("=" * 72)
    print("STRUDELFORCE v0.2.0 — KIWI SMART INJECTOR V5")
    print(f"Zielverzeichnis: {os.path.abspath(target_dir)}")
    print("=" * 72)

    extracted_count = 0
    total_bytes = 0

    # 1. Haupt-Extraktion: Base64 decode -> zlib decompress -> os.makedirs -> Datei schreiben
    for rel_path, (expected_sz, expected_sha, b64_payload) in ARCHIVE.items():
        out_path = os.path.join(target_dir, rel_path.replace("/", os.sep))
        out_dir = os.path.dirname(out_path)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)

        raw_bytes = zlib.decompress(base64.b64decode(b64_payload.encode("ascii")))
        actual_sha = hashlib.sha256(raw_bytes).hexdigest()

        if actual_sha != expected_sha:
            print(f"  [FEHLER] Checksummen-Fehler bei {rel_path}!")
            sys.exit(1)

        with open(out_path, "wb") as f:
            f.write(raw_bytes)

        extracted_count += 1
        total_bytes += len(raw_bytes)
        if rel_path.startswith("TFMX Professional/") or rel_path.endswith((".json", ".c", ".h")):
            print(f"  [OK] {rel_path:<56} ({len(raw_bytes):>7} Bytes)")

    print("-" * 72)
    print(f"ERFOLG: {extracted_count} Dateien entpackt ({total_bytes} Bytes total).")

    # 2. Nahtlose Verlinkung / Bereitstellung fuer Test-Suiten und Assets
    aliases = [
        ("TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.tfx", "tests/data/Turrican 2 - World 1 - Dessert.tfx"),
        ("TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.sam", "tests/data/Turrican 2 - World 1 - Dessert.sam"),
        ("TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx", "tests/data/Apidya - Ongame 1 - Meadow.tfx"),
        ("TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.sam", "tests/data/Apidya - Ongame 1 - Meadow.sam"),
        ("TFMX Professional/Turrican 2/Turrican 2 - Load.tfx", "assets/audio_reference/turrican2_load.tfx"),
        ("TFMX Professional/Turrican 2/Turrican 2 - Load.sam", "assets/audio_reference/turrican2_load.sam"),
        ("TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.tfx", "assets/audio_reference/turrican2_world1.tfx"),
        ("TFMX Professional/Turrican 2/Turrican 2 - World 1 - Dessert.sam", "assets/audio_reference/turrican2_world1.sam"),
        ("TFMX Professional/Turrican 2/Turrican 2 - World 2 - Cave.tfx", "assets/audio_reference/turrican2_world2.tfx"),
        ("TFMX Professional/Turrican 2/Turrican 2 - World 2 - Cave.sam", "assets/audio_reference/turrican2_world2.sam"),
    ]
    for src_rel, dst_rel in aliases:
        s_p = os.path.join(target_dir, src_rel.replace("/", os.sep))
        d_p = os.path.join(target_dir, dst_rel.replace("/", os.sep))
        if os.path.exists(s_p) and not os.path.exists(d_p):
            os.makedirs(os.path.dirname(d_p), exist_ok=True)
            shutil.copyfile(s_p, d_p)

    print("=" * 72)
    print("Goldene Sandbox-Regeln:")
    print("  1. ZERO-CHAT-DUMP: Kein Code/Hex-Dumps im Chat ausgeben.")
    print("  2. CREATE_FILE API: Fuer garantierte Persistenz verwenden.")
    print("  3. MAX 3-5 SUBAGENTEN pro Iteration delegieren.")
    print("  4. ZERO-FLOAT: Keine Floats/Doubles im Simulations-Loop.")
    print("  5. VERIFIKATION: python tools/selftest.py ausfuehren.")
    print("=" * 72)

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
            res = subprocess.run([sys.executable, os.path.join("tools", "selftest.py")], cwd=target)
            sys.exit(res.returncode)

if __name__ == "__main__":
    main()
''')

    final_sz = os.path.getsize(OUT_FILE)
    print(f"\n[OK] kiwi_injector_v5.py erfolgreich generiert!")
    print(f"Dateigroesse: {final_sz / 1024:.1f} KB ({final_sz / (1024*1024):.2f} MB, {final_sz} Bytes)")
    return final_sz

if __name__ == "__main__":
    build_smart_injector()

