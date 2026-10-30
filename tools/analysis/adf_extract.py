#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
adf_extract.py - Generischer AmigaDOS FFS (Fast File System) ADF-Parser & -Extractor
=====================================================================================
Teil der Turrican-2-REA (docs/TURRICAN2_REA/), generisch fuer jede Standard-ADF.

FFS-Layout (880K, 1760 Sektoren a 512B):
  Sektor 0: Bootblock ("DOS\\0", Checksum, Volume-Size, Bitmap/Dir-Offsets)
  Sektor 2: Bitmap (1 Bit pro Sektor)
  Sektor 3: Root-Directory (44B-Entries, Block-Kette via 'next' Entry0)
  Sektor 4+: Daten (Dateien ueber Extent-Blöcke zerstreut)

Extent-Block: u32 next (0xFFFFFFFF=Ende), u32 length (Sektoren), u32 first

CLI:
  list <adf>            : Bootblock + Dateitabelle anzeigen
  extract <adf> <dir>   : alle Dateien extrahieren
  dump <adf> <sec> [n]  : roher Hexdump
  vhd2raw <vhd> <out> [off] : WinUAE-VHD ('VAE--ADF') -> rohes 512er-Image
"""
import os
import struct
import sys

SECTOR = 512
DIR_ENTRY = 44
NODATA = 0xFFFFFFFF
FT_FILE = 0x0001
FT_DIR = 0x0002


class ADF:
    def __init__(self, data, path=""):
        self.data = data
        self.path = path
        self.sectors = len(data) // SECTOR
        self.boot = None
        self.files = []
        self.warn = []

    def sector(self, n):
        if n < 0 or n >= self.sectors:
            raise IndexError("sector %d out of range (0..%d)" % (n, self.sectors - 1))
        return self.data[n * SECTOR:(n + 1) * SECTOR]

    def parse_bootblock(self):
        b = self.sector(0)
        if b[0:4] != b"DOS\x00":
            raise ValueError("kein AmigaDOS-Bootblock (Magic != 'DOS\\0')")
        v = {
            "checksum": struct.unpack(">I", b[4:8])[0],
            "volumesize": struct.unpack(">I", b[8:12])[0],
            "reserved": struct.unpack(">I", b[12:16])[0],
            "bitmap_off": struct.unpack(">I", b[16:20])[0],
            "dir_off": struct.unpack(">I", b[20:24])[0],
            "bitmap_size": struct.unpack(">I", b[24:28])[0],
            "dir_size": struct.unpack(">I", b[28:32])[0],
            "name": b[32:50].split(b"\x00")[0].decode("ascii", "replace"),
        }
        self.boot = v
        return v

    def _dir_walk(self, dir_sector, out):
        """Verzeichnis-Blockkette: 44B-Entries sammeln (Kette: next von Entry0)."""
        sec = dir_sector
        guard = 0
        while sec != NODATA and sec < self.sectors and guard < 512:
            guard += 1
            blk = self.sector(sec)
            for i in range(0, SECTOR - DIR_ENTRY + 1, DIR_ENTRY):
                et = struct.unpack(">H", blk[i:i + 2])[0]
                if et == 0:
                    continue
                name = blk[i + 35:i + 51].split(b"\x00")[0].decode("ascii", "replace").strip()
                if not name:
                    continue
                out.append({
                    "type": et,
                    "size": struct.unpack(">I", blk[i + 3:i + 7])[0],
                    "modified": struct.unpack(">I", blk[i + 7:i + 11])[0],
                    "prot": struct.unpack(">I", blk[i + 11:i + 15])[0],
                    "first": struct.unpack(">I", blk[i + 27:i + 31])[0],
                    "next": struct.unpack(">I", blk[i + 31:i + 35])[0],
                    "name": name,
                })
            sec = struct.unpack(">I", blk[31:35])[0]

    def parse_files(self):
        """Bootblock + Directory (Bootblock-Werte + Fallback Sektor 3)."""
        self.parse_bootblock()
        b = self.boot
        cand_dir = []
        if 3 <= b["dir_off"] < self.sectors and b["dir_size"] > 0:
            cand_dir.append(("bootblock", b["dir_off"]))
        cand_dir.append(("standard", 3))
        best = None
        for tag, sec in cand_dir:
            entries = []
            try:
                self._dir_walk(sec, entries)
            except Exception as e:
                self.warn.append("%s dir@%d: %s" % (tag, sec, e))
                continue
            score = sum(1 for e in entries
                        if e["type"] in (FT_FILE, FT_DIR) and 0 < e["size"] < (1 << 24)
                        and e["first"] != NODATA)
            if best is None or score > best[1]:
                best = (entries, score, tag, sec)
        entries, score, tag, sec = best
        valid = [e for e in entries
                 if e["type"] == FT_FILE and e["first"] != NODATA
                 and 0 < e["size"] <= self.sectors * SECTOR]
        self.files = valid
        self.dir_sector = sec
        self.dir_tag = tag
        return valid

    def file_extents(self, first_block):
        """Extent-Kette -> Liste von (start_sektor, length_sektoren)."""
        runs = []
        sec = first_block
        guard = 0
        while sec != NODATA and sec < self.sectors and guard < 10000:
            guard += 1
            blk = self.sector(sec)
            nxt = struct.unpack(">I", blk[0:4])[0]
            length = struct.unpack(">I", blk[4:8])[0]
            start = struct.unpack(">I", blk[8:12])[0]
            if length <= 0 or start >= self.sectors:
                break
            runs.append((start, length))
            sec = nxt
        return runs

    def extract_file(self, entry):
        runs = self.file_extents(entry["first"])
        total = sum(l for _, l in runs)
        need = (entry["size"] + SECTOR - 1) // SECTOR
        if total < need:
            self.warn.append("%s: Extents %d < benoetigt %d Sektoren"
                             % (entry["name"], total, need))
        buf = bytearray()
        for start, length in runs:
            buf += self.data[start * SECTOR:(start + length) * SECTOR]
        return bytes(buf[:entry["size"]])


# ---------- CLI ----------
def hexdump(blk, base=0):
    out = []
    for i in range(0, len(blk), 16):
        row = blk[i:i + 16]
        txt = "".join(chr(c) if 32 <= c < 127 else "." for c in row)
        out.append("%7d: %-30s  %s" % (base + i, row.hex(), txt))
    return "\n".join(out)


def cmd_list(path):
    adf = ADF(open(path, "rb").read(), path)
    try:
        files = adf.parse_files()
    except ValueError as e:
        print("FEHLER:", e)
        return 1
    b = adf.boot
    print("== %s ==" % os.path.basename(path))
    print("Sektoren: %d (%d bytes), Volumenname: '%s'"
          % (adf.sectors, len(adf.data), b["name"]))
    print("Bootblock: vol_size=%d bitmap_off=%d dir_off=%d (verwendet: dir@%d, %s)"
          % (b["volumesize"], b["bitmap_off"], b["dir_off"], adf.dir_sector, adf.dir_tag))
    print("Dateien (%d):" % len(files))
    print("%-12s %10s %8s %8s  %s" % ("Name", "Size", "First", "Blocke", "Extents"))
    for f in sorted(files, key=lambda x: x["name"]):
        runs = adf.file_extents(f["first"])
        ext = ";".join("%d+%d" % (s, l) for s, l in runs)
        print("%-12s %10d %8d %8d  %s" % (f["name"], f["size"], f["first"],
                                          (f["size"] + SECTOR - 1) // SECTOR, ext))
    for w in adf.warn:
        print("WARNUNG:", w)
    return 0


def cmd_extract(path, outdir):
    adf = ADF(open(path, "rb").read(), path)
    try:
        files = adf.parse_files()
    except ValueError as e:
        print("FEHLER:", e)
        return 1
    os.makedirs(outdir, exist_ok=True)
    for f in files:
        data = adf.extract_file(f)
        dest = os.path.join(outdir, f["name"].replace("/", "_"))
        open(dest, "wb").write(data)
        print("%-12s %10d bytes -> %s" % (f["name"], len(data), dest))
    for w in adf.warn:
        print("WARNUNG:", w)
    return 0


def cmd_dump(path, sector, count=1):
    adf = ADF(open(path, "rb").read(), path)
    for i in range(sector, min(sector + count, adf.sectors)):
        print("== Sektor %d ==" % i)
        print(hexdump(adf.sector(i), i * SECTOR))
    return 0


def cmd_vhd2raw(path, out, dataoff=None):
    """WinUAE VHD ('VAE--ADF') -> rohes 512er-Disk-Image.
    Datenoffset: wenn nicht angegeben, wird nach dem DOS-Bootblock gesucht."""
    data = open(path, "rb").read()
    if data[:8] not in (b"VAE--ADF", b"VAE--VHD"):
        print("FEHLER: Signatur VAE--ADF/VHD nicht gefunden:", data[:8].hex())
        return 1
    off = dataoff
    if off is None:
        cands = []
        i = data.find(b"DOS\x00")
        while i != -1 and i < len(data) - 64:
            if i % 8 == 0:
                cands.append(i)
            i = data.find(b"DOS\x00", i + 1)
        best, best_score = None, -1
        for c in cands[:200]:
            vs = struct.unpack(">I", data[c + 8:c + 12])[0]
            score = 0
            if 100 <= vs <= 4000:
                score += 2
            if c >= 32 and data[c - 32:c] == b"\x95\x21\x35\x48" * 8:
                score += 1
            if score > best_score:
                best, best_score = c, score
        off = best
        if off is None:
            print("FEHLER: kein plausibler DOS-Bootblock im VHD gefunden")
            return 1
    raw = data[off:]
    raw = raw[: len(raw) // SECTOR * SECTOR]
    open(out, "wb").write(raw)
    print("VHD: %s, Datenoffset=%d, rohes Image: %d Sektoren (%d bytes) -> %s"
          % (data[:8].decode(), off, len(raw) // SECTOR, len(raw), out))
    return 0


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd = sys.argv[1]
    if cmd == "list":
        return cmd_list(sys.argv[2])
    if cmd == "extract":
        return cmd_extract(sys.argv[2], sys.argv[3])
    if cmd == "dump":
        return cmd_dump(sys.argv[2], int(sys.argv[3]),
                        int(sys.argv[4]) if len(sys.argv) > 4 else 1)
    if cmd == "vhd2raw":
        off = int(sys.argv[4]) if len(sys.argv) > 4 else None
        return cmd_vhd2raw(sys.argv[2], sys.argv[3], off)
    print("unbekannter Befehl:", cmd)
    return 1


if __name__ == "__main__":
    sys.exit(main())

