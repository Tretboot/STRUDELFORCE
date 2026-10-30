#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
t2_ffs.py - Amiga FFS/OFS Dateisystem-Parser fuer ADF-Images.

FFS-Layout (Block = 512 Byte):
  Block 0 (Bootblock): "DOS " (0x444F5320), +4 Flags(2), +6 BlockExp(1),
    +7 RootBlock(1), +8 RootCyl(2), +10 RootHead(1), +11 RootSec(1),
    +12 FS-Size(4), +16 Used(4), +20 FS-Type(4), +24 VolID(4),
    +28 Modified(4), +32 VolumeName(32)
  Blockkopf: +0 Type(2: 0x42='B', 0x44='D', 0x41='A'; OFS +0x2000),
    +2 DataLength(2), +4 NextBlock(2, 0xFFFF = Ende)
  Verzeichnis-Block: 16 Eintraege zu 24 Byte ab +32:
    Name(12), Size(4), Flags(2, 0x40=DIR), Type(2), StartBlock(4)

Verwendung:
    python t2_ffs.py list  <ADF>
    python t2_ffs.py get   <ADF> <Dateiname> [Ziel]
    python t2_ffs.py dump  <ADF> <Dateiname>
"""
import struct
import sys

BLOCK = 512


class FFS(object):
    def __init__(self, data, base=0):
        self.data = data
        self.base = base

    def blk(self, n):
        return self.data[self.base + n * BLOCK:self.base + (n + 1) * BLOCK]

    def boot(self):
        bb = self.blk(0)
        if bb[0:4] not in (b'DOS ', b'DOS2'):
            raise ValueError('Kein FFS-Bootblock: %r' % bb[0:4])
        flags = struct.unpack('>H', bb[4:6])[0]
        root_blk = bb[7]
        fs_size = struct.unpack('>I', bb[12:16])[0]
        used = struct.unpack('>I', bb[16:20])[0]
        fs_type = struct.unpack('>I', bb[20:24])[0]
        volname = bb[32:64].split(b'\x00')[0].decode('latin1')
        return {'flags': flags, 'root': root_blk, 'size': fs_size,
                'used': used, 'type': fs_type, 'name': volname}

    def dir_entry(self, blkdata, idx):
        e = blkdata[32 + idx * 24:32 + (idx + 1) * 24]
        name = e[0:12].split(b'\x00')[0].decode('latin1')
        size = struct.unpack('>I', e[12:16])[0]
        flags = struct.unpack('>H', e[16:18])[0]
        ftype = struct.unpack('>H', e[18:20])[0]
        start = struct.unpack('>I', e[20:24])[0]
        return {'name': name, 'size': size, 'flags': flags, 'type': ftype,
                'start': start}

    def walk(self, start_blk, prefix=''):
        """Rekursives Verzeichnis-Listing."""
        entries = []
        blk = start_blk
        visited = set()
        while blk != 0xFFFFFFFF and blk not in visited:
            visited.add(blk)
            bd = self.blk(blk)
            nxt = struct.unpack('>H', bd[4:6])[0]
            for i in range(16):
                e = self.dir_entry(bd, i)
                if not e['name'] or e['start'] == 0xFFFFFFFF or e['name'] == '.':
                    continue
                full = prefix + '/' + e['name'] if prefix else e['name']
                is_dir = (e['flags'] & 0x40) != 0
                entries.append((full, e, is_dir))
                if is_dir:
                    entries += self.walk(e['start'], full)
            if nxt == 0xFFFF:
                break
            blk = nxt
        return entries

    def find(self, path):
        """Datei/Verzeichnis nach Namen suchen (case-insensitiv, auch Teilpfade)."""
        parts = [p for p in path.replace('\\', '/').split('/') if p]
        boot = self.boot()

        def walk_for(start, remaining):
            b = start
            seen = set()
            while b != 0xFFFFFFFF and b not in seen:
                seen.add(b)
                bd = self.blk(b)
                nxt = struct.unpack('>H', bd[4:6])[0]
                for i in range(16):
                    e = self.dir_entry(bd, i)
                    if not e['name'] or e['start'] == 0xFFFFFFFF or e['name'] == '.':
                        continue
                    if e['name'].lower() == remaining[0].lower():
                        if len(remaining) == 1:
                            return e
                        if (e['flags'] & 0x40) != 0:
                            f = walk_for(e['start'], remaining[1:])
                            if f:
                                return f
                if nxt == 0xFFFF:
                    break
                b = nxt
            return None

        return walk_for(boot['root'], parts)


def find_base(d):
    if d[0:4] in (b'DOS ', b'DOS2'):
        return 0
    for sig in (b'DOS ', b'DOS2'):
        p = d.find(sig)
        if p >= 0:
            return p
    return -1


def main():
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    if len(sys.argv) < 3:
        print(__doc__)
        return
    cmd, adf = sys.argv[1], sys.argv[2]
    d = open(adf, 'rb').read()
    base = find_base(d)
    if base < 0:
        print('Kein FFS gefunden!')
        return
    fs = FFS(d, base)
    try:
        boot = fs.boot()
    except ValueError as ex:
        print(ex)
        return
    print('Volume   : %r' % boot['name'])
    print('Root-Blk : %d  FS-Size: %d Blocks  Used: %d  Type: 0x%08X'
          % (boot['root'], boot['size'], boot['used'], boot['type']))
    print()
    if cmd == 'list':
        for full, e, is_dir in fs.walk(boot['root']):
            kind = '<DIR> ' if is_dir else ''
            print('%-8s %10d  blk=0x%05X  %s' % (kind, e['size'], e['start'], full))
    elif cmd == 'get':
        if len(sys.argv) < 4:
            print('Dateiname fehlt')
            return
        entry = fs.find(sys.argv[3])
        if not entry or (entry['flags'] & 0x40) != 0:
            print('Nicht gefunden: %s' % sys.argv[3])
            return
        data = fs.read_file(entry)
        out = sys.argv[4] if len(sys.argv) > 4 else entry['name']
        open(out, 'wb').write(data)
        print('Gespeichert: %s (%d Bytes)' % (out, len(data)))
    elif cmd == 'dump':
        if len(sys.argv) < 4:
            print('Dateiname fehlt')
            return
        entry = fs.find(sys.argv[3])
        if not entry:
            print('Nicht gefunden: %s' % sys.argv[3])
            return
        data = fs.read_file(entry)
        print('%s: %d Bytes' % (sys.argv[3], len(data)))
        for off in range(0, min(len(data), 512), 16):
            row = data[off:off + 16]
            print('%04X  %-48s %s' % (off, ' '.join('%02X' % b for b in row),
                                      ''.join(chr(b) if 32 <= b < 127 else '.' for b in row)))


if __name__ == '__main__':
    main()

