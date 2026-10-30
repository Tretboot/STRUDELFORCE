#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
t2_slave_parse.py - Zerlegt die WHDLoad .Slave-Datei von Turrican 2 (v1.5):
  - Header (WHDLOADS)
  - Patch-Tabelle
  - Load-Liste (welche Disk-Offsets wo nach RAM geladen werden)
  - Config-/Sound-Blöcke

Verwendung: python t2_slave_parse.py <Turrican2.Slave>
"""
import struct
import sys


def main():
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    path = sys.argv[1] if len(sys.argv) > 1 else 'Turrican2.Slave'
    d = open(path, 'rb').read()
    print('Slave-Datei: %d Bytes' % len(d))

    # --- Header (64 Byte) ---
    print()
    print('=== HEADER ===')
    print('Signature : %r' % d[0:8].decode('latin1'))
    print('Version   : %d' % struct.unpack('>H', d[8:10])[0])
    print('Date      : %04d-%02d-%02d' % (struct.unpack('>H', d[12:14])[0] - 2048,
                                          struct.unpack('>B', d[14:15])[0],
                                          struct.unpack('>B', d[15:16])[0]))
    print('Min Ver   : %d  (0x%02X)' % (d[16], d[16]))
    print('Info      : %r' % d[17:49].split(b'\x00')[0].decode('latin1'))
    print('Flags     : 0x%02X' % d[50])
    print('Crc       : 0x%04X' % struct.unpack('>H', d[54:56])[0])
    # Rest-Header-Strings
    for off in (58,):
        seg = d[off:off + 98].split(b'\x00')[0].decode('latin1', 'replace')
        if seg.strip():
            print('Str@%d  : %r' % (off, seg))

    # --- Patch-Tabelle: startet nach 64-Byte-Header? (whd: 0x40) ---
    # Format: u32 Adresse, u16 Länge, u8 Size, u8 Code (10 Byte pro Patch)
    print()
    print('=== PATCH-TABELLE ===')
    off = 0x40
    patches = 0
    while off + 10 <= len(d):
        addr = struct.unpack('>I', d[off:off + 4])[0]
        psize = struct.unpack('>H', d[off + 4:off + 6])[0]
        size = d[off + 6]
        code = d[off + 7]
        if psize == 0 or addr == 0:
            break
        if addr > 0x400000 or psize > 0x1000:
            break
        data = d[off + 8:off + 8 + size]
        print('  0x%06X  size=%-4d code=%-3d data=%s' % (addr, size, code, data[:24].hex()))
        off += 10 + size
        patches += 1
        if patches > 200:
            print('  ... (Abbruch, >200)')
            break
    print('Patches: %d, Tabelle endet bei 0x%04X' % (patches, off))

    # --- Load-Liste: u32 Offset, u32 Adresse, u8 Flags, u8 Size ---
    print()
    print('=== LOAD-LISTE ===')
    entries = []
    i = 0
    while off + 10 <= len(d) and i < 500:
        src_off = struct.unpack('>I', d[off:off + 4])[0]
        dst_addr = struct.unpack('>I', d[off + 4:off + 8])[0]
        flags = d[off + 8]
        size = d[off + 9]
        if src_off == 0 and dst_addr == 0:
            off += 10
            continue
        if dst_addr > 0x400000 or src_off > 0x200000:
            break
        length = {0: 4, 1: 2, 2: 1, 3: 4}[size & 3]
        entries.append((src_off, dst_addr, flags, size & 3, length))
        print('  #%-3d src=0x%07X -> dst=0x%06X flags=%d len=%d' % (i, src_off, dst_addr, flags, length))
        off += 10
        i += 1
    if entries:
        print('Load-Eintraege: %d, Liste endet bei 0x%04X' % (i, off))


if __name__ == '__main__':
    main()
