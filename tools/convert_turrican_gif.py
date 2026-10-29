# tools/convert_turrican_gif.py
import os

def decompress_lzw(lzw_min, stream):
    clear_code = 1 << lzw_min
    end_code = clear_code + 1
    code_size = lzw_min + 1
    code_mask = (1 << code_size) - 1
    table = [[i] for i in range(clear_code)] + [[]] + [[]]
    bits = 0; nbits = 0; pos = 0
    def get_code():
        nonlocal bits, nbits, pos
        while nbits < code_size and pos < len(stream):
            bits |= stream[pos] << nbits
            pos += 1; nbits += 8
        code = bits & code_mask
        bits >>= code_size; nbits -= code_size
        return code
    out = []
    old_code = -1
    while True:
        code = get_code()
        if code == clear_code:
            code_size = lzw_min + 1
            code_mask = (1 << code_size) - 1
            table = [[i] for i in range(clear_code)] + [[]] + [[]]
            old_code = -1
            continue
        elif code == end_code:
            break
        if old_code == -1:
            out.extend(table[code]); old_code = code
        else:
            if code < len(table): entry = table[code]
            else: entry = table[old_code] + [table[old_code][0]]
            out.extend(entry)
            table.append(table[old_code] + [entry[0]])
            if len(table) == (1 << code_size) and code_size < 12:
                code_size += 1
                code_mask = (1 << code_size) - 1
            old_code = code
    return out

with open('ref/misc/Turrican.gif', 'rb') as f:
    data = f.read()

W = data[6] | (data[7] << 8)
H = data[8] | (data[9] << 8)
pos = 13
palette_rgb = []
if data[10] & 0x80:
    for i in range(16):
        r = data[pos + i*3]
        g = data[pos + i*3 + 1]
        b = data[pos + i*3 + 2]
        palette_rgb.append((r << 16) | (g << 8) | b)
    pos += 16 * 3

frames = []
trans_idx = 0

while pos < len(data):
    b = data[pos]
    if b == 0x21:
        ext_type = data[pos+1]
        pos += 2
        block_len = data[pos]
        if ext_type == 0xF9:
            ctrl = data[pos+1:pos+1+block_len]
            if ctrl[0] & 1: trans_idx = ctrl[3]
        while block_len > 0:
            pos += 1 + block_len
            block_len = data[pos]
        pos += 1
    elif b == 0x2C:
        left = data[pos+1] | (data[pos+2] << 8)
        top = data[pos+3] | (data[pos+4] << 8)
        w = data[pos+5] | (data[pos+6] << 8)
        h = data[pos+7] | (data[pos+8] << 8)
        img_flags = data[pos+9]
        pos += 10
        if img_flags & 0x80:
            pos += 3 * (2 ** ((img_flags & 7) + 1))
        lzw_min = data[pos]
        pos += 1
        blocks = []
        block_len = data[pos]
        while block_len > 0:
            blocks.append(data[pos+1:pos+1+block_len])
            pos += 1 + block_len
            block_len = data[pos]
        pos += 1
        stream = b''.join(blocks)
        pixels = decompress_lzw(lzw_min, stream)
        
        frame = [0] * (W * H)
        for y in range(h):
            for x in range(w):
                c = pixels[y * w + x]
                if c != trans_idx:
                    frame[(top + y) * W + (left + x)] = c
        frames.append(frame)
    elif b == 0x3B:
        break
    else:
        break

print(f'Decoded {len(frames)} frames of size {W}x{H}')

# Crop to standardized 64x74 box: x in [12..75], y in [6..79]
CROP_W = 64
CROP_H = 74
BOX_X0 = 12
BOX_Y0 = 6

header_lines = [
    "/* turrican_sprites.h - Original Amiga Turrican Bren McGuire Sprites */",
    "/* Generiert aus Turrican.gif (16 Farben, 14 Frames @ 64x74) */",
    "#ifndef TURRICAN_SPRITES_H",
    "#define TURRICAN_SPRITES_H",
    "",
    "#include <stdint.h>",
    "",
    f"#define TURRICAN_FRAME_W {CROP_W}",
    f"#define TURRICAN_FRAME_H {CROP_H}",
    f"#define TURRICAN_FRAME_COUNT {len(frames)}",
    "",
    "/* 16-Farben Amiga-Palette */",
    "static const uint32_t TURRICAN_PALETTE[16] = {"
]

for i, c in enumerate(palette_rgb):
    header_lines.append(f"    0x{c:06X}u, /* {i} */")
header_lines.append("};")
header_lines.append("")

# Pack pixels into 4-bit nibbles (2 pixels per byte): 64*74/2 = 2368 bytes per frame
header_lines.append("/* 4-Bit gehaeckselte Sprite-Daten (0 = Transparent) */")
header_lines.append(f"static const uint8_t TURRICAN_FRAMES[{len(frames)}][{CROP_W * CROP_H // 2}] = {{")

for fi, f in enumerate(frames):
    header_lines.append(f"    /* Frame {fi} */")
    header_lines.append("    {")
    row_bytes = []
    for y in range(CROP_H):
        byte_list = []
        for x in range(0, CROP_W, 2):
            p0 = f[(BOX_Y0 + y) * W + (BOX_X0 + x)] & 0x0F
            p1 = f[(BOX_Y0 + y) * W + (BOX_X0 + x + 1)] & 0x0F
            byte_list.append(f"0x{(p0 << 4) | p1:02X}")
        header_lines.append("        " + ", ".join(byte_list) + ",")
    header_lines.append("    },")

header_lines.append("};")
header_lines.append("")
header_lines.append("#endif /* TURRICAN_SPRITES_H */")

with open('src/turrican_sprites.h', 'w') as f:
    f.write("\n".join(header_lines) + "\n")

print(f"SUCCESS: src/turrican_sprites.h created ({os.path.getsize('src/turrican_sprites.h')} bytes)!")

