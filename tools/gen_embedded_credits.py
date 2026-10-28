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
    essay_path = os.path.join(ROOT, 'WORKFLOW_ESSAY.md')
    log_path = os.path.join(ROOT, 'captains_log.md')

    essay_lines = read_and_wrap(essay_path)
    log_lines = read_and_wrap(log_path)

    out_path = os.path.join(ROOT, 'src', 'embedded_credits.h')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("/* embedded_credits.h - GENERIERT von tools/gen_embedded_credits.py */\n")
        f.write("/* Enthaelt das vollstaendige WORKFLOW_ESSAY & CAPTAINS_LOG direkt im Binary */\n")
        f.write("#ifndef EMBEDDED_CREDITS_H\n")
        f.write("#define EMBEDDED_CREDITS_H\n\n")

        # Workflow Essay
        f.write(f"static const int EMBEDDED_WORKFLOW_ESSAY_COUNT = {len(essay_lines)};\n")
        f.write("static const char * const EMBEDDED_WORKFLOW_ESSAY[] = {\n")
        for l in essay_lines:
            f.write(f'    "{escape_c_string(l)}",\n')
        f.write("};\n\n")

        # Captain's Log
        f.write(f"static const int EMBEDDED_CAPTAINS_LOG_COUNT = {len(log_lines)};\n")
        f.write("static const char * const EMBEDDED_CAPTAINS_LOG[] = {\n")
        for l in log_lines:
            f.write(f'    "{escape_c_string(l)}",\n')
        f.write("};\n\n")

        f.write("#endif /* EMBEDDED_CREDITS_H */\n")

    print(f"SUCCESS: src/embedded_credits.h generated: {len(essay_lines)} essay lines, {len(log_lines)} log lines!")

if __name__ == '__main__':
    main()

