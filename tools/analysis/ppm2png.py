#!/usr/bin/env python3
"""ppm2png.py — Pure-Python PPM (P6) to PNG Converter (ohne externe Dependencies)."""
import os
import sys
import zlib
import struct

def ppm_to_png(ppm_path: str, png_path: str = None) -> str:
    if png_path is None:
        png_path = os.path.splitext(ppm_path)[0] + ".png"
    with open(ppm_path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P6":
            raise ValueError(f"Ungueltiges PPM-Format: {magic}")
        line = f.readline().strip()
        while line.startswith(b"#"):
            line = f.readline().strip()
        parts = line.split()
        w, h = int(parts[0]), int(parts[1])
        f.readline()  # maxval
        data = f.read()

    stride = w * 3
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter type 0
        raw.extend(data[y * stride : (y + 1) * stride])

    def chunk(tag: bytes, content: bytes) -> bytes:
        c = struct.pack(">I", len(content)) + tag + content
        crc = zlib.crc32(tag + content) & 0xFFFFFFFF
        return c + struct.pack(">I", crc)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")

    with open(png_path, "wb") as f:
        f.write(png)
    return png_path

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: ppm2png.py <file.ppm> [out.png]")
        sys.exit(1)
    out = ppm_to_png(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
    print(f"Erfolgreich konvertiert: {out}")
