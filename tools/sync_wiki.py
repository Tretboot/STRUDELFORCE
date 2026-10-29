#!/usr/bin/env python3
"""
tools/sync_wiki.py - Synchronisiert das lokale wiki/ Verzeichnis mit dem GitHub Wiki
Repository https://github.com/Tretboot/STRUDELFORCE.wiki.git
"""

import os
import sys
import shutil
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WIKI_DIR = os.path.join(ROOT, 'wiki')
TMP_WIKI_REPO = os.path.join(ROOT, 'tmp', 'wiki_clone')
WIKI_GIT_URL = 'https://github.com/Tretboot/STRUDELFORCE.wiki.git'

def run_cmd(cmd, cwd=None):
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, shell=True)
    return res.returncode, res.stdout.strip(), res.stderr.strip()

def main():
    print("=" * 70)
    print("STRUDELFORCE GITHUB WIKI SYNCHRONIZER")
    print("=" * 70)

    if not os.path.isdir(WIKI_DIR):
        print(f"[-] FEHLER: Wiki-Verzeichnis nicht gefunden: {WIKI_DIR}")
        sys.exit(1)

    wiki_files = [f for f in os.listdir(WIKI_DIR) if f.endswith('.md')]
    print(f"[+] Lokale Wiki-Seiten gefunden ({len(wiki_files)} Dateien):")
    for f in sorted(wiki_files):
        print(f"    - {f}")

    print("\n[*] Pruefe GitHub Wiki Remote-Repository...")
    code, out, err = run_cmd(f'git ls-remote "{WIKI_GIT_URL}"')
    if code != 0:
        print("\n" + "!" * 70)
        print("[!] HINWEIS: Das GitHub Wiki-Repository existiert noch nicht auf GitHub!")
        print("!" * 70)
        print("GitHub provisioniert das interne Wiki-Git-Repository erst in dem Moment,")
        print("in dem der Repository-Besitzer einmalig im Browser die erste Seite anlegt.\n")
        print("SO INITIALISIERST DU ES IN 10 SEKUNDEN:")
        print("  1. Oeffne im Browser:")
        print("     https://github.com/Tretboot/STRUDELFORCE/wiki")
        print("  2. Klicke auf den gruenen Button 'Create the first page' (oder 'Save page').")
        print("  3. Fuehre danach dieses Skript erneut aus:")
        print("     python tools/sync_wiki.py")
        print("\nSobald die erste Seite existiert, laedt dieses Skript alle 8 Seiten")
        print("vollautomatisch mit Historie und Navigation hoch!")
        print("=" * 70)
        return

    print("[+] Wiki-Remote-Repository ist aktiv!")
    os.makedirs(os.path.dirname(TMP_WIKI_REPO), exist_ok=True)
    if os.path.isdir(TMP_WIKI_REPO):
        shutil.rmtree(TMP_WIKI_REPO)

    print(f"[*] Klone {WIKI_GIT_URL} nach tmp/wiki_clone...")
    code, out, err = run_cmd(f'git clone "{WIKI_GIT_URL}" "{TMP_WIKI_REPO}"')
    if code != 0:
        print(f"[-] Fehler beim Klonen: {err}")
        sys.exit(1)

    print("[*] Kopiere Wiki-Seiten in das Repository...")
    for f in wiki_files:
        src = os.path.join(WIKI_DIR, f)
        dst = os.path.join(TMP_WIKI_REPO, f)
        shutil.copy2(src, dst)
        print(f"    -> {f}")

    print("[*] Stage und committe Aenderungen...")
    run_cmd('git add .', cwd=TMP_WIKI_REPO)
    code, out, _ = run_cmd('git status --porcelain', cwd=TMP_WIKI_REPO)
    if not out:
        print("[+] GitHub Wiki ist bereits auf dem neuesten Stand! Keine Aenderungen.")
        return

    commit_msg = "Update STRUDELFORCE Wiki (v0.3.1 Documentation & Architecture)"
    code, out, err = run_cmd(f'git commit -m "{commit_msg}"', cwd=TMP_WIKI_REPO)
    print(f"[+] Commit erstellt: {out.splitlines()[0] if out else 'OK'}")

    print("[*] Pushe Aenderungen zu GitHub Wiki...")
    code, out, err = run_cmd('git push origin master', cwd=TMP_WIKI_REPO)
    if code != 0:
        # Manche Wikis nutzen main statt master
        code, out, err = run_cmd('git push origin main', cwd=TMP_WIKI_REPO)

    if code == 0:
        print("\n" + "=" * 70)
        print("[SUCCESS] Alle STRUDELFORCE Wiki-Seiten wurden erfolgreich synchronisiert!")
        print("Live ansehen unter:")
        print("  https://github.com/Tretboot/STRUDELFORCE/wiki")
        print("=" * 70)
    else:
        print(f"[-] Fehler beim Pushen: {err}")

if __name__ == '__main__':
    main()

