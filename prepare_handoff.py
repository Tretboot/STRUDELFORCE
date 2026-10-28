#!/usr/bin/env python3
"""
prepare_handoff.py - Automatisches Handoff-Paket fuer Claude in der KIWI-Cloud

Fuehrt die 4 Handoff-Schritte aus:
  1. Lokale C-Tests ausfuehren & Compiler-/Engine-Report in 'local_c_test_report.txt' sichern
  2. captains_log.md mit dem Uebergabe-Eintrag aktualisieren
  3. CLAUDE_MISSION.md mit der Aufgabenstellung erstellen
  4. STRUDELFORCE_Claude_Handoff.zip packen (ohne Binaries oder pycache)
   5. --lite: STRUDELFORCE_Claude_Handoff_Lite.zip (< 1 MB fuer Cloud-Upload: keine Audio-Dumps
      .wav/.adf/.mp3/.raw, nur EIN TFMX-Referenz-Track, Code-Kern zu 100% erhalten)
"""

import os
import sys
import subprocess
import shutil
import zipfile
from datetime import datetime

def step1_run_c_tests():
    print("=== SCHRITT 1: Lokale C-Tests ausfuehren & Report generieren ===")
    report_lines = []
    report_lines.append("=" * 70)
    report_lines.append("STRUDELFORCE - LOKALER C-TEST & ENGINE REPORT")
    report_lines.append(f"Zeitstempel: {datetime.now().isoformat()}")
    report_lines.append(f"Plattform: {sys.platform} ({os.name})")
    report_lines.append("=" * 70)
    report_lines.append("")

    # 1. Versuche GCC Compilation
    report_lines.append("--- 1. GCC COMPILER PRUEFUNG & BUILD ---")
    gcc_cmd = [
        "gcc", "-std=c99", "-Wall", "-Wextra", "-O2", "-Isrc",
        "src/game_main.c", "src/core_sim.c", "src/game_audio.c",
        "src/tfmx_player.c", "src/amiga_intro.c", "src/livecode_ui.c",
        "-o", "strudelforce_game", "-lm"
    ]
    try:
        res = subprocess.run(gcc_cmd, capture_output=True, text=True, timeout=30,
                             encoding="utf-8", errors="replace")
        report_lines.append(f"Kommando: {' '.join(gcc_cmd)}")
        report_lines.append(f"Returncode: {res.returncode}")
        report_lines.append("STDOUT:\n" + (res.stdout or "(leer)"))
        report_lines.append("STDERR:\n" + (res.stderr or "(leer)"))
    except Exception as e:
        report_lines.append(f"GCC-Kompilierung nicht direkt moeglich: {e}")
        report_lines.append("Hinweis fuer Claude: In der Windows-Host-Umgebung war gcc nicht im PATH. Wir testen mit MSVC cl.exe undfuehren die Binaries aus.")

    report_lines.append("")

    # 2. MSVC Compilation (falls auf Windows verfuegbar)
    report_lines.append("--- 2. MSVC BUILD & ENGINE COMPILER LOG ---")
    vcvars_paths = [
        r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ]
    bat_path = os.path.abspath("build_msvc.bat")
    if os.path.exists(bat_path):
        try:
            res_msvc = subprocess.run(["cmd", "/c", bat_path], capture_output=True, text=True, timeout=30,
                                  encoding="utf-8", errors="replace")
            report_lines.append(f"MSVC Returncode: {res_msvc.returncode}")
            report_lines.append("STDOUT:\n" + (res_msvc.stdout or "(leer)"))
            if res_msvc.stderr:
                report_lines.append("STDERR:\n" + res_msvc.stderr)
        except Exception as e:
            report_lines.append(f"MSVC Fehler: {e}")
    else:
        report_lines.append("build_msvc.bat nicht gefunden.")

    report_lines.append("")

    # 3. Phase-1 C-Integrationstest (43 Checks aller 5 Subsysteme)
    report_lines.append("--- 3. PHASE-1 INTEGRATIONSTEST (test_phase1.c) ---")
    if os.path.exists("strudelforce_test.exe"):
        try:
            res_test = subprocess.run([".\\strudelforce_test.exe"], capture_output=True, text=True, timeout=15,
                                      encoding="utf-8", errors="replace")
            report_lines.append(f"strudelforce_test Returncode: {res_test.returncode}")
            report_lines.append("OUTPUT:\n" + res_test.stdout)
            if res_test.stderr:
                report_lines.append("STDERR:\n" + res_test.stderr)
        except Exception as e:
            report_lines.append(f"Fehler bei Ausfuehrung von strudelforce_test.exe: {e}")
    else:
        report_lines.append("strudelforce_test.exe nicht vorhanden.")

    report_lines.append("")

    # 4. TFMX Audio-Playback & Gameplay Demo-Run
    report_lines.append("--- 4. TFMX AUDIO-PLAYBACK & DEMO RUN (500 Ticks Apidya Meadow) ---")
    demo_cmd = [
        ".\\strudelforce_game.exe", "--demo", "500",
        "--tfmx", "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx",
        "--audio", "handoff_audio_check.wav"
    ]
    if os.path.exists("strudelforce_game.exe"):
        try:
            res_demo = subprocess.run(demo_cmd, capture_output=True, text=True, timeout=20,
                                  encoding="utf-8", errors="replace")
            report_lines.append(f"Demo Returncode: {res_demo.returncode}")
            report_lines.append("OUTPUT:\n" + res_demo.stdout)
            if res_demo.stderr:
                report_lines.append("STDERR:\n" + res_demo.stderr)
        except Exception as e:
            report_lines.append(f"Fehler bei Demo-Run: {e}")
    else:
        report_lines.append("strudelforce_game.exe nicht vorhanden.")

    report_lines.append("")

    # 5. TFMX Importer Output Test
    report_lines.append("--- 5. PYTHON TFMX IMPORTER CHECK ---")
    imp_cmd = [
        sys.executable, "tools/tfmx_importer.py",
        "--in", "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx",
        "--dump"
    ]
    try:
        res_imp = subprocess.run(imp_cmd, capture_output=True, text=True, timeout=15,
                                 encoding="utf-8", errors="replace")
        report_lines.append(f"Importer Returncode: {res_imp.returncode}")
        report_lines.append("OUTPUT (Auszug):\n" + res_imp.stdout[:1500])
        if res_imp.stderr:
            report_lines.append("STDERR:\n" + res_imp.stderr)
    except Exception as e:
        report_lines.append(f"Fehler bei TFMX Importer Test: {e}")

    report_lines.append("")
    report_lines.append("=" * 70)
    report_lines.append("ENDE DES LOKALEN REPORTS FUER CLAUDE")
    report_lines.append("=" * 70)

    report_content = "\n".join(report_lines)
    with open("local_c_test_report.txt", "w", encoding="utf-8") as f:
        f.write(report_content)

    print("  -> 'local_c_test_report.txt' erfolgreich geschrieben.")


