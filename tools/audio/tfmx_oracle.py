#!/usr/bin/env python3
"""
tfmx_oracle.py - STRUDELFORCE TFMX Professional reference player (Python oracle)

Integer-exact mirror of src/tfmx_player.c. Used for zero-gcc verification:
  python3 tools/tfmx_oracle.py --scan  <dir|zip>      # load/parse every module
  python3 tools/tfmx_oracle.py --wav out.wav file.tfx [--sub N] [--secs S]
  python3 tools/tfmx_oracle.py --trace file.tfx       # jiffy trace (golden ref)

Format: Chris Huelsbeck TFMX 1.5 / Pro / 7V (mdat+smpl, .tfx+.sam, TFHD, TFMXPAK).
NOT Jochen Hippel's "TFMX"/COSO format.
"""
import sys, os, struct, zipfile, io

NOTEVALS = [
 0x6AE,0x64E,0x5F4,0x59E,0x54D,0x501,0x4B9,0x475,0x435,0x3F9,0x3C0,0x38C,
 0x358,0x32A,0x2FC,0x2D0,0x2A8,0x282,0x25E,0x23B,0x21B,0x1FD,0x1E0,0x1C6,
 0x1AC,0x194,0x17D,0x168,0x154,0x140,0x12F,0x11E,0x10E,0x0FE,0x0F0,0x0E3,
 0x0D6,0x0CA,0x0BF,0x0B4,0x0AA,0x0A0,0x097,0x08F,0x087,0x07F,0x078,0x071,
 0x0D6,0x0CA,0x0BF,0x0B4,0x0AA,0x0A0,0x097,0x08F,0x087,0x07F,0x078,0x071,
 0x0D6,0x0CA,0x0BF,0x0B4]
PAULA_CLOCK = 3546895          # PAL
CIA_CLOCK   = 709379
CIA_STEP    = CIA_CLOCK >> 4   # 44336
CIA_DIVIDEND = 0x1B51F8        # ciaTimer = 0x1B51F8 / bpm
NVOICES = 8
MAX_MACRO_CMDS = 64            # runaway guard per voice per jiffy
MAX_PATT_CMDS = 128

def s8(x):  return x - 256 if x & 0x80 else x
def s16(x): return x - 65536 if x & 0x8000 else x

class LoadError(Exception): pass

def split_container(data, sam=None):
    """Returns (mdat, smpl). Handles raw mdat, TFHD and TFMXPAK."""
    if data[:4] == b"TFHD":
        hsz = struct.unpack(">I", data[4:8])[0]
        msz, ssz = struct.unpack(">II", data[10:18])
        return data[hsz:hsz+msz], data[hsz+msz:hsz+msz+ssz]
    if data[:8] == b"TFMXPAK ":
        end = data.find(b">>>")
        if end < 0: raise LoadError("TFMXPAK header")
        parts = data[8:end].split()
        msz, ssz = int(parts[0]), int(parts[1])
        o = end + 3
        return data[o:o+msz], data[o+msz:o+msz+ssz]
    return data, (sam or b"")

class Voice:
    def __init__(s): s.reset_all()
    def reset_all(s):
        s.mrun=0; s.mptr=0; s.mstep=0; s.mwait=0; s.mloop=-1
        s.rptr=0; s.rstep=0; s.keyup=0
        s.note=0; s.prevnote=0; s.velo=0; s.finetune=0
        s.dest=0; s.cur=0
        s.porta_rate=0; s.porta_reset=0; s.porta_time=0; s.porta_per=0
        s.vib_reset=0; s.vib_time=0; s.vib_width=0; s.vib_off=0
        s.env_rate=0; s.env_reset=0; s.env_time=0; s.env_end=0
        s.ab_time=0; s.ab_reset=0; s.ab_delta=0
        s.vol=0; s.dma_wait=-1
        # Paula: registers written by macros, latched on DMA start / reload
        s.reg_start=0; s.reg_len=1
        s.lat_start=0; s.lat_bytes=2; s.pos=0; s.dma=0
        s.reloads=0

class Track:
    def __init__(s):
        s.pnum=0xFF; s.paddr=0; s.pstep=0; s.pwait=0; s.ploop=-1; s.xpose=0
        s.raddr=0; s.rstep=0

class Tfmx:
    def __init__(s, data, sam=None):
        m, sm = split_container(data, sam)
        if len(m) < 0x200 or m[:8].upper() != b"TFMXSONG" and m[:9] != b"TFMX-SONG":
            raise LoadError("no TFMX-SONG header")
        s.m = m; s.smpl = sm
        s.oob = 0; s.unknown = {}
        s.title = " | ".join(m[0x10+i*40:0x10+i*40+40].decode("latin-1").strip() for i in range(6)).strip(" |")
        t,p,mc = struct.unpack(">III", m[0x1D0:0x1DC])
        if t == 0 and p == 0 and mc == 0:
            t,p,mc = 0x800,0x400,0x600          # TFMX 1.5 fixed layout
        if not (t < len(m) and p < len(m) and mc < len(m)):
            raise LoadError("bad section offsets %x %x %x" % (t,p,mc))
        s.track_off, s.patt_off, s.macro_off = t,p,mc
        s.starts = struct.unpack(">32H", m[0x100:0x140])
        s.ends   = struct.unpack(">32H", m[0x140:0x180])
        s.tempos = struct.unpack(">32H", m[0x180:0x1C0])
        # trackstep table ends at first pattern data / pointer table
        lim = len(m)
        for o in (p, mc, s.u32(p)):          # pattern data usually follows the trackstep table
            if t < o < lim: lim = o
        s.num_steps = (lim - t) // 16
        s.num_subsongs = 0
        for i in range(31):
            st, en = s.starts[i], s.ends[i]
            if i > 0 and st == 0 and en == 0: continue
            if en >= st and en < s.num_steps:
                s.num_subsongs = i + 1
        s.voices = [Voice() for _ in range(NVOICES)]
        s.tracks = [Track() for _ in range(8)]
        s.playing = 0

    # ---------- bounds-checked readers ----------
    def u16(s,o): return (s.m[o]<<8)|s.m[o+1] if 0<=o and o+2<=len(s.m) else 0
    def u32(s,o): return struct.unpack(">I",s.m[o:o+4])[0] if 0<=o and o+4<=len(s.m) else 0
    def cmd(s,o):
        if 0 <= o and o+4 <= len(s.m): return s.m[o],s.m[o+1],s.m[o+2],s.m[o+3]
        s.oob += 1; return (0xF4,0,0,0)   # safe "stop"
    def patt_addr(s,n):
        a = s.u32(s.patt_off + 4*n)
        if a == 0 or a >= len(s.m): s.oob += 1; return -1
        return a
    def macro_addr(s,n):
        a = s.u32(s.macro_off + 4*n)
        if a == 0 or a >= len(s.m): s.oob += 1; return -1
        return a

    # ---------- song control ----------
    def start(s, sub, rate=16574):
        if sub >= max(1,s.num_subsongs): sub = 0
        s.rate = rate
        s.voices = [Voice() for _ in range(NVOICES)]
        s.tracks = [Track() for _ in range(8)]
        s.first = s.starts[sub]; s.last = s.ends[sub]; s.pos = s.first
        s.speed = 1; s.set_cia(0) # Default speed 1 (Turrican 3 / TFMX Pro double speed)
        tp = s.tempos[sub]
        if tp >= 0x10: s.set_cia(tp); s.speed = 0
        elif tp: s.speed = tp
        s.speed_cnt = 0; s.tloop = -1; s.master = 64
        s.fade_dest = 64; s.fade_time = 0; s.fade_reset = 0; s.fade_step = 0
        s.jiffy = 0; s.acc = 0
        s.playing = 1
        s.get_trackstep()

    def set_cia(s, bpm):
        # 32-bit-safe jiffy clock in units of 1/16 CIA tick per output sample.
        # bpm==0 -> PAL VBlank 50 Hz (CIA timer 14188)
        timer = 14188 if bpm == 0 else CIA_DIVIDEND // bpm
        s.jlen = timer * (s.rate >> 4) + ((timer * (s.rate & 15)) >> 4)
        s.bpm = bpm

    def get_trackstep(s):
        guard = 0
        while True:
            guard += 1
            if guard > 64 or s.pos >= s.num_steps:
                s.playing = 0; return
            o = s.track_off + 16*s.pos
            w = [s.u16(o+2*i) for i in range(8)]
            if w[0] == 0xEFFE:
                c = w[1]
                if c == 0:                       # stop song
                    s.playing = 0; return
                elif c == 1:                     # loop
                    if s.tloop == 0:
                        s.tloop = -1; s.pos += 1; continue
                    elif s.tloop < 0: s.tloop = w[3] - 1
                    else: s.tloop -= 1
                    s.pos = w[2]; continue
                elif c == 2:                     # speed / tempo
                    s.speed = w[2]; s.speed_cnt = 0
                    if w[3] & 0x1FF >= 0x10 and not (w[3] & 0xF000):
                        s.set_cia(w[3] & 0x1FF)
                    s.pos += 1; continue
                elif c == 4:                     # fade
                    s.start_fade(w[2] & 0xFF, w[3] & 0xFF)
                    s.pos += 1; continue
                else:                            # 3 = 7V timeshare, others: ignore
                    s.pos += 1; continue
            for i in range(8):
                tr = s.tracks[i]
                pn, xp = w[i] >> 8, s8(w[i] & 0xFF)
                if pn < 0x80:
                    a = s.patt_addr(pn)
                    if a < 0: tr.pnum = 0xFF; continue
                    tr.pnum = pn; tr.paddr = a; tr.pstep = 0; tr.pwait = 0; tr.ploop = -1; tr.xpose = xp
                elif pn == 0xFE:                 # stop track + silence voice
                    tr.pnum = 0xFF
                    v = s.voices[xp & 7]; v.mrun = 0; v.dma = 0
                elif pn == 0xFF:
                    tr.pnum = 0xFF
                else:                            # 0x80..0xFD: keep pattern running, new transpose
                    tr.xpose = xp
            return

    def start_fade(s, speed, dest):
        s.fade_dest = min(dest, 64)
        if speed == 0 or s.fade_dest == s.master:
            s.master = s.fade_dest; s.fade_time = 0; return
        s.fade_reset = s.fade_time = speed
        s.fade_step = 1 if s.fade_dest > s.master else -1

    # ---------- pattern sequencer ----------
    def note_port(s, b0, b1, b2, b3):
        v = s.voices[b2 & (NVOICES-1)]
        if b0 < 0xC0:
            v.prevnote = v.note; v.note = b0 & 0x3F
            v.velo = (b2 >> 4) & 0xF; v.finetune = s8(b3)
            v.keyup = 0; v.mloop = -1; v.dma_wait = -1
            a = s.macro_addr(b1)
            if a < 0: v.mrun = 0; return
            v.mptr = a; v.mstep = 0; v.mwait = 0; v.mrun = 1
        elif b0 < 0xF0:                          # portamento note
            v.porta_reset = b1; v.porta_time = 1
            if not v.porta_rate: v.porta_per = v.dest
            v.porta_rate = b3
            v.prevnote = v.note; v.note = b0 & 0x3F
            v.dest = NOTEVALS[v.note]
        elif b0 == 0xF5: v.keyup = 1
        elif b0 == 0xF6:
            v.vib_reset = b1 & 0xFE; v.vib_time = v.vib_reset >> 1
            v.vib_width = s8(b3); v.vib_off = 0
        elif b0 == 0xF7:
            v.env_rate = 1; v.env_reset = v.env_time = max(1, b1)   # F7 sp cv ev
            v.env_end = min(64, b3)

    def do_track(s, i):
        tr = s.tracks[i]
        if tr.pnum >= 0x90: return False
        if tr.pwait:
            tr.pwait -= 1; return False
        for _ in range(MAX_PATT_CMDS):
            b0,b1,b2,b3 = s.cmd(tr.paddr + 4*tr.pstep)
            tr.pstep += 1
            if b0 < 0xF0:
                wait = (b0 & 0xC0) == 0x80
                if wait: tr.pwait = b3; b3 = 0
                n = (b0 + tr.xpose) & 0x3F
                if (b0 & 0xC0) == 0xC0: n |= 0xC0
                s.note_port(n, b1, b2, b3)
                if wait: return False
                continue
            if b0 == 0xF0:                       # end -> next trackstep
                tr.pnum = 0xFF; return True
            elif b0 == 0xF1:                     # loop
                if tr.ploop == 0: tr.ploop = -1; continue
                elif tr.ploop < 0: tr.ploop = b1 - 1
                else: tr.ploop -= 1
                tr.pstep = (b2 << 8) | b3
            elif b0 == 0xF2:                     # jump
                a = s.patt_addr(b1)
                if a < 0: tr.pnum = 0xFF; return False
                tr.pnum = b1; tr.paddr = a; tr.pstep = (b2 << 8) | b3; tr.ploop = -1
            elif b0 == 0xF3:                     # wait
                tr.pwait = b1; return False
            elif b0 == 0xF4:                     # stop track
                tr.pnum = 0xFF; return False
            elif b0 in (0xF5,0xF6,0xF7):
                s.note_port(b0,b1,b2,b3)
            elif b0 == 0xF8:                     # gosub pattern
                a = s.patt_addr(b1)
                if a < 0: continue
                tr.raddr, tr.rstep = tr.paddr, tr.pstep
                tr.paddr = a; tr.pstep = (b2 << 8) | b3
            elif b0 == 0xF9:                     # return
                tr.paddr, tr.pstep = tr.raddr, tr.rstep
            elif b0 == 0xFA:                     # fade master volume
                s.start_fade(b1, b3)
            elif b0 == 0xFB:                     # play pattern on other track
                t2 = s.tracks[b2 & 7]; a = s.patt_addr(b1)
                if a >= 0:
                    t2.pnum = b1; t2.paddr = a; t2.pstep = 0; t2.pwait = 0; t2.ploop = -1; t2.xpose = s8(b3)
            elif b0 == 0xFE:                     # stop custom pattern
                s.tracks[b2 & 7].pnum = 0xFF
            # FC lock, FD cue, FF nop: ignored
        tr.pnum = 0xFF; return False             # runaway guard

    def do_tracks(s):
        done = [False]*8
        restarts = 0
        i = 0
        while i < 8:
            if not done[i]:
                done[i] = True
                if s.do_track(i):
                    # F0: advance trackstep, then run freshly started tracks this same tick
                    s.pos = s.first if s.pos >= s.last else s.pos + 1
                    s.get_trackstep()
                    if not s.playing: return
                    restarts += 1
                    if restarts > 4: return
                    for k in range(8):
                        if s.tracks[k].pnum < 0x80 and s.tracks[k].pstep == 0: done[k] = False
                    i = 0; continue
            i += 1

    # ---------- macro engine ----------
    def set_period_from(s, v, note, fine):
        b = NOTEVALS[note & 0x3F]
        b = (b * (fine + v.finetune + 0x100)) >> 8
        v.dest = b
        if not v.porta_rate: v.cur = b

    def paula_on(s, v):
        v.lat_start = v.reg_start & 0xFFFFFE; v.lat_bytes = (v.reg_len or 0x10000) * 2
        v.pos = 0; v.dma = 1

    def do_macro(s, v, vi):
        if not v.mrun: return
        if v.mwait:
            v.mwait -= 1; return
        if v.dma_wait >= 0: return               # WaitOnDMA suspended
        for _ in range(MAX_MACRO_CMDS):
            b0,b1,b2,b3 = s.cmd(v.mptr + 4*v.mstep)
            v.mstep += 1
            w1 = (b2 << 8) | b3
            op = b0
            if op == 0x00 or op == 0x13:         # DMAoff (+reset)
                if op == 0x00:
                    v.env_rate = v.vib_reset = v.porta_rate = v.ab_time = 0
                    if b1: v.vol = min(64, b3)   # 00 01 xx vv: reset + set volume
                v.dma = 0
            elif op == 0x01: s.paula_on(v)       # DMAon
            elif op == 0x02:                     # SetBegin
                v.ab_time = 0; v.reg_start = (b1 << 16) | w1
            elif op == 0x03: v.reg_len = w1      # SetLen (words)
            elif op == 0x04:                     # Wait
                v.mwait = w1; return
            elif op == 0x05 or (op == 0x10 and not v.keyup):  # Loop / LoopKeyUp
                if v.mloop == 0: v.mloop = -1; continue
                elif v.mloop < 0: v.mloop = b1 - 1
                else: v.mloop -= 1
                v.mstep = w1
            elif op == 0x10: pass                # key released: fall through
            elif op == 0x06:                     # Cont
                a = s.macro_addr(b1)
                if a < 0: v.mrun = 0; return
                v.mptr = a; v.mstep = w1; v.mloop = -1
            elif op == 0x07: v.mrun = 0; return  # Stop
            elif op == 0x08: s.set_period_from(v, v.note + s8(b1), s16(w1))
            elif op == 0x09: s.set_period_from(v, b1, s16(w1))
            elif op == 0x1F: s.set_period_from(v, v.prevnote + s8(b1), s16(w1))
            elif op == 0x0A: v.env_rate = v.vib_reset = v.porta_rate = v.ab_time = 0
            elif op == 0x0B:                     # Portamento
                v.porta_reset = b1; v.porta_time = 1
                if not v.porta_rate: v.porta_per = v.dest
                v.porta_rate = w1
            elif op == 0x0C:                     # Vibrato
                v.vib_reset = b1; v.vib_time = b1 >> 1; v.vib_width = s8(b3)
                if not v.porta_rate: v.cur = v.dest; v.vib_off = 0
            elif op == 0x0D:                     # AddVolume
                v.vol = max(0, min(64, v.velo*3 + s8(b3)))
            elif op == 0x0E: v.vol = min(64, b3) # SetVolume
            elif op == 0x0F:                     # Envelope
                v.env_rate = b1; v.env_reset = v.env_time = b2; v.env_end = min(64, b3)
            elif op == 0x11:                     # AddBegin
                v.ab_time = v.ab_reset = b1; v.ab_delta = s16(w1)
                v.reg_start = (v.reg_start + v.ab_delta) & 0xFFFFFF
            elif op == 0x12: v.reg_len = (v.reg_len + s16(w1)) & 0xFFFF   # AddLen
            elif op == 0x14:                     # WaitKeyUp (timeout b3)
                if v.keyup: v.mloop = -1; continue
                if v.mloop == 0: v.mloop = -1; continue
                elif v.mloop < 0: v.mloop = b3 - 1
                else: v.mloop -= 1
                v.mstep -= 1; return
            elif op == 0x15:                     # GoSub
                a = s.macro_addr(b1)
                if a < 0: continue
                v.rptr, v.rstep = v.mptr, v.mstep
                v.mptr = a; v.mstep = w1; v.mloop = -1
            elif op == 0x16: v.mptr, v.mstep = v.rptr, v.rstep   # Return
            elif op == 0x17:                     # SetPeriod
                v.dest = w1
                if not v.porta_rate: v.cur = w1
            elif op == 0x18:                     # SampleLoop (applies on next reload)
                off = ((b1 << 16) | w1) & 0xFFFFFE
                v.reg_start = (v.reg_start + off) & 0xFFFFFF
                v.reg_len = max(1, v.reg_len - (off >> 1))
            elif op == 0x19:                     # OneShot -> loop on silent word 0
                v.ab_time = 0; v.reg_start = 0; v.reg_len = 1
            elif op == 0x1A:                     # WaitOnDMA
                v.dma_wait = w1; return
            elif op == 0x1C:                     # SplitKey
                if v.note > b1: v.mstep = w1
            elif op == 0x1D:                     # SplitVolume
                if v.vol > b1: v.mstep = w1
            elif op == 0x21:                     # PlayMacro on other voice
                s.note_port(v.note, b1, b2, v.finetune & 0xFF)
            else:
                s.unknown[op] = s.unknown.get(op, 0) + 1
        v.mrun = 0                               # runaway guard

    def do_effects(s, v):
        if v.ab_time:                            # AddBegin sweep
            v.reg_start = (v.reg_start + v.ab_delta) & 0xFFFFFF
            v.ab_time -= 1
            if v.ab_time == 0: v.ab_time = v.ab_reset; v.ab_delta = -v.ab_delta
        if v.vib_reset:                          # Vibrato
            v.vib_off += v.vib_width
            a = (v.dest * (0x800 + v.vib_off)) >> 11
            if not v.porta_rate: v.cur = a
            v.vib_time -= 1
            if v.vib_time <= 0: v.vib_time = v.vib_reset; v.vib_width = -v.vib_width
        if v.porta_rate:                         # Portamento
            v.porta_time -= 1
            if v.porta_time <= 0:
                v.porta_time = v.porta_reset
                a = v.porta_per
                if v.porta_per > v.dest:
                    a = (v.porta_per * (256 - v.porta_rate) - 128) >> 8
                    if a <= v.dest: v.porta_rate = 0
                elif v.porta_per < v.dest:
                    a = (v.porta_per * (256 + v.porta_rate)) >> 8
                    if a >= v.dest: v.porta_rate = 0
                else: v.porta_rate = 0
                if not v.porta_rate: a = v.dest
                v.porta_per = v.cur = a
        if v.env_rate:                           # Envelope
            v.env_time -= 1
            if v.env_time <= 0:
                v.env_time = v.env_reset
                if v.vol > v.env_end:
                    v.vol -= v.env_rate
                    if v.vol <= v.env_end: v.vol = v.env_end; v.env_rate = 0
                elif v.vol < v.env_end:
                    v.vol += v.env_rate
                    if v.vol >= v.env_end: v.vol = v.env_end; v.env_rate = 0
                else: v.env_rate = 0

    def jiffy_tick(s):
        for i,v in enumerate(s.voices):
            s.do_macro(v, i); s.do_effects(v)
        if s.fade_time:
            s.fade_time -= 1
            if s.fade_time == 0:
                s.fade_time = s.fade_reset; s.master += s.fade_step
                if s.master == s.fade_dest: s.fade_time = 0
        if s.playing:
            if s.speed_cnt == 0:
                s.speed_cnt = s.speed
                s.do_tracks()
            else:
                s.speed_cnt -= 1
        s.jiffy += 1

    # ---------- Paula mixer (mono, integer) ----------
    def render(s, n):
        out = []
        sm = s.smpl; ssz = len(sm)
        incs = [0]*NVOICES
        for i in range(n):
            if s.acc <= 0:
                s.jiffy_tick(); s.acc += s.jlen
                for k,v in enumerate(s.voices):
                    per = v.cur if v.cur >= 113 else 113   # OCS DMA limit (~124), keep audible
                    incs[k] = ((((PAULA_CLOCK << 4) // per) << 12) // s.rate) if v.cur else 0
            s.acc -= CIA_STEP
            acc = 0
            for k,v in enumerate(s.voices):
                if not v.dma or not incs[k]: continue
                idx = v.lat_start + (v.pos >> 16)
                if idx < ssz:
                    b = sm[idx]; acc += (b - 256 if b & 0x80 else b) * v.vol
                else:
                    s.oob += 1
                v.pos += incs[k]
                if (v.pos >> 16) >= v.lat_bytes:          # Paula reload from registers
                    v.pos -= v.lat_bytes << 16
                    v.lat_start = v.reg_start & 0xFFFFFE; v.lat_bytes = (v.reg_len or 0x10000) * 2
                    if (v.pos >> 16) >= v.lat_bytes: v.pos = 0
                    v.reloads += 1
                    if v.dma_wait >= 0:
                        if v.dma_wait == 0: v.dma_wait = -1
                        else: v.dma_wait -= 1
            acc = (acc * s.master) >> 6
            acc = (acc * 3) >> 2
            out.append(max(-32768, min(32767, acc)))
        return out

# ---------------------------------------------------------------------------
def iter_modules(src):
    """Yields (name, mdat_bytes, smpl_bytes_or_None) from a dir or zip."""
    if zipfile.is_zipfile(src):
        z = zipfile.ZipFile(src); names = set(z.namelist())
        get = lambda n: z.read(n)
        lst = sorted(names)
    else:
        lst = []
        for r,_,fs in os.walk(src):
            for f in fs: lst.append(os.path.join(r,f))
        names = set(lst); get = lambda n: open(n,"rb").read()
        lst.sort()
    for n in lst:
        base = os.path.basename(n); low = base.lower()
        if low.endswith(".tfx"):
            sam = n[:-4] + ".sam"
            if sam not in names:   # shared sample set (e.g. Einmal Kanzler Sein/Set.sam)
                cand = [x for x in names if os.path.dirname(x) == os.path.dirname(n) and x.lower().endswith(".sam")]
                sam = cand[0] if len(cand) == 1 else None
            yield n, get(n), (get(sam) if sam else None)
        elif low.startswith("mdat."):
            sam = os.path.join(os.path.dirname(n), "smpl." + base[5:])
            yield n, get(n), (get(sam) if sam in names else None)
        elif low.endswith(".tfmx"):
            yield n, get(n), None

def scan(src, secs=20):
    ok = bad = 0; issues = []
    for name, d, sm in iter_modules(src):
        try:
            t = Tfmx(d, sm)
            if not t.smpl: raise LoadError("missing smpl")
            nsub = max(1, t.num_subsongs)
            t.start(0); pcm = t.render(16574 * secs)
            peak = max(abs(x) for x in pcm)
            if t.oob or peak == 0:
                issues.append((name, "oob=%d peak=%d steps=%d" % (t.oob, peak, t.num_steps)))
            ok += 1
        except LoadError as e:
            bad += 1; issues.append((name, "LOAD: %s" % e))
        except Exception as e:
            bad += 1; issues.append((name, "CRASH: %r" % e))
    return ok, bad, issues

def write_wav(path, pcm, rate):
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + 2*len(pcm)) + b"WAVEfmt " +
                struct.pack("<IHHIIHH", 16, 1, 1, rate, rate*2, 2, 16) +
                b"data" + struct.pack("<I", 2*len(pcm)))
        f.write(struct.pack("<%dh" % len(pcm), *pcm))

if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "--scan":
        ok, bad, iss = scan(a[1], int(a[2]) if len(a) > 2 else 10)
        for n,m in iss: print("  %-80s %s" % (n[-80:], m))
        print("SCAN ok=%d failed=%d warnings=%d" % (ok, bad, len(iss)))
    elif a and a[0] == "--wav":
        out, fn = a[1], a[2]
        sub = int(a[a.index("--sub")+1]) if "--sub" in a else 0
        secs = int(a[a.index("--secs")+1]) if "--secs" in a else 30
        d = open(fn,"rb").read(); smp = None
        if fn.lower().endswith(".tfx") and os.path.exists(fn[:-4]+".sam"): smp = open(fn[:-4]+".sam","rb").read()
        t = Tfmx(d, smp); t.start(sub, 22050); pcm = t.render(22050*secs)
        write_wav(out, pcm, 22050); print("WAV", out, "oob", t.oob, "unknown", t.unknown)
    else:
        print(__doc__)
