#!/usr/bin/env python3
"""mod_oracle.py - integer-exact Python mirror of src/mod_player.c

  python3 tools/mod_oracle.py --info  song.mod
  python3 tools/mod_oracle.py --wav   out.wav song.mod [--secs 20]
  python3 tools/mod_oracle.py --pcm   out.raw song.mod [--secs 5]   # golden ref for C (s16le mono)
"""
import sys, struct

PAULA = 3546895
VIB = [0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,
       255,253,250,244,235,224,212,197,180,161,141,120,97,74,49,24]
ARP = [65535,61858,58386,55109,52016,49096,46341,43740,41285,38968,36781,34716,32768,30929,29193,27554]

class LoadError(Exception): pass

class Smp:
    def __init__(s): s.data=b""; s.len=0; s.rep=0; s.replen=0; s.vol=0; s.name=""
class Ch:
    def __init__(s):
        s.sidx=0; s.pos=0; s.inc=0; s.period=0; s.dest=0; s.vol=0; s.active=0
        s.cmd=0; s.prm=0; s.porta=0; s.vib_pos=0; s.vib_prm=0; s.loop_row=0; s.loop_cnt=0
        s.note_per=0; s.offset_mem=0

class Mod:
    def __init__(s, buf):
        if len(buf) < 1084: raise LoadError("too small")
        sig = buf[1080:1084]
        if sig in (b"M.K.", b"M!K!", b"FLT4", b"4CHN"): s.nch = 4
        elif sig == b"6CHN": s.nch = 6
        elif sig in (b"8CHN", b"FLT8", b"OCTA"): s.nch = 8
        elif sig[2:] == b"CH" and sig[:2].isdigit() and 1 <= int(sig[:2]) <= 8: s.nch = int(sig[:2])
        else: raise LoadError("unsupported signature %r (15-sample MOD?)" % sig)
        s.title = buf[:20].split(b"\0")[0].decode("latin-1")
        s.songlen = buf[950]
        if not 1 <= s.songlen <= 128: raise LoadError("songlen")
        s.restart = buf[951]; s.orders = list(buf[952:1080])
        s.npat = max(s.orders) + 1
        off = 1084 + s.npat * 256 * s.nch
        if off > len(buf): raise LoadError("patterns truncated")
        s.pat = buf[1084:off]
        s.smp = [Smp() for _ in range(32)]
        for i in range(1, 32):
            h = buf[20+(i-1)*30:20+i*30]; x = s.smp[i]
            x.name = h[:22].split(b"\0")[0].decode("latin-1")
            ln = struct.unpack(">H", h[22:24])[0]*2
            x.vol = min(64, h[25]); x.rep = struct.unpack(">H", h[26:28])[0]*2
            x.replen = struct.unpack(">H", h[28:30])[0]*2
            x.data = buf[off:off+ln]; x.len = len(x.data)
            if x.rep > x.len: x.rep = 0; x.replen = 0
            if x.rep + x.replen > x.len: x.replen = x.len - x.rep
            off += ln

    def start(s, rate=16574):
        s.rate = rate; s.ch = [Ch() for _ in range(s.nch)]
        s.pos = s.row = s.tick = 0; s.speed = 6; s.bpm = 125; s.pdelay = 0
        s.jump = 0; s.next_pos = s.next_row = 0; s.acc = 0; s.ticks = 0; s.loops = 0; s.playing = 1
        s.events = []   # (tick, ch, sample, period) for strudel export / trace

    def set_inc(s, c, per):
        per = max(113, min(6848, per))
        c.inc = (((PAULA << 4) // per) << 12) // s.rate

    def trigger(s, c):
        sm = s.smp[c.sidx]; c.period = c.note_per
        c.pos = (c.offset_mem << 24) if c.cmd == 9 else 0
        c.active = 1 if (c.sidx and sm.len > 2) else 0
        if c.vib_prm == 0 or c.cmd != 4: c.vib_pos = 0
        if c.cmd == 9 and (c.pos >> 16) >= sm.len: c.active = 0

    @staticmethod
    def vslide(c, p):
        v = c.vol + (p >> 4) if p & 0xF0 else c.vol - (p & 15)
        c.vol = max(0, min(64, v))
    @staticmethod
    def tporta(c):
        if not c.dest or not c.porta: return
        if c.period < c.dest: c.period = min(c.dest, c.period + c.porta)
        elif c.period > c.dest: c.period = max(c.dest, c.period - c.porta)
    @staticmethod
    def vib(c):
        d = (VIB[c.vib_pos & 31] * (c.vib_prm & 15)) >> 7
        if c.vib_pos & 32: d = -d
        c.vib_pos = (c.vib_pos + (c.vib_prm >> 4)) & 63
        return d

    def row_start(s):
        base = (s.orders[s.pos]*64 + s.row) * s.nch * 4
        for k, c in enumerate(s.ch):
            e = s.pat[base+k*4:base+k*4+4]
            smp = (e[0] & 0xF0) | (e[2] >> 4); per = ((e[0] & 15) << 8) | e[1]
            c.cmd = e[2] & 15; c.prm = e[3]
            if smp and smp <= 31: c.sidx = smp; c.vol = s.smp[smp].vol
            if c.cmd == 9 and c.prm: c.offset_mem = c.prm
            if per:
                if c.cmd in (3, 5): c.dest = per
                else:
                    c.note_per = per
                    if not (c.cmd == 0xE and c.prm >> 4 == 0xD):
                        s.trigger(c); s.events.append((s.ticks, k, c.sidx, per))
            x, y = c.prm >> 4, c.prm & 15
            if c.cmd == 3 and c.prm: c.porta = c.prm
            elif c.cmd == 4:
                if y: c.vib_prm = (c.vib_prm & 0xF0) | y
                if x: c.vib_prm = (c.vib_prm & 0x0F) | (x << 4)
            elif c.cmd == 0xB: s.jump = 1; s.next_pos = c.prm; s.next_row = 0
            elif c.cmd == 0xC: c.vol = min(64, c.prm)
            elif c.cmd == 0xD:
                if not s.jump: s.next_pos = s.pos + 1
                s.jump = 1; s.next_row = x*10 + y
                if s.next_row > 63: s.next_row = 0
            elif c.cmd == 0xF and c.prm:
                if c.prm < 32: s.speed = c.prm
                else: s.bpm = c.prm
            elif c.cmd == 0xE:
                if x == 1: c.period = max(113, c.period - y)
                elif x == 2: c.period = min(1712, c.period + y)
                elif x == 6:
                    if y == 0: c.loop_row = s.row
                    else:
                        c.loop_cnt = y if c.loop_cnt == 0 else c.loop_cnt - 1
                        if c.loop_cnt: s.jump = 1; s.next_pos = s.pos; s.next_row = c.loop_row
                elif x == 0xA: c.vol = min(64, c.vol + y)
                elif x == 0xB: c.vol = max(0, c.vol - y)
                elif x == 0xC and y == 0: c.vol = 0
                elif x == 0xE and not s.pdelay: s.pdelay = y
            s.set_inc(c, c.period)

    def row_tick(s):
        for c in s.ch:
            per = c.period; x, y = c.prm >> 4, c.prm & 15; cmd = c.cmd
            if cmd == 0 and c.prm:
                st = s.tick % 3; semis = x if st == 1 else (y if st == 2 else 0)
                per = (c.period * ARP[semis]) >> 16
            elif cmd == 1: c.period = max(113, c.period - c.prm); per = c.period
            elif cmd == 2: c.period = min(6848, c.period + c.prm); per = c.period
            elif cmd == 3: s.tporta(c); per = c.period
            elif cmd == 4: per = c.period + s.vib(c)
            elif cmd == 5: s.tporta(c); s.vslide(c, c.prm); per = c.period
            elif cmd == 6: s.vslide(c, c.prm); per = c.period + s.vib(c)
            elif cmd == 0xA: s.vslide(c, c.prm)
            elif cmd == 0xE:
                if x == 9 and y and s.tick % y == 0: c.pos = 0; c.active = 1 if c.sidx else 0
                elif x == 0xC and s.tick == y: c.vol = 0
                elif x == 0xD and s.tick == y and c.note_per:
                    c.cmd = 0; s.trigger(c); c.cmd = 0xE; per = c.period
            s.set_inc(c, per)

    def do_tick(s):
        if not s.playing: return
        if s.tick == 0: s.row_start()
        else: s.row_tick()
        s.ticks += 1; s.tick += 1
        if s.tick >= s.speed * (s.pdelay + 1):
            s.tick = 0; s.pdelay = 0
            if s.jump:
                s.jump = 0
                if s.next_pos <= s.pos and not (s.next_pos == s.pos and s.next_row > s.row): s.loops += 1
                s.pos = s.next_pos & 0xFF; s.row = s.next_row
            else:
                s.row += 1
                if s.row >= 64: s.row = 0; s.pos += 1
            if s.pos >= s.songlen:
                s.pos = s.restart if s.restart < s.songlen else 0; s.loops += 1

    def render(s, n):
        out = []
        for _ in range(n):
            if not s.playing: out.append(0); continue
            if s.acc <= 0: s.do_tick(); s.acc += s.rate * 5
            s.acc -= 2 * s.bpm
            acc = 0
            for c in s.ch:
                if not c.active: continue
                sm = s.smp[c.sidx]; idx = c.pos >> 16
                if idx >= sm.len: c.active = 0; continue
                b = sm.data[idx]; acc += (b - 256 if b & 0x80 else b) * c.vol
                c.pos += c.inc; idx = c.pos >> 16
                if sm.replen > 2:
                    while idx >= sm.rep + sm.replen: c.pos -= sm.replen << 16; idx = c.pos >> 16
                elif idx >= sm.len: c.active = 0
            acc = (acc * 3) >> 2
            if s.nch > 4: acc = int(acc * 4 / s.nch)
            out.append(max(-32768, min(32767, acc)))
        return out

def write_wav(path, pcm, rate):
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36+2*len(pcm)) + b"WAVEfmt " +
                struct.pack("<IHHIIHH", 16, 1, 1, rate, rate*2, 2, 16) + b"data" + struct.pack("<I", 2*len(pcm)))
        f.write(struct.pack("<%dh" % len(pcm), *pcm))

if __name__ == "__main__":
    a = sys.argv[1:]
    secs = int(a[a.index("--secs")+1]) if "--secs" in a else 20
    if a and a[0] == "--info":
        m = Mod(open(a[1], "rb").read())
        print("title=%r ch=%d songlen=%d patterns=%d" % (m.title, m.nch, m.songlen, m.npat))
        for i in range(1, 32):
            if m.smp[i].len: print("  smp%02d len=%d rep=%d replen=%d vol=%d %s" % (i, m.smp[i].len, m.smp[i].rep, m.smp[i].replen, m.smp[i].vol, m.smp[i].name))
    elif a and a[0] in ("--wav", "--pcm"):
        m = Mod(open(a[2], "rb").read()); m.start(16574); pcm = m.render(16574*secs)
        if a[0] == "--wav": write_wav(a[1], pcm, 16574)
        else: open(a[1], "wb").write(struct.pack("<%dh" % len(pcm), *pcm))
        print("ok peak=%d loops=%d" % (max(abs(x) for x in pcm), m.loops))
    else: print(__doc__)