def step2_update_captains_log():
    print("=== SCHRITT 2: captains_log.md aktualisieren ===")
    entry_text = (
        "\n\n---\n\n"
        "## Captain's Log — Handoff an KIWI (Claude)\n\n"
        "**Handoff an KIWI (Claude):** Nach initialer lokaler Entwicklung und TFMX-Import-Tests durch Qwen "
        "und Gemini 3.8 Flash in VS Code wird der Workspace nun an Claude übergeben. "
        "Lokale C-Tests wurden in `local_c_test_report.txt` gesichert.\n"
    )

    log_paths = [
        "STRUDELFORCE_Smith_Core/captains_log.md",
        "captains_log.md"
    ]

    for lp in log_paths:
        if os.path.exists(lp):
            with open(lp, "r", encoding="utf-8") as f:
                content = f.read()
            if "**Handoff an KIWI (Claude):**" not in content:
                with open(lp, "a", encoding="utf-8") as f:
                    f.write(entry_text)
                print(f"  -> '{lp}' aktualisiert.")
            else:
                print(f"  -> '{lp}' enthaelt bereits den Handoff-Eintrag.")
        else:
            # Falls im Root noch nicht existiert, aus Smith_Core kopieren und ergaenzen
            if os.path.exists("STRUDELFORCE_Smith_Core/captains_log.md"):
                shutil.copy("STRUDELFORCE_Smith_Core/captains_log.md", lp)
                with open(lp, "a", encoding="utf-8") as f:
                    f.write(entry_text)
                print(f"  -> '{lp}' angelegt und aktualisiert.")


