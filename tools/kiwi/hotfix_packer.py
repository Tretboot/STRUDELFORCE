# tools/hotfix_packer.py
import os
import zlib
import base64
import sys

# Trage hier die Dateien ein, die der Cloud fehlen
FILES_TO_PACK = [
    "src/strudelforce.h",
    "tests/data/Turrican 2 - World 1 - Dessert.tfx",
    "tests/data/Turrican 2 - World 1 - Dessert.sam"
]

def build_hotfix():
    print("Erzeuge KIWI Hotfix Injector...")
    payload = {}
    
    for filepath in FILES_TO_PACK:
        if os.path.exists(filepath):
            with open(filepath, "rb") as f:
                raw_data = f.read()
                compressed = zlib.compress(raw_data, level=9)
                b64_str = base64.b64encode(compressed).decode('utf-8')
                payload[filepath] = b64_str
                print(f" Gepackt: {filepath} ({len(raw_data)} bytes -> {len(b64_str)} bytes b64)")
        else:
            print(f" WARNUNG: Datei nicht lokal gefunden: {filepath}")

    script_content = f"""import os, zlib, base64

PAYLOAD = {payload}

for path, b64_data in PAYLOAD.items():
    out_dir = os.path.dirname(path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    raw_data = zlib.decompress(base64.b64decode(b64_data))
    with open(path, "wb") as f:
        f.write(raw_data)
    print(f"Entpackt und erzwungen: {{path}}")
print("Hotfix erfolgreich angewendet!")
"""

    with open("kiwi_hotfix.py", "w", encoding="utf-8") as f:
        f.write(script_content)
    
    final_sz = os.path.getsize("kiwi_hotfix.py")
    print(f"\n[OK] kiwi_hotfix.py wurde erstellt ({final_sz} Bytes / {final_sz/1024:.1f} KB)! Lade diese Datei in die Cloud hoch und fuehre sie aus.")

if __name__ == "__main__":
    build_hotfix()
