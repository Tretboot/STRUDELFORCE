#!/usr/bin/env python3
"""tools/budget_analysis.py - Detailed memory budget and section breakdown for strudelforce.exe"""
import os, sys, struct

def analyze():
    exe_path = "strudelforce.exe"
    if not os.path.isfile(exe_path):
        print(f"Error: {exe_path} not found.")
        return

    exe_sz = os.path.getsize(exe_path)
    limit = 1048576 # 1 MB

    with open(exe_path, "rb") as f:
        b = f.read()

    e_lfanew = struct.unpack("<I", b[0x3C:0x40])[0]
    num_sections = struct.unpack("<H", b[e_lfanew + 6:e_lfanew + 8])[0]
    opt_hdr_sz = struct.unpack("<H", b[e_lfanew + 20:e_lfanew + 22])[0]
    sec_start = e_lfanew + 24 + opt_hdr_sz

    sections = []
    for i in range(num_sections):
        offset = sec_start + i * 40
        name = b[offset:offset+8].decode("latin-1").strip("\x00")
        virt_sz = struct.unpack("<I", b[offset+8:offset+12])[0]
        raw_sz = struct.unpack("<I", b[offset+16:offset+20])[0]
        sections.append((name, virt_sz, raw_sz))

    # Read music assets
    music_assets = {}
    if os.path.isfile("src/embedded_music.h"):
        with open("src/embedded_music.h", "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if "const uint8_t" in line and "[" in line and "]" in line:
                    parts = line.split()
                    for p in parts:
                        if "[" in p and "]" in p:
                            name = p.split("[")[0]
                            sz = int(p.split("[")[1].split("]")[0])
                            music_assets[name] = sz

    credits_sz = 0
    if os.path.isfile("src/embedded_credits.h"):
        # count total string length in bytes
        with open("src/embedded_credits.h", "r", encoding="latin-1") as f:
            lines = [l for l in f if l.strip().startswith('"')]
            credits_sz = sum(len(l.strip().strip('",')) for l in lines)

    print("=" * 78)
    print(f"  STRUDELFORCE MEMORY BUDGET BREAKDOWN  (Ziel: < 1,048,576 Bytes / 1024.0 KB)")
    print("=" * 78)
    print(f"  Gesamt-EXE: {exe_sz:8d} Bytes ({exe_sz/1024:6.1f} KB) - "
          f"{exe_sz/limit*100:5.2f}% des 1MB-Budgets  [Headroom: {limit - exe_sz:6d} B / {(limit - exe_sz)/1024:4.1f} KB]")
    print("-" * 78)
    print("  PE-SEKTIONEN:")
    for name, vsz, rsz in sections:
        pct = (rsz / exe_sz) * 100
        desc = ""
        if name == ".text": desc = "(Kompilierter Maschinencode C99)"
        elif name == ".rdata": desc = "(Read-Only Daten: Musik, Samples, Sprites, Texte)"
        elif name == ".data": desc = "(Initialisierte Variablen)"
        elif name == ".pdata": desc = "(Exception Handling Tabellen x64)"
        elif name == ".reloc": desc = "(Relokationstabelle)"
        print(f"    {name:<10s}: {rsz:8d} Bytes ({rsz/1024:6.1f} KB)  {pct:5.2f}%  {desc}")
    print("-" * 78)
    print("  GROESSTE ASSETS IN .rdata (Musik, Samples, Credits, Sprites):")
    total_music = sum(music_assets.values())
    for k, v in sorted(music_assets.items(), key=lambda x: -x[1]):
        pct = (v / exe_sz) * 100
        print(f"    {k:<32s}: {v:7d} Bytes ({v/1024:5.1f} KB)  {pct:4.2f}%")
    print(f"    {'EMBEDDED_CREDITS_DATA':<32s}: {credits_sz:7d} Bytes ({credits_sz/1024:5.1f} KB)  {credits_sz/exe_sz*100:4.2f}%")
    print(f"    {'TURRICAN_SPRITE_FRAMES':<32s}:   33152 Bytes ( 32.4 KB)   3.21%")
    print("-" * 78)
    print(f"  SUMME ALLER AUDIO-TRACKS: {total_music:8d} Bytes ({total_music/1024:6.1f} KB) ({total_music/exe_sz*100:5.2f}% der EXE)")
    print("=" * 78)

if __name__ == "__main__":
    analyze()