def step3_create_claude_mission():
    print("=== SCHRITT 3: CLAUDE_MISSION.md erstellen ===")
    mission_content = (
        "# STRUDELFORCE - Mission für Claude (KIWI)\n"
        "Stand: 2026-10-27 - finales Handoff. Details & Priorität: TODO.md; bindende Architektur-Regeln: AGENTS.md (die 3 Eisernen Gesetze).\n"
        "Deine Aufgaben in dieser Iteration:\n"
        "**AUFGABE 1 (P0, HÖCHSTE PRIORITÄT): LEVEL-DESIGN OVERHAUL**\n"
        "- Das aktuelle Level-Gerüst ist UNBRAUCHBAR: Es erlaubt derzeit nur rudimentäres Rechts/Links-Movement und hat keine echte Spielstruktur.\n"
        "- Das Level-Design muss VON GRUND AUF neu gebaut werden: echte Kollisionen, Plattformen, Vertikalität.\n"
        "- Ziel ist ein echtes, spielbares Fundament - erst DARAUF folgt das Balancing.\n"
        "- Wir brauchen strukturierte Voxel-Levels (basierend auf unserer 2.5D Architektur):\n"
        "  handgebaute Hommagen an Klassiker wie Turrican, Giana Sisters und Super Mario,\n"
        "  ZUSÄTZLICH ein prozedural generierter Roguelike-Modus, der Level-Bausteine zufällig, aber logisch zusammensetzt.\n"
        "  Nutze deine Stärke im Level-Design, um die Datenstrukturen (Arrays/Structs) in C dafür vorzubereiten.\n"
        "**AUFGABE 2: TFMX REVERSE ENGINEERING + DER ORIGINAL-SOUNDTRACK (AUDIO)**\n"
        "- Der aktuelle TFMX-Konverter/Player erzeugt fehlerhaften Sound und Ladefehler.\n"
        "- Analysiere die beiliegenden originalen .mdat/.smpl Dateien (z.B. Apidya).\n"
        "- Repariere die Byte-Offsets, Wait-States und DMA-Makros im C-Code / Python-Skript. Der Sound muss originalgetreu laufen.\n"
        "- NEU: Die originalen TFMX-Module (Apidya, Turrican etc.) sollen NICHT NUR technisch, sondern auch\n"
        "  INHALTLICH/MUSIKALISCH analysiert werden (Song-Form, Stimm-Verbrauch, Sample-Zuordnung, Arrangement-Prinzipien).\n"
        "- Ziel: EIN EIGENER ORIGINAL-SOUNDTRACK für \"STRUDELFORCE MASTER\" (Titel/Boot, Biome-Tunes, Boss-Kämpfe).\n"
        "- Lass dich von den Klassikern inspirieren, extrahiere deren originale Amiga-Samples (aus .smpl/.mdat)\n"
        "  und wiederverwende sie in unseren neuen Kompositionen.\n"
        "**AUFGABE 3: DIE VOXEL-EVOLUTION (NEUES GAMEPLAY-KONZEPT, für die spätere Implementierung)**\n"
        "- Der Spielercharakter wird modular aus Voxeln (PC) bzw. Pixel-Clustern (Amiga) zusammengesetzt.\n"
        "- Der Spieler startet als kleines, rudimentäres Konstrukt und findet in der Welt Bauteile (Beine, Arme, Waffen).\n"
        "- Durch das Einsammeln wächst der Charakter physisch, wird detaillierter und schaltet Funktionen frei.\n"
        "- Architektur-Vorbereitung: statische C-Datenstrukturen (16.16-Fixed-Point, null malloc, 512 KB Chip-RAM),\n"
        "  Bauteil-Spawns als Level-Features anbinden (-> Aufgabe 1), Amiga-Seite = Pixel-Cluster auf 6 Bitplanes.\n"
    )

    with open("CLAUDE_MISSION.md", "w", encoding="utf-8") as f:
        f.write(mission_content)

    print("  -> 'CLAUDE_MISSION.md' erfolgreich erstellt.")


