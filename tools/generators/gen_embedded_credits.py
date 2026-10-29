# tools/generators/gen_embedded_credits.py
import os
import textwrap

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def escape_c_string(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')

def sanitize_ascii(s):
    # German umlauts and typographics to clean ASCII for retro display
    replacements = [
        ('ä', 'ae'), ('ö', 'oe'), ('ü', 'ue'),
        ('Ä', 'Ae'), ('Ö', 'Oe'), ('Ü', 'Ue'),
        ('ß', 'ss'), ('—', '--'), ('–', '-'),
        ('’', "'"), ('“', '"'), ('”', '"'),
        ('»', '>>'), ('«', '<<'), ('…', '...'),
        ('→', '->'), ('←', '<-'), ('•', '*'),
        ('`', "'"), ('\t', '    ')
    ]
    for orig, rep in replacements:
        s = s.replace(orig, rep)
    # Remove any non-ASCII characters
    return s.encode('ascii', errors='replace').decode('ascii')

def main():
    credits_lines = [
        "==========================================================================",
        "  S T R U D E L F O R C E   0 . 3 . 1   --   A B S P A N N   &   C R E D I T S",
        "==========================================================================",
        "",
        "  Chris Huelsbeck Tribute | Manfred Trenz Turrican II Benchmark",
        "  50Hz PAL Fixed-Point Engine | Cleanroom Cyber-Paula TFMX Synthesizer",
        "",
        "  Entwickelt im Verbund autonomer KI-Entwickler mit Human-in-the-Loop Paarprogrammierung.",
        "  Alle Systeme wurden von Grund auf nach Amiga-Originalspezifikationen",
        "  und Cleanroom-Reverse-Engineering in reinem ANSI-C realisiert.",
        "",
        "  [ TASTENSTEUERUNG IM ABSPANN: ESC = ZURUECK | SPACE = PAUSE | RAUF/RUNTER = SCHNELLER ]",
        "",
        "==========================================================================",
        "  KAPITEL I: SOUND-ARCHITEKTUR & CYBER-PAULA TFMX SPEZIFIKATION",
        "==========================================================================",
        "",
        "  Die Sound-Engine emuliert die Paula-DMA-Hardware (MOS Technology 8364) des",
        "  Amiga 500 mit 4 Hardware-Kanaelen, erweitert um einen 8-Stimmen-Softwaremixer",
        "  mit 16.16 Fixed-Point Panning, Resonanz-Tiefpassfilter und Soundgodizer.",
        "",
        "  TFMX V1.5 / PRO / 7-STIMMEN REPLAY:",
        "  - Unterstuetzt mdat. und smpl. Container mit dynamischer Makro-Abarbeitung",
        "  - Pattern-Kompression, Note-Slides, Vibrato, Portamento, Envelopes",
        "  - 50Hz VBlank Taktung synchron zum Amiga PAL-Rasterstrahl",
        "  - Original Soundtracks: Apidya Meadow, Turrican 2 Desert & Cave",
        "  - Turrican 3 World 1 Factory mit Chris Huelsbecks Tempo=0 Bugfix (Speed 2)",
        "  - Software Manager Titel Subsongs (Chris Huelsbeck / Kaiko Sound System)",
        "",
        "  PROTRACKER MOD PLAYER:",
        "  - Echtes 4-Kanal Amiga ProTracker V2.3 Replay mit ST-01 / ST-02 Samples",
        "  - Arpeggio (0xy), Portamento (1xx/2xx/3xx), Vibrato (4xy), Volume Slide (Axy)",
        "  - Praeziser 3.546895 MHz Paula-Taktteiler fuer bit-genaue Frequenzen",
        "  - Demoszene Cracktro Tunes: Fairlight, Skid Row, Razor 1911, Paradox",
        "  - Pop-Klassiker: New Order 'Blue Monday' (KGB Demo Disk Edition)",
        "  - Wing Commander Commander Office & Lost Patrol (MOD Klassiker)",
        "",
        "  STRUDEL LIVE-CODING ENGINE:",
        "  - Cleanroom Mini-Notation Parser nach TidalCycles / Strudel.cc Syntax",
        "  - Zyklusrand-Swap: Pattern-Wechsel rasten atomar auf 16-Step-Grenzen ein",
        "  - Reaktivitaet: Waffenwechsel schalten TFMX Bassline-Makros in Echtzeit um",
        "  - Unterwasser-Biome: Oeffnen der Schatztruhe aktiviert den Amiga-Filter",
        "    ueber CIA-A Register $BFE001 (Hardware Lowpass)",
        "",
        "==========================================================================",
        "  KAPITEL II: 2.5D VOXEL-ENGINE & AMIGA HARDWARE REVERSE-ENGINEERING",
        "==========================================================================",
        "",
        "  DIE DREI EISERNEN GESETZE VON STRUDELFORCE:",
        "  1. Amiga-First Core: 100% Fixed-Point (16.16), 0.0 Floats in der Simulation!",
        "     Null malloc im Gameplay-Loop. Kompatibel mit 512 KB Chip-RAM (A500 OCS).",
        "  2. Kein Minecraft-Look: 2.5D Tiefen-Extrusion aus 2D-Sprites mit",
        "     Face-Shading, Ambient Occlusion und Amiga Copper-Hintergruenden.",
        "  3. Cyber-Paula Audio: Hyper-reaktive Kopplung zwischen Waffen, Schild,",
        "     Livecoding-Buffer und 50Hz Makro-Synthese.",
        "",
        "  GRAFIK-PIPELINE & SHADER ENGINE:",
        "  - OCS/AGA Dual-Bitplane Pipeline mit 32-Farben Dual-Playfield Emulation",
        "  - Copper-Sky: Dynamische 80-Band Amiga Copper-Verlaeufe",
        "  - 4 Parallax-Layer: Deep Sky, Ruinen-Saeulen, Spielfeld, Vordergrund-Rohre",
        "  - Turrican 2 Sprite Matrix: 14 Frames Lauf- und Sprunganimation",
        "  - RetroArch Demoszene-Filter: Sony Trinitron Aperture Grille, Scanlines,",
        "    Bilineare Weichzeichnung und Phosphor-Glow ohne externe Grafik-Bibliotheken",
        "",
        "==========================================================================",
        "  KAPITEL III: CONTROLLER, GAMEPLAY & TURRICAN 2 LEVEL-BENCHMARK",
        "==========================================================================",
        "",
        "  STEUERUNGSSYSTEM:",
        "  - XInput & DirectInput Unterstuetzung fuer moderne und klassische Pads",
        "  - Retro-Joystick Support: Competition Pro USB (Oben = Springen)",
        "  - 360-Grad invertierbarer Laserstrahl ueber rechten Analog-Stick",
        "  - Morph-Ball Gyro-Rad: Zerstoert Barrieren und ueberrollt Grunts",
        "  - Destructible Blocks: Fragezeichen-Bloecke geben Diamanten & Upgrades",
        "  - 3 Leben-System mit Extra-Leben an den aeusseren Welt-Raendern",
        "",
        "  KAMPAGNE & WELTEN (5 BIOME):",
        "  - Welt 1: Chrome Tempest (Turrican Mechanik-Welt, Blitz-Himmel)",
        "  - Welt 2: The Great Bath (Unterwasser-Ruinen, Chris Huelsbeck Tribute)",
        "  - Welt 3: Mechanical Hive (Alien-Bienenstock, Laser-Draht-Fallen)",
        "  - Welt 4: Molten Core (Magma-Kavernen, Waerme-Verzerrung)",
        "  - Welt 5: Void Sanctuary (Weltall-Schrein, Schwerelosigkeit, Mini-Bosse)",
        "",
        "==========================================================================",
        "  KAPITEL IV: DEMOSZENE GREETINGS & CREDITS",
        "==========================================================================",
        "",
        "  SPECIAL GREETINGS AN DIE AMIGA & ATARI DEMOSZENE:",
        "  Fairlight (FLT) * Skid Row * Razor 1911 * Paradox * Anthrox * Silents *",
        "  Red Sector Inc. (RSI) * Kefrens * Scoopex * Future Crew * Triton *",
        "  Trsi * Crusaders * Phenomena * Melon Dezign * The Silents * Flash Production",
        "",
        "  EHRE AN DIE AMIGA-PIONIERE:",
        "  Chris Huelsbeck (TFMX Sound System, Turrican 1-3, Apidya, Great Giana Sisters)",
        "  Manfred Trenz (Turrican Schoepfer & Sprite-Magier)",
        "  Jochen Hippel (Mad Max chiptune legend)",
        "  Ramiro Vaca, Peter Thierolf, Thomas Engel (Rainbow Arts & Factor 5)",
        "  Jay Miner, RJ Mical, Dave Needle (Erfinder des Amiga 1000 & Paula Chip)",
        "",
        "  DEVELOPMENT & ENGINEERING:",
        "  KIWI Autonomous Sandbox Engine * Meeseeks Multi-Agent Cluster *",
        "  DeepMind Agentic Assistant Antigravity * tretboot (Human-in-the-Loop Vision)",
        "",
        "  *** KEEP THE AMIGA SPIRIT ALIVE *** STRUDELFORCE 0.3.1 RELEASE ***",
        "",
        ""
    ]

    phase0_line = 0
    phase1_line = 13
    phase2_line = 44
    phase3_line = 66
    phase4_line = 85

    # Kapitel V: Das vollstaendige Captain's Log
    captains_log_line = len(credits_lines)
    credits_lines.append("==========================================================================")
    credits_lines.append("  KAPITEL V: CAPTAIN'S LOG - DIE FORSCHUNGS- & ENTWICKLUNGSCHRONIK")
    credits_lines.append("==========================================================================")
    credits_lines.append("")
    credits_lines.append("  Vollstaendiges chronologisches Forschungstagebuch des STRUDELFORCE-")
    credits_lines.append("  Entwicklerverbunds (Human-in-the-Loop & Multi-Agent Swarm).")
    credits_lines.append("  Dokumentation aller Meeseeks-Iterationen, Architektur-Entscheidungen,")
    credits_lines.append("  Stolpersteine, Durchbrueche und Handoffs.")
    credits_lines.append("")
    credits_lines.append("  [ HINWEIS: MUSIK- & SUBSONG-WECHSEL ERFOLGT AUTOMATISCH / TASTE M ]")
    credits_lines.append("")

    log_path = os.path.join(ROOT, 'captains_log.md')
    if os.path.isfile(log_path):
        with open(log_path, 'r', encoding='utf-8') as f:
            log_text = f.read()

        for raw_line in log_text.splitlines():
            clean = sanitize_ascii(raw_line.strip())
            if not clean:
                credits_lines.append("")
                continue
            if clean.startswith('# '):
                credits_lines.append("")
                credits_lines.append("==========================================================================")
                credits_lines.append("  " + clean[2:].upper())
                credits_lines.append("==========================================================================")
                credits_lines.append("")
            elif clean.startswith('## '):
                credits_lines.append("")
                credits_lines.append("--- [ " + clean[3:].upper() + " ] ---")
                credits_lines.append("")
            elif clean.startswith('### '):
                credits_lines.append("")
                credits_lines.append("  >> " + clean[4:].upper())
            elif clean.startswith('- '):
                wrapped = textwrap.wrap(clean[2:], width=70)
                for idx, w in enumerate(wrapped):
                    prefix = "  * " if idx == 0 else "    "
                    credits_lines.append(prefix + w)
            elif clean.startswith('> '):
                wrapped = textwrap.wrap(clean[2:], width=70)
                for w in wrapped:
                    credits_lines.append("  | " + w)
            else:
                wrapped = textwrap.wrap(clean, width=74)
                for w in wrapped:
                    credits_lines.append("  " + w)

        credits_lines.append("")
        credits_lines.append("==========================================================================")
        credits_lines.append("  ENDE DES CAPTAIN'S LOGS -- FORSCHUNGSTAGEBUCH ABGESCHLOSSEN")
        credits_lines.append("  STRUDELFORCE 0.3.1  --  COMMODORE AMIGA 500 FOREVER!")
        credits_lines.append("==========================================================================")
        credits_lines.append("")

    out_path = os.path.join(ROOT, 'src', 'embedded_credits.h')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("/* embedded_credits.h - Automatisch generiert aus Doku & Spezifikation */\n")
        f.write("#ifndef EMBEDDED_CREDITS_H\n#define EMBEDDED_CREDITS_H\n\n")
        f.write(f"#define EMBEDDED_CREDITS_COUNT {len(credits_lines)}\n")
        f.write(f"#define CREDITS_PHASE0_LINE {phase0_line}\n")
        f.write(f"#define CREDITS_PHASE1_LINE {phase1_line}\n")
        f.write(f"#define CREDITS_PHASE2_LINE {phase2_line}\n")
        f.write(f"#define CREDITS_PHASE3_LINE {phase3_line}\n")
        f.write(f"#define CREDITS_PHASE4_LINE {phase4_line}\n")
        f.write(f"#define CREDITS_CAPTAINS_LOG_LINE {captains_log_line}\n\n")
        f.write("static const char * const EMBEDDED_CREDITS_DATA[EMBEDDED_CREDITS_COUNT] = {\n")
        for line in credits_lines:
            f.write(f'    "{escape_c_string(line)}",\n')
        f.write("};\n\n#endif /* EMBEDDED_CREDITS_H */\n")

    print(f"Generated {out_path}: {len(credits_lines)} lines (Captain's Log starts at line {captains_log_line}).")

if __name__ == '__main__':
    main()
