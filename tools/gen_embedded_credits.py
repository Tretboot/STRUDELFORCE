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
    lore_path = os.path.join(ROOT, 'docs', 'LORE.md')
    music_path = os.path.join(ROOT, 'docs', 'MUSIC_SYSTEM.md')
    essay_path = os.path.join(ROOT, 'WORKFLOW_ESSAY.md')
    log_path = os.path.join(ROOT, 'captains_log.md')

    lore_lines = read_and_wrap(lore_path)
    music_lines = read_and_wrap(music_path)
    essay_lines = read_and_wrap(essay_path)
    log_lines = read_and_wrap(log_path)

    # Decorative section dividers with spacing
    div_1 = [
        "",
        "==========================================================================",
        "  CHAPTER I: LORE & TURRICAN WARRIOR HERITAGE",
        "==========================================================================",
        ""
    ]
    div_2 = [
        "",
        "",
        "==========================================================================",
        "  CHAPTER II: TECHNICAL ARCHITECTURE & CYBER-PAULA SPECIFICATION",
        "==========================================================================",
        "",
        "  Cleanroom TFMX Replay Engine (C89/C99 Dual Target)",
        "  Paula 8-Voice DMA Mixer | 16.16 Fixed-Point Math | Zero-Float PAL 50Hz",
        "  Hardware Registers: AUD0LCH/LEN/PER/VOL ... CIA-A $BFE001 Lowpass",
        ""
    ]
    div_gap = [
        "",
        "",
        "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
        "  [ TRANSITION GAP: SOFTWARE MANAGER SUBSONG 0 -> SUBSONG 21 ]",
        "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
        "",
        ""
    ]
    div_3 = [
        "",
        "==========================================================================",
        "  CHAPTER III: WORKFLOW ESSAYS & AGENTIC ARCHITECTURAL HYPOTHESES",
        "==========================================================================",
        ""
    ]
    div_4 = [
        "",
        "",
        "==========================================================================",
        "  CHAPTER IV: CAPTAIN'S LOG - FORSCHUNGSTAGEBUCH DER SANDBOX",
        "==========================================================================",
        ""
    ]
    div_end = [
        "",
        "",
        "==========================================================================",
        "  CHAPTER V: BUG REPORTS, OPEN SOURCE ACKNOWLEDGEMENTS & CERTIFICATION",
        "==========================================================================",
        "",
        "  Alle Implementierungen wurden im Cleanroom-Verfahren entwickelt.",
        "  Sollten dennoch Inkompatibilitaeten auftreten, bitte einen Bugreport",
        "  im STRUDELFORCE Repository erstellen. Keep the Amiga Spirit alive!",
        "",
        "  *** END OF TRANSMISSION - VICTORY FANFARE ***"
    ]

    all_credits = []
    all_credits.extend(div_1)
    all_credits.extend(lore_lines)
    all_credits.extend(div_2)
    all_credits.extend(music_lines)
    all_credits.extend(div_gap)
    gap_line_index = len(all_credits)
    all_credits.extend(div_3)
    all_credits.extend(essay_lines)
    all_credits.extend(div_4)
    all_credits.extend(log_lines)
    all_credits.extend(div_end)

    out_path = os.path.join(ROOT, 'src', 'embedded_credits.h')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("/* embedded_credits.h - GENERIERT von tools/gen_embedded_credits.py */\n")
        f.write("/* Enthaelt das vollstaendige Dokumentations- & Code-Archiv direkt im Binary */\n")
        f.write("#ifndef EMBEDDED_CREDITS_H\n")
        f.write("#define EMBEDDED_CREDITS_H\n\n")

        f.write(f"static const int EMBEDDED_CREDITS_GAP_LINE = {gap_line_index};\n")
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

    print(f"SUCCESS: src/embedded_credits.h generated: {len(all_credits)} total lines (Gap at line {gap_line_index})!")

if __name__ == '__main__':
    main()