def step4_create_handoff_zip(lite=False):
    zip_filename = "STRUDELFORCE_Claude_Handoff_Lite.zip" if lite else "STRUDELFORCE_Claude_Handoff.zip"
    print(f"=== SCHRITT 4: {zip_filename} erstellen{' (LITE: keine Audio-Dumps, nur 1 TFMX-Referenz-Track)' if lite else ''} ===")
    
    # Auszuschliessende Dateien & Ordner
    # .wav/.adf/.mp3/.raw: Audio- & Disketten-Dumps rigoros ausschliessen (Code-Handoff)
    excluded_extensions = {".exe", ".obj", ".o", ".a", ".lib", ".dll", ".wav", ".zip", ".pdb", ".ilk", ".adf", ".mp3", ".raw"}
    excluded_dirs = {"__pycache__", ".git", ".vscode", "releases", ".system_generated"}

    # LITE: NUR genau EIN funktionierendes Referenz-Beispiel fuer das TFMX-Reverse-Engineering
    lite_tfxmx_tracks = [
        "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.sam",
        "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx",
    ]

    file_count = 0
    with zipfile.ZipFile(zip_filename, "w", zipfile.ZIP_DEFLATED) as zf:
        # 1. Root-Dateien
        for item in os.listdir("."):
            if os.path.isfile(item):
                ext = os.path.splitext(item)[1].lower()
                if ext not in excluded_extensions and item != zip_filename:
                    # Spezifisch Markdown, Python, Makefile, C, Header, txt
                    if (item.endswith((".md", ".py", ".txt", ".c", ".h", ".bat")) or
                        item in ("Makefile", "AGENTS.md", "CLAUDE_MISSION.md", "local_c_test_report.txt")):
                        zf.write(item, item)
                        file_count += 1

        # 2. Spezifische Ordner
        include_folders = ["src", "tools", "user_music", "TFMX Professional", "STRUDELFORCE", "STRUDELFORCE_Smith_Core"]
        for fld in include_folders:
            if not os.path.exists(fld):
                continue
            if lite and fld == "TFMX Professional":
                continue  # wird unten als einziger Referenz-Track angehaengt
            for root, dirs, files in os.walk(fld):
                # Filter dirs in-place
                dirs[:] = [d for d in dirs if d not in excluded_dirs]
                for fn in files:
                    ext = os.path.splitext(fn)[1].lower()
                    if ext not in excluded_extensions:
                        full_path = os.path.join(root, fn)
                        arcname = full_path
                        zf.write(full_path, arcname)
                        file_count += 1

        # 3. LITE: der einzige TFMX-Referenz-Track
        if lite:
            for track in lite_tfxmx_tracks:
                if os.path.exists(track):
                    zf.write(track, track)
                    file_count += 1
                else:
                    print(f"  WARNUNG: Referenz-Track fehlt: {track}")

    zip_size = os.path.getsize(zip_filename)
    print(f"  -> '{zip_filename}' erfolgreich gepackt ({file_count} Dateien, {zip_size:,} Bytes).")


def main():
    lite = "--lite" in sys.argv
    print(f"Starte automatische Handoff-Vorbereitung fuer Claude{' (LITE-Modus)' if lite else '...'}")
    step1_run_c_tests()
    step2_update_captains_log()
    step3_create_claude_mission()
    step4_create_handoff_zip(lite=lite)
    print("\n[FERTIG] Handoff an Claude wurde vollstaendig vorbereitet!")

if __name__ == "__main__":
    main()
