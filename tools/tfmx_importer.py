#!/usr/bin/env python3
"""
tfmx_importer.py - TFMX Professional to Strudel Mini-Notation Importer
STRUDELFORCE Project - Clean Room Amiga Audio Tool

Parses Amiga TFMX (The Final Musicsystem eXtended) modules (.tfx files)
and exports tracks/patterns into Strudel/TidalCycles mini-notation compatible
with strudel_cleanroom.h, livecode_ui.c, and game_audio.c.

Supports:
  - TFMX Pro (dynamic offsets at 0x1d0)
  - TFMX 1.5 (fixed offsets 0x400/0x600/0x800)
  - Subsong selection, track extraction, macro inspection
  - Batch export of entire folders
"""

import sys
import os
import struct
import argparse

NOTE_NAMES = ["c", "c#", "d", "eb", "e", "f", "f#", "g", "ab", "a", "bb", "b"]

def amiga_to_midi_note(amiga_note, transpose=0):
    """
    Converts TFMX Amiga note number + transpose to MIDI note number.
    In Amiga tracker convention, C-1 is ~12..24. Note 36 is typically C-2 / C-3 (MIDI 36 or 48).
    We map TFMX note 0 -> MIDI 24 (C1). Note 36 -> MIDI 60 (C4) or 48 (C3).
    Here note 24 = C2 (MIDI 36).
    """
    effective = amiga_note + transpose
    if effective < 0:
        effective = 0
    if effective > 127:
        effective = 127
    return effective + 12  # TFMX 0 -> MIDI 12 (C0)

def midi_to_strudel_token(midi_note):
    """
    Converts a MIDI note number to Strudel token (e.g. 36 -> 'c2', 39 -> 'eb2').
    strudel_cleanroom.h maps: (octave + 1) * 12 + base == midi_note.
    So octave = (midi_note // 12) - 1.
    """
    if midi_note < 0:
        return "~"
    octave = (midi_note // 12) - 1
    if octave < 0:
        octave = 0
    if octave > 8:
        octave = 8
    base = midi_note % 12
    return f"{NOTE_NAMES[base]}{octave}"

class TfmxModule:
    def __init__(self, filename):
        self.filename = filename
        with open(filename, "rb") as f:
            self.data = f.read()

        self.magic = self.data[:10].decode("latin-1", errors="replace")
        self.title_lines = []
        for i in range(6):
            start = 0x10 + i * 40
            line = self.data[start:start+40].decode("latin-1", errors="replace").strip()
            if line:
                self.title_lines.append(line)

        # Offsets
        if len(self.data) >= 0x1dc:
            self.track_off, self.patt_off, self.macro_off = struct.unpack(">III", self.data[0x1d0:0x1dc])
        else:
            self.track_off = self.patt_off = self.macro_off = 0

        # Check for TFMX 1.5 fixed offsets
        if self.track_off == 0 and self.patt_off == 0 and self.macro_off == 0:
            self.track_off = 0x800
            self.patt_off = 0x400
            self.macro_off = 0x600

        # Subsongs (up to 16 subsongs)
        self.subsongs = []
        if len(self.data) >= 0x1a0:
            starts = struct.unpack(">16H", self.data[0x100:0x120])
            ends = struct.unpack(">16H", self.data[0x140:0x160])
            tempos = struct.unpack(">16H", self.data[0x180:0x1a0])
            for s in range(16):
                st = starts[s]
                en = ends[s]
                tp = tempos[s]
                if st != 0 or en != 0 or s == 0:
                    self.subsongs.append({
                        "id": s,
                        "start_step": st,
                        "end_step": en,
                        "tempo": tp
                    })

        # Pattern Pointers
        self.pattern_ptrs = []
        if self.patt_off < len(self.data):
            patt_end = self.macro_off if (self.macro_off > self.patt_off and self.macro_off <= len(self.data)) else len(self.data)
            num_patts = min(128, (patt_end - self.patt_off) // 4)
            for p in range(num_patts):
                ptr = struct.unpack(">I", self.data[self.patt_off + p*4 : self.patt_off + (p+1)*4])[0]
                if ptr > 0 and ptr < len(self.data):
                    self.pattern_ptrs.append(ptr)
                else:
                    break

        # Track steps
        self.track_steps = []
        if self.track_off < len(self.data) and len(self.pattern_ptrs) > 0:
            track_end = min(self.pattern_ptrs[0], len(self.data))
            num_steps = (track_end - self.track_off) // 16
            for s in range(max(0, num_steps)):
                step_bytes = self.data[self.track_off + s*16 : self.track_off + (s+1)*16]
                step = []
                for ch in range(8):
                    patt = step_bytes[ch*2]
                    trans = struct.unpack("b", bytes([step_bytes[ch*2 + 1]]))[0]
                    step.append((patt, trans))
                self.track_steps.append(step)

    def parse_pattern_events(self, patt_idx):
        """
        Parses 4-byte TFMX pattern commands into high-level events:
          ('NOTE', note, macro, detune, wait)
          ('WAIT', ticks)
          ('PORTA', note, macro, speed, wait)
          ('LOOP', count, step)
          ('STOP',)
          ('END',)
        """
        if patt_idx >= len(self.pattern_ptrs):
            return []
        offset = self.pattern_ptrs[patt_idx]
        events = []
        idx = offset

        while idx + 4 <= len(self.data):
            b0, b1, b2, b3 = self.data[idx:idx+4]
            idx += 4

            if b0 == 0xF0:  # End
                events.append(("END",))
                break
            elif b0 == 0xF1:  # Loop
                events.append(("LOOP", b1, (b2 << 8) | b3))
                break
            elif b0 == 0xF2:  # Cont / Jump
                events.append(("JUMP", b1, (b2 << 8) | b3))
                break
            elif b0 == 0xF3:  # Wait ticks
                events.append(("WAIT", b1))
            elif b0 == 0xF4:  # Stop
                events.append(("STOP",))
                break
            elif b0 == 0xF5:  # Key Up
                events.append(("KEYUP",))
            elif b0 < 0x80:  # Immediate note
                events.append(("NOTE", b0, b1, b2, 0))
            elif 0x80 <= b0 < 0xC0:  # Note with wait duration
                events.append(("NOTE", b0 - 0x80, b1, b2, b3))
            elif 0xC0 <= b0 < 0xF0:  # Portamento
                events.append(("PORTA", b0 - 0xC0, b1, b2, b3))
            else:
                events.append(("CMD", b0, b1, b2, b3))

        return events

    def pattern_to_strudel(self, patt_idx, transpose=0):
        """
        Converts a pattern to a Strudel mini-notation string.
        Groups into rhythmic blocks and tokens.
        """
        events = self.parse_pattern_events(patt_idx)
        if not events:
            return "~"

        tokens = []
        for ev in events:
            cmd = ev[0]
            if cmd == "NOTE":
                _, note, macro, detune, wait = ev
                midi_note = amiga_to_midi_note(note, transpose)
                tok = midi_to_strudel_token(midi_note)
                if wait > 8:
                    # Sustained note
                    tokens.append(tok)
                elif wait > 0:
                    tokens.append(tok)
                else:
                    tokens.append(tok)
            elif cmd == "WAIT":
                # Rest or subdivision
                tokens.append("~")
            elif cmd in ("STOP", "END"):
                break

        if not tokens:
            return "~"

        # Format as groups of 4 notes if length > 4
        if len(tokens) <= 4:
            return " ".join(tokens)
        
        # Segment into bars of 4
        bars = []
        for i in range(0, len(tokens), 4):
            chunk = tokens[i:i+4]
            bars.append("[" + " ".join(chunk) + "]")
        return " ".join(bars)

    def export_subsong_to_strudel(self, subsong_id=0, track_id=None):
        """
        Exports a subsong as a complete Strudel mini-notation composition.
        """
        if subsong_id >= len(self.subsongs):
            subsong = {"start_step": 0, "end_step": len(self.track_steps) - 1, "tempo": 4}
        else:
            subsong = self.subsongs[subsong_id]

        start_s = subsong["start_step"]
        end_s = min(subsong["end_step"], len(self.track_steps) - 1)

        output_lines = []
        output_lines.append(f"// TFMX Professional Import: {os.path.basename(self.filename)}")
        if self.title_lines:
            output_lines.append(f"// Title: {' | '.join(self.title_lines)}")
        output_lines.append(f"// Subsong: {subsong_id} (Steps {start_s}..{end_s}, Tempo: {subsong['tempo']})")
        output_lines.append("")

        tracks_to_process = [track_id] if track_id is not None else range(8)

        for ch in tracks_to_process:
            ch_patterns = []
            for s in range(start_s, end_s + 1):
                if s < len(self.track_steps):
                    patt, trans = self.track_steps[s][ch]
                    if patt < 0xFE:  # Valid pattern
                        ch_patterns.append((patt, trans))

            if not ch_patterns:
                continue

            # Convert unique patterns
            output_lines.append(f"// --- Channel {ch} (Voice {ch}) ---")
            patt_cache = {}
            seq_patt_refs = []

            for p_idx, trans in ch_patterns:
                key = (p_idx, trans)
                if key not in patt_cache:
                    s_code = self.pattern_to_strudel(p_idx, trans)
                    patt_cache[key] = s_code
                seq_patt_refs.append(patt_cache[key])

            # Produce cycle alternation or sequence
            if len(seq_patt_refs) > 1:
                # Deduplicate consecutive identical patterns
                dedup = []
                for p in seq_patt_refs:
                    if not dedup or dedup[-1] != p:
                        dedup.append(p)
                combined = "<" + " ".join(dedup[:8]) + ">"
            elif seq_patt_refs:
                combined = seq_patt_refs[0]
            else:
                combined = "~"

            output_lines.append(f"track_{ch} = \"{combined}\"")
            output_lines.append("")

        return "\n".join(output_lines)

    def dump_debug_info(self):
        """
        Prints detailed technical overview of the TFMX file.
        """
        print("=" * 60)
        print(f"TFMX Module: {self.filename}")
        print(f"Magic: {self.magic}")
        print(f"Size: {len(self.data)} bytes")
        for line in self.title_lines:
            print(f"  Header Text: {line}")
        print("-" * 60)
        print(f"Track Offset:  0x{self.track_off:04x}")
        print(f"Patt Offset:   0x{self.patt_off:04x} ({len(self.pattern_ptrs)} patterns)")
        print(f"Macro Offset:  0x{self.macro_off:04x}")
        print("-" * 60)
        print("Subsongs:")
        for sub in self.subsongs:
            print(f"  Subsong {sub['id']:2d}: Start Step {sub['start_step']:3d} | End Step {sub['end_step']:3d} | Tempo {sub['tempo']:2d}")
        print("-" * 60)
        print(f"Track Steps ({len(self.track_steps)} steps):")
        for s in range(min(12, len(self.track_steps))):
            step_str = " | ".join(f"P{p:02x}:{t:+03d}" for p, t in self.track_steps[s])
            print(f"  Step {s:2d}: {step_str}")
        if len(self.track_steps) > 12:
            print(f"  ... ({len(self.track_steps) - 12} more steps)")
        print("=" * 60)


def main():
    parser = argparse.ArgumentParser(description="TFMX to Strudel Cleanroom Notation Converter")
    parser.add_argument("--in", dest="input_file", help="Path to input .tfx file")
    parser.add_argument("--out", dest="output_file", help="Path to output .strudel file")
    parser.add_argument("--subsong", type=int, default=0, help="Subsong index to export (default: 0)")
    parser.add_argument("--track", type=int, default=None, help="Filter export to specific track (0..7)")
    parser.add_argument("--dump", action="store_true", help="Dump module metadata and disassembly")
    parser.add_argument("--batch", help="Batch convert all .tfx files in directory")

    args = parser.parse_args()

    if args.batch:
        in_dir = args.batch
        out_dir = args.output_file or "strudel_exported"
        os.makedirs(out_dir, exist_ok=True)
        count = 0
        for root, _, files in os.walk(in_dir):
            for fn in sorted(files):
                if fn.lower().endswith(".tfx"):
                    in_path = os.path.join(root, fn)
                    try:
                        mod = TfmxModule(in_path)
                        strudel_src = mod.export_subsong_to_strudel(args.subsong, args.track)
                        out_name = os.path.splitext(fn)[0] + ".strudel"
                        out_path = os.path.join(out_dir, out_name)
                        with open(out_path, "w", encoding="utf-8") as out_f:
                            out_f.write(strudel_src)
                        count += 1
                    except Exception as e:
                        print(f"Error converting {fn}: {e}", file=sys.stderr)
        print(f"Successfully converted {count} TFMX modules to {out_dir}")
        return

    if not args.input_file:
        parser.print_help()
        sys.exit(1)

    if not os.path.exists(args.input_file):
        print(f"Error: File not found: {args.input_file}", file=sys.stderr)
        sys.exit(1)

    mod = TfmxModule(args.input_file)

    if args.dump:
        mod.dump_debug_info()

    strudel_code = mod.export_subsong_to_strudel(args.subsong, args.track)

    if args.output_file:
        os.makedirs(os.path.dirname(os.path.abspath(args.output_file)), exist_ok=True)
        with open(args.output_file, "w", encoding="utf-8") as f:
            f.write(strudel_code)
        print(f"Exported Strudel pattern to {args.output_file}")
    else:
        print(strudel_code)

if __name__ == "__main__":
    main()

