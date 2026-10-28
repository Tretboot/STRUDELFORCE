#!/usr/bin/env python3
"""
build_code_injector.py - Generator for kiwi_code_injector.py
Packs only lightweight code, tools, docs, and build scripts into a self-extracting script.
Assets are omitted and loaded from strudel_assets.zip.txt instead.
"""
import os
import sys
import base64
import zlib
import hashlib

ROOT_DIR = os.path.abspath(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_FILE = os.path.join(ROOT_DIR, "kiwi_code_injector.py")

def collect_code_files():
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

    # 2. src/
    src_dir = os.path.join(ROOT_DIR, "src")
    for fn in sorted(os.listdir(src_dir)):
        if fn.endswith((".c", ".h")):
            files.append(f"src/{fn}")

    # 3. docs/
    docs_dir = os.path.join(ROOT_DIR, "docs")
    if os.path.exists(docs_dir):
        for fn in sorted(os.listdir(docs_dir)):
            if fn.endswith((".md", ".txt")):
                files.append(f"docs/{fn}")

    # 4. levels/
    levels_dir = os.path.join(ROOT_DIR, "levels")
    if os.path.exists(levels_dir):
        for fn in sorted(os.listdir(levels_dir)):
            files.append(f"levels/{fn}")

    # 5. tests/
    tests_dir = os.path.join(ROOT_DIR, "tests")
    if os.path.exists(tests_dir):
        for root, dirs, fnames in os.walk(tests_dir):
            for fn in sorted(fnames):
                rel_p = os.path.relpath(os.path.join(root, fn), ROOT_DIR).replace("\\", "/")
                files.append(rel_p)

    # 6. tools/
    tools_dir = os.path.join(ROOT_DIR, "tools")
    if os.path.exists(tools_dir):
        for fn in sorted(os.listdir(tools_dir)):
            if fn.endswith(".py") and fn not in ("build_injector_v3.py", "build_code_injector.py"):
                files.append(f"tools/{fn}")

    # KEIN assets/ Ordner!
    return sorted(files)

def build_code_injector():
    files = collect_code_files()
    archive_dict = {}

    total_raw = 0
    total_comp = 0

    print(f"Sammle {len(files)} Quellcode-Dateien fuer kiwi_code_injector.py...")

    for rel in files:
        fp = os.path.join(ROOT_DIR, rel.replace("/", os.sep))
        with open(fp, "rb") as f:
            data = f.read()

        raw_sz = len(data)
        sha = hashlib.sha256(data).hexdigest()
        comp = zlib.compress(data, 9)
        b64 = base64.b64encode(comp).decode("ascii")

        archive_dict[rel] = (raw_sz, sha, b64)
        total_raw += raw_sz
        total_comp += len(comp)

    print(f"Rohgroesse: {total_raw / 1024:.1f} KB, Komprimiert: {total_comp / 1024:.1f} KB")

    with open(OUT_FILE, "w", encoding="utf-8") as out:
        out.write('#!/usr/bin/env python3\n')
        out.write('"""\n')
        out.write('kiwi_code_injector.py - STRUDELFORCE v0.2.0 Lightweight Code Injector\n')
        out.write('Enthaelt die vollstaendige C-Codebasis, Tools, Docs und Build-Skripte.\n')
        out.write('Binaere Assets werden automatisch aus strudel_assets.zip.txt entpackt.\n\n')
        out.write('Aufruf:\n')
        out.write('    python kiwi_code_injector.py             # Entpackt ins aktuelle Verzeichnis\n')
        out.write('    python kiwi_code_injector.py <ziel_dir>  # Entpackt in Ziel-Verzeichnis\n')
        out.write('    python kiwi_code_injector.py --test      # Entpackt und fuehrt Verifikation aus\n')
        out.write('"""\n')
        out.write('import os\nimport sys\nimport base64\nimport zlib\nimport hashlib\n\n')
        out.write('# (Dateipfad, Unkomprimierte Groesse, SHA256, Base64-zlib-Payload)\n')
        out.write('ARCHIVE = {\n')
        for rel, (sz, sha, b64) in archive_dict.items():
            out.write(f'    {repr(rel)}: ({sz}, {repr(sha)}, {repr(b64)}),\n')
        out.write('}\n\n')
        out.write('''def extract_all(target_dir="."):
    print("=" * 72)
    print("STRUDELFORCE v0.2.0 — KIWI LIGHTWEIGHT CODE INJECTOR")
    print(f"Zielverzeichnis: {os.path.abspath(target_dir)}")
    print("=" * 72)

    extracted_count = 0
    total_bytes = 0

    # 1. Quellcode und Textdateien entpacken (erzwingt os.makedirs vor jedem Schreiben)
    for rel_path, (expected_sz, expected_sha, b64_payload) in ARCHIVE.items():
        out_path = os.path.join(target_dir, rel_path.replace("/", os.sep))
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
        print(f"  [OK] {rel_path:<52} ({len(raw_data):>7} Bytes)")

    print("-" * 72)
    print(f"ERFOLG (Code): {extracted_count} Dateien entpackt ({total_bytes} Bytes total).")

    # 2. Pruefen, ob strudel_assets.zip.txt vorhanden ist und via zipfile entpacken
    asset_candidates = [
        "strudel_assets.zip.txt",
        "strudel_assets.zip",
        os.path.join(target_dir, "strudel_assets.zip.txt"),
        os.path.join(target_dir, "strudel_assets.zip")
    ]
    found_assets = None
    for cand in asset_candidates:
        if os.path.isfile(cand):
            found_assets = cand
            break

    if found_assets:
        print(f"\\n[INFO] Gefundenes Asset-Archiv: {found_assets}")
        print("Entpacke binaere Assets via zipfile...")
        import zipfile
        with zipfile.ZipFile(found_assets, "r") as zf:
            for member in zf.infolist():
                target_path = os.path.join(target_dir, member.filename.replace("/", os.sep))
                target_dir_name = os.path.dirname(target_path)
                if target_dir_name:
                    os.makedirs(target_dir_name, exist_ok=True)
                if not member.is_dir():
                    with zf.open(member) as s, open(target_path, "wb") as d:
                        d.write(s.read())
                    print(f"  [ASSET] {member.filename:<50} ({member.file_size:>7} Bytes)")
        print("[OK] Alle Assets erfolgreich entpackt!")
    else:
        print("\\n[HINWEIS] 'strudel_assets.zip.txt' wurde noch nicht im Verzeichnis gefunden.")
        print("          Bitte laden Sie 'strudel_assets.zip.txt' hoch und platzieren Sie es hier.")

    # 3. Pruefen, ob strudel_tfmx.zip.txt vorhanden ist und via zipfile entpacken
    tfmx_candidates = [
        "strudel_tfmx.zip.txt",
        "strudel_tfmx.zip",
        os.path.join(target_dir, "strudel_tfmx.zip.txt"),
        os.path.join(target_dir, "strudel_tfmx.zip")
    ]
    found_tfmx = None
    for cand in tfmx_candidates:
        if os.path.isfile(cand):
            found_tfmx = cand
            break

    if found_tfmx:
        print(f"\\n[INFO] Gefundenes TFMX-Archiv: {found_tfmx}")
        print("Entpacke TFMX-Soundtracks via zipfile...")
        import zipfile
        with zipfile.ZipFile(found_tfmx, "r") as zf:
            for member in zf.infolist():
                target_path = os.path.join(target_dir, member.filename.replace("/", os.sep))
                target_dir_name = os.path.dirname(target_path)
                if target_dir_name:
                    os.makedirs(target_dir_name, exist_ok=True)
                if not member.is_dir():
                    with zf.open(member) as s, open(target_path, "wb") as d:
                        d.write(s.read())
                    print(f"  [TFMX] {member.filename:<50} ({member.file_size:>7} Bytes)")
        print("[OK] Alle TFMX-Tracks erfolgreich entpackt!")
    else:
        print("\\n[HINWEIS] 'strudel_tfmx.zip.txt' wurde nicht im Verzeichnis gefunden.")
        print("          Optional: Laden Sie 'strudel_tfmx.zip.txt' hoch fuer vollstaendige TFMX-Musik.")

    print("=" * 72)
    print("\\nGoldene Sandbox-Regeln:")
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
    print(f"\n[OK] kiwi_code_injector.py erfolgreich generiert!")
    print(f"Dateigroesse: {final_sz / 1024:.1f} KB ({final_sz} Bytes)")
    return final_sz

if __name__ == "__main__":
    build_code_injector()

