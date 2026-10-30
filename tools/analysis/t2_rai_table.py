#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
t2_rai_table.py - Zerlegt die 'RA-I' (Rainbow Image) Dateitabelle von
Turrican 2 (WHDLoad v1.5, Disk.1).

RA-I = Dateicontainer-Format von Rainbow Arts / Factor 5.
Layout (Big-Endian):
    +0x00: "RA-I" (4 Byte Magic)
    +0x04: u16 Version  (2)
    +0x06: u16 Eintraege (480)
    +0x08: Tabelle, 16 Byte pro Eintrag:
        u16 A, u16 B, u32 Offset, u32 Size, char Name[4]

Verwendung:
    python t2_rai_table.py <Disk.1> [filter]
"""
import struct
import sys


def main():
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    path = sys.argv[1] if len(sys.argv) > 1 else 'Disk.1'
    flt = sys.argv[2].upper() if len(sys.argv) > 2 else ''
    d = open(path, 'rb').read()
    if d[:4] != b'RA-I':
        print('Kein RA-I-Image: %r' % d[:8])
        return
    # Record-Layout (16 Byte, Big-Endian), erste Record @0:
    #   char Name[4], u16 A, u16 B, u32 Offset, u32 Size
    rec0_a, rec0_b = struct.unpack('>HH', d[4:8])
    count = rec0_b
    print('Magic: RA-I  Version: %d  Eintraege: %d  Imagegroesse: %d' % (rec0_a, count, len(d)))
    print()
    off = 0
    total = 0
    for i in range(count):
        name = d[off + 0:off + 4].decode('latin1').replace('\x00', '')
        a, b = struct.unpack('>HH', d[off + 4:off + 8])
        o, s = struct.unpack('>II', d[off + 8:off + 16])
        total += s
        n = name.strip()
        if not n and not flt:
            pass
        if flt and flt not in n.upper():
            off += 16
            continue
        print('R%-3d %-8s A=%-5d B=%-7d Off=0x%06X Size=0x%06X' % (i, n or '?', a, b, o, s))
        off += 16
    print()
    print('Summe Size: 0x%X (%d Bytes)' % (total, total))



if __name__ == '__main__':
    main()
