"""ppm2bmp - mini PPM(P6)->BMP Konverter fuer GUI-Frame-Dumps (Testzweck).
Nutzung: python tools/ppm2bmp.py <vd_xxx.ppm> [<vd_yyy.ppm> ...]"""
import re, struct, sys

def convert(src):
    d = open(src, 'rb').read()
    m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    if not m:
        print("kein P6:", src); return
    W, H, MX = int(m.group(1)), int(m.group(2)), int(m.group(3))
    pos = m.end()
    raw = d[pos:pos + W * H * 3]
    if len(raw) < W * H * 3:
        print("RAW zu kurz:", src, len(raw)); return
    row = W * 3
    pad = (4 - row % 4) % 4
    rowsz = row + pad
    size = 54 + rowsz * H
    out = bytearray()
    out += b'BM' + struct.pack('<IHHI', size, 0, 0, 54)
    out += struct.pack('<IiiHHIIiiII', 40, W, H, 1, 24, 0, rowsz * H, 2835, 2835, 0, 0)
    for y in range(H - 1, -1, -1):
        out += raw[y * row:(y + 1) * row] + b'\0' * pad
    dst = src[:-4] + '.bmp'
    open(dst, 'wb').write(bytes(out))
    print(dst, "%dx%d" % (W, H), len(out))

for f in sys.argv[1:]:
    convert(f)