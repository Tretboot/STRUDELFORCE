#!/usr/bin/env python3
"""
STRUDELFORCE MASTER - GitHub Exporter / Packager
"""

import os
import zipfile
import shutil

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Zu packende Verzeichnisse und Dateien
INCLUDE_DIRS = [
    "src",
    "assets",
    "tools",
    "levels",
    "docs"
]

INCLUDE_FILES = [
    "PROJECT_BIBLE.md",
    "WORKFLOW_ESSAY.md",
    "KIWI_SURVIVAL_GUIDE.md",
    "README.md",
    "Makefile",
    "build_msvc.bat",
    "LICENSE",
    "AGENTS.md",
    "MERGE_NOTES.md"
]

# Zu ignorierende Muster
IGNORE_PATTERNS = [
    ".exe", ".obj", ".o", ".a", ".lib", ".pyc",
    "__pycache__", ".git", ".gitignore", ".swp",
    "tmp_obj", "--help", "nul"
]

def should_ignore(path):
    for pattern in IGNORE_PATTERNS:
        if pattern in path:
            return True
    return False

def create_export_zip(output_path):
    print("=" * 60)
    print("STRUDELFORCE MASTER - Exporter")
    print("=" * 60)
    
    total_files = 0
    total_size = 0
    
    with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for dir_name in INCLUDE_DIRS:
            dir_path = os.path.join(ROOT_DIR, dir_name)
            if not os.path.exists(dir_path):
                continue
            for root, dirs, files in os.walk(dir_path):
                dirs[:] = [d for d in dirs if not should_ignore(d)]
                for f in files:
                    if should_ignore(f):
                        continue
                    fp = os.path.join(root, f)
                    arcname = os.path.relpath(fp, ROOT_DIR).replace('\\', '/')
                    zipf.write(fp, arcname)
                    size = os.path.getsize(fp)
                    total_files += 1
                    total_size += size
        
        for file_name in INCLUDE_FILES:
            fp = os.path.join(ROOT_DIR, file_name)
            if os.path.exists(fp) and not should_ignore(file_name):
                zipf.write(fp, file_name)
                size = os.path.getsize(fp)
                total_files += 1
                total_size += size
    
    print(f"\n[OK] Export abgeschlossen: {output_path}")
    print(f"     Dateien: {total_files}, Groesse: {total_size:,} Bytes ({total_size/1024:.2f} KB)")
    print("=" * 60)
    return total_files, total_size

if __name__ == "__main__":
    out = os.path.join(ROOT_DIR, "strudelforce_export.zip")
    create_export_zip(out)
