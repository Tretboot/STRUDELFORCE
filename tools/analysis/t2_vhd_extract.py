#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""t2_vhd_extract.py - WinUAE VHD (UAE--ADF) -> Rohes 512-Byte Amiga-Disk-Image

Layout:
  Header (128 Bytes) = Magic "UAE--ADF", Größe, DataOffset (bis DOS-Bootblock)
  Padding (bis 648) (mit Nullen aufgefüllt?)
  DOS-Bootblock @ 648 (512 Bytes)
  weiter: rohes Amiga-DOS-FFS-Image

Verwendung:
  python t2_vhd_extract.py VHD.OUT
"""
import struct
import sys

VHD_MAGIC = b"UAE--ADF"
VHD_SECTOR = 512
VHD_HEADER = 128
VHD_BOOT_BLOCK_OFF = 648

def extract(path, out):
    data = open(path, "rb").read()
    print(f"VHD {path}: {len(data)} Bytes")
    if data[:8] != VHD_MAGIC:
        print(f"FEHLER: Keine VHD-Magic (gefundend: {data[:8]!r})")
        return 1
    # Optionaler Header mit DataOffset?
    # Lesen von DataOffset bei 32? Typisch: 0x18 Offset = 24?
    off = struct.unpack(">I", data[24:28])[0] if len(data) >= 28 else VHD_BOOT_BLOCK_OFF
    if off < 0 or off >= len(data):
        off = VHD_BOOT_BLOCK_OFF
    print(f"Datenoffset: {off} ({hex(off)})")
    raw = data[off:]
    # Auf Vielfache von VHD_SECTOR zuschneiden
    raw = raw[:len(raw) // VHD_SECTOR * VHD_SECTOR]
    open(out, "wb").write(raw)
    print(f"Rohes Image -> {out} ({len(raw)} Bytes, {len(raw)//VHD_SECTOR} Sektoren)")
    return 0

def main():
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    if len(sys.argv) < 3:
        print(__doc__)
        return
    return extract(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
    sys.exit(main())