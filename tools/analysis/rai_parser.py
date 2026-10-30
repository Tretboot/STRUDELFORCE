#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""rai_parser.py - RA-I Container-Parser (Rainbow Arts / Factor 5)

Format:
  +0x00: Magic "RA-I"
  +0x04: u16 Version
  +0x06: u16 Anzahl Einträge (Big Endian)
  +0x08: Tabelle, 16 Byte pro Eintrag:
         u16 A (Index?)
         u16 B (Typ?)
         u32 Offset (relativ zum RA-I-Start)
         u32 Size
         char Name[4] (Big Endian, Null-terminiert)

Verwendung:
  python rai_parser.py <Image> <RA-I-Offset> [Ausgabe-Ordner]
"""

import struct
import sys
import os

MAGIC_RAI = b'RA-I'


def parse_rai(data, rai_offset):
    """Parse RA-I Container ab Offset rai_offset."""
    if data[rai_offset:rai_offset + 4] != MAGIC_RAI:
        print(f'FEHLER: Kein RA-I-Magic bei Offset {rai_offset}: {data[rai_offset:rai_offset+4]!r}')
        return []

    version = struct.unpack('>H', data[rai_offset + 4:rai_offset + 6])[0]
    count = struct.unpack('>H', data[rai_offset + 6:rai_offset + 8])[0]
    print(f'RA-I Version: {version}, Einträge: {count}')

    entries = []
    table_start = rai_offset + 8
    for i in range(count):
        off = table_start + i * 16
        if off + 16 > len(data):
            print(f'FEHLER: Tabelle endet vor Eintrag {i}')
            break
        entry = data[off:off + 16]
        a = struct.unpack('>H', entry[0:2])[0]
        b = struct.unpack('>H', entry[2:4])[0]
        offset = struct.unpack('>I', entry[4:8])[0]
        size = struct.unpack('>I', entry[8:12])[0]
        name_bytes = entry[12:16]
        name = name_bytes.decode('latin1').rstrip('\x00')
        entries.append({
            'index': i,
            'name': name,
            'a': a,
            'b': b,
            'offset': offset,
            'size': size
        })
    return entries


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1

    image_path = sys.argv[1]
    rai_offset = int(sys.argv[2], 0) if len(sys.argv) > 2 else 1722
    outdir = sys.argv[3] if len(sys.argv) > 3 else 'tmp/t2_re/rai_output'

    data = open(image_path, 'rb').read()
    print(f'Image {image_path}: {len(data)} Bytes')

    entries = parse_rai(data, rai_offset)
    print(f'\nGefundene Einträge: {len(entries)}\n')

    print(f'{"Nr":>3}  {"Name":<8} {"A":>4} {"B":>4} {"Offset":>10} {"Size":>10}')
    for e in entries:
        print(f'{e["index"]:>3}  {e["name"]:<8} {e["a"]:>4} {e["b"]:>4} {e["offset"]:>10} {e["size"]:>10}')

    os.makedirs(outdir, exist_ok=True)
    print(f'\nExtrahiere Dateien nach {outdir}...')
    for e in entries:
        abs_offset = rai_offset + e['offset']
        abs_size = e['size']
        if abs_offset + abs_size > len(data):
            print(f'FEHLER: Eintrag {e["name"]} außerhalb des Images')
            continue
        content = data[abs_offset:abs_offset + abs_size]
        out_path = os.path.join(outdir, f'{e["name"]}.bin')
        with open(out_path, 'wb') as f:
            f.write(content)
        print(f'  {e["name"]}.bin ({abs_size} Bytes)')

    return 0


if __name__ == '__main__':
    sys.exit(main())