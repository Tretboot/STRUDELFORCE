# tools/gen_embedded_credits.py
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def read_and_wrap(filepath, max_len=74):
    if not os.path.isfile(filepath):
        return []
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()

    # Transcribe German umlauts and special punctuation
    replacements = {
        'ä': 'ae', 'ö': 'oe', 'ü': 'ue',
        'Ä': 'Ae', 'Ö': 'Oe', 'Ü': 'Ue',
        'ß': 'ss', '—': '--', '–': '-',
        '“': '"', '”': '"', '‘': "'", '’': "'",
        '→': '->', '←': '<-', '…': '...'
    }
    for k, v in replacements.items():
        text = text.replace(k, v)

    raw_lines = text.splitlines()
    wrapped = []
    for line in raw_lines:
        line = line.strip()
        if not line:
            wrapped.append("")
            continue
        while len(line) > max_len:
            split = max_len
            while split > 30 and line[split] != ' ':
                split -= 1
            if split <= 30:
                split = max_len
            wrapped.append(line[:split].strip())
            line = line[split:].strip()
        if line:
            wrapped.append(line)
    return wrapped

def escape_c_string(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')

def main():
    music_path = os.path.join(ROOT, 'docs', 'MUSIC_SYSTEM.md')
    essay_path = os.path.join(ROOT, 'WORKFLOW_ESSAY.md')
    log_path = os.path.join(ROOT, 'captains_log.md')

    music_lines = read_and_wrap(music_path)
    essay_lines = read_and_wrap(essay_path)
    log_lines = read_and_wrap(log_path)

    # Decorative section dividers with spacing
    div_header = [
        "==========================================================================",
        "  S T R U D E L F O R C E   0 . 3 . 0   --   A B S P A N N   &   C R E D I T S",
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
        ""
    ]

    div_phase1 = [
        "",
        "",
        "==========================================================================",
        "  KAPITEL I: SOUND-ARCHITEKTUR & CYBER-PAULA TFMX SPEZIFIKATION",
        "==========================================================================",
        "",
        "  Cleanroom TFMX Replay Engine (C89/C99 Dual Target)",
        "  Paula 8-Voice DMA Mixer | 16.16 Fixed-Point Math | Zero-Float PAL 50Hz",
        "  Hardware Registers: AUD0LCH/LEN/PER/VOL ... CIA-A $BFE001 Lowpass",
        ""
    ]

    div_phase2 = [
        "",
        "",
        "==========================================================================",
        "  KAPITEL II: WORKFLOW ESSAY - ARCHITEKTUR-HYPOTHESEN & DEMOSZENE-METHODIK",
        "==========================================================================",
        ""
    ]

    div_phase3 = [
        "",
        "",
        "==========================================================================",
        "  KAPITEL III: CAPTAIN'S LOG - TAGEBUCH DER FORSCHUNG & ENTWICKLUNG",
        "==========================================================================",
        ""
    ]

    div_phase4 = [
        "",
        "",
        "==========================================================================",
        "  KAPITEL IV: ZERTIFIZIERUNG, BUG-REPORTS & AMIGA-HERITAGE",
        "==========================================================================",
        "",
        "  Alle Implementierungen wurden im Cleanroom-Verfahren entwickelt.",
        "  Sollten dennoch Inkompatibilitaeten auftreten, bitte einen Bugreport",
        "  im STRUDELFORCE Repository erstellen. Keep the Amiga Spirit alive!",
        "",
        "  *** END OF TRANSMISSION - TURRICAN 2 LEVEL COMPLETE FANFARE ***"
    ]

    phase0_line = 0
    all_credits = []
    all_credits.extend(div_header)

    phase1_line = len(all_credits)
    all_credits.extend(div_phase1)
    all_credits.extend(music_lines)

    phase2_line = len(all_credits)
    all_credits.extend(div_phase2)
    all_credits.extend(essay_lines)

    phase3_line = len(all_credits)
    all_credits.extend(div_phase3)
    all_credits.extend(log_lines)

    phase4_line = len(all_credits)
    all_credits.extend(div_phase4)

    out_path = os.path.join(ROOT, 'src', 'embedded_credits.h')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("/* embedded_credits.h - GENERIERT von tools/gen_embedded_credits.py */\n")
        f.write("/* Enthaelt das vollstaendige Dokumentations- & Code-Archiv direkt im Binary */\n")
        f.write("#ifndef EMBEDDED_CREDITS_H\n")
        f.write("#define EMBEDDED_CREDITS_H\n\n")

        f.write(f"static const int EMBEDDED_CREDITS_PHASE_0_LINE = {phase0_line};\n")
        f.write(f"static const int EMBEDDED_CREDITS_PHASE_1_LINE = {phase1_line};\n")
        f.write(f"static const int EMBEDDED_CREDITS_PHASE_2_LINE = {phase2_line};\n")
        f.write(f"static const int EMBEDDED_CREDITS_PHASE_3_LINE = {phase3_line};\n")
        f.write(f"static const int EMBEDDED_CREDITS_PHASE_4_LINE = {phase4_line};\n")
        f.write(f"static const int EMBEDDED_CREDITS_GAP_LINE = {phase2_line};\n")
        f.write(f"static const int EMBEDDED_CREDITS_TOTAL_COUNT = {len(all_credits)};\n")
        f.write("static const char * const EMBEDDED_CREDITS_ALL[] = {\n")
        for l in all_credits:
            f.write(f'    "{escape_c_string(l)}",\n')
        f.write("};\n\n")

        # Keep legacy symbols for backward compatibility with test_suite / gui
        f.write(f"static const int EMBEDDED_WORKFLOW_ESSAY_COUNT = {len(essay_lines)};\n")
        f.write("static const char * const EMBEDDED_WORKFLOW_ESSAY[] = {\n")
        for l in essay_lines:
            f.write(f'    "{escape_c_string(l)}",\n')
        f.write("};\n\n")

        f.write(f"static const int EMBEDDED_CAPTAINS_LOG_COUNT = {len(log_lines)};\n")
        f.write("static const char * const EMBEDDED_CAPTAINS_LOG[] = {\n")
        for l in log_lines:
            f.write(f'    "{escape_c_string(l)}",\n')
        f.write("};\n\n")

        f.write("#endif /* EMBEDDED_CREDITS_H */\n")

    print(f"SUCCESS: src/embedded_credits.h generated: {len(all_credits)} lines (Phases: 0={phase0_line}, 1={phase1_line}, 2={phase2_line}, 3={phase3_line}, 4={phase4_line})")

if __name__ == '__main__':
    main()
