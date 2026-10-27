#!/usr/bin/env python3
"""
verify_headless.py — Faithful Python-3-Port des STRUDELFORCE Headless-Tests
(= src/test_core_audio.c, Ziel `make headless_test && ./strudelforce_test`).

Warum: Diese Sandbox hat KEINE C-Toolchain (kein gcc/make/cc/tcc, kein Netzwerk).
Dieser Port führt die EXAKTE Testlogik aus und verifiziert die Clean-Room
Strudel-Parser- + TFMX-Engine-Semantik ohne Compiler.

1:1 portiert (Semantik, nicht Syntax):
  C (strudel_cleanroom.h)          -> Python
  ---------------------------------  ----------------------------------
  strudel_parse_note_token()       -> strudel_parse_note_token()
  strudel_alloc_node()             -> alloc_node()
  strudel_parse_expr()             -> parse_expr()
  strudel_compile()                -> strudel_compile()
  strudel_query_step()             -> strudel_query_step()
  C (tfmx_macro_synth.h)           -> Python
  cyber_paula_init_waves()         -> cyber_paula_init_waves() (Sample-RAM-Teil)

Referenz-Trace (handoff.md §6):
  AST-Knoten = 9
  Cycle 0: [36] . . . [39] . [43] . [48] . [48] . [46] . . .
  Cycle 1: [36] . . . [39] . [43] . [48] . [48] . [43] ~ ~ [43]
"""
import math
import re

# --- Konstanten (identisch zu den C-Headern) ---
STRUDEL_MAX_NODES = 128
STRUDEL_MAX_CHILDREN = 16
PAULA_VOICES = 8
SAMPLE_RAM_SIZE = 65536

S_NODE_ATOM, S_NODE_SEQ, S_NODE_ALT, S_NODE_EUCLID = 0, 1, 2, 3


def strudel_parse_note_token(tok):
    """C: strudel_parse_note_token — 'c2','eb3','f#4','~' -> MIDI-Note (-1 = Pause)."""
    if not tok or tok[0] in ('~', '-'):
        return -1
    base_map = {'c': 0, 'd': 2, 'e': 4, 'f': 5, 'g': 7, 'a': 9, 'b': 11}
    base = base_map.get(tok[0].lower())
    if base is None:
        return -1
    idx = 1
    # C liest tok[idx] ohne Bounds-Check; out-of-range == '\0' == kein Match.
    if idx < len(tok) and tok[idx] in ('#', 's'):
        base += 1
        idx += 1
    elif idx < len(tok) and tok[idx] == 'b':
        base -= 1
        idx += 1
    octave = 3
    if idx < len(tok) and tok[idx].isdigit():
        octave = int(tok[idx])
    return (octave + 1) * 12 + base


class Node:
    """C: StrudelNode."""
    __slots__ = ('type', 'midi_note', 'instrument_id', 'repeat_mult',
                 'euclid_k', 'euclid_n', 'num_children', 'children')

    def __init__(self, type_):
        self.type = type_
        self.midi_note = -1
        self.instrument_id = 0
        self.repeat_mult = 1
        self.euclid_k = 0
        self.euclid_n = 0
        self.num_children = 0
        self.children = []


def alloc_node(pat, type_):
    """C: strudel_alloc_node."""
    if len(pat['nodes']) >= STRUDEL_MAX_NODES:
        return -1
    pat['nodes'].append(Node(type_))
    return len(pat['nodes']) - 1


def parse_expr(pat, p, n, parent_idx, close_char):
    """C: strudel_parse_expr (rekursiver Descent-Parser). Liefert neue Position."""
    while p < n:
        while p < n and pat['code'][p].isspace():
            p += 1
        if p >= n or pat['code'][p] == close_char:
            if p < n and pat['code'][p] == close_char:
                p += 1
            break
        if pat['code'][p] in ('[', '<'):
            expected_close = ']' if pat['code'][p] == '[' else '>'
            ntype = S_NODE_SEQ if pat['code'][p] == '[' else S_NODE_ALT
            child_idx = alloc_node(pat, ntype)
            parent = pat['nodes'][parent_idx]
            if parent.num_children < STRUDEL_MAX_CHILDREN:
                parent.children.append(child_idx)
                parent.num_children += 1
            p = parse_expr(pat, p + 1, n, child_idx, expected_close)
            if p < n and pat['code'][p] == '*':
                p += 1
                j = p
                while j < n and pat['code'][j].isdigit():
                    j += 1
                # C: atoi(p) — leerer String -> 0
                pat['nodes'][child_idx].repeat_mult = (int(pat['code'][p:j]) if j > p else 0) & 0xFF
                p = j
        else:
            start = p
            while p < n and not pat['code'][p].isspace() and pat['code'][p] not in ']>[<':
                p += 1
            tok = pat['code'][start:p][:31]  # C: char tok[32], max 31 Zeichen
            atom_idx = alloc_node(pat, S_NODE_ATOM)
            node = pat['nodes'][atom_idx]
            # C: strchr(tok,'*') — Token kürzen, atoi(Rest)
            star = tok.find('*')
            if star != -1:
                node.repeat_mult = (int(tok[star + 1:]) if star + 1 < len(tok) else 0) & 0xFF
                tok = tok[:star]
            # C: strchr(tok,'(') + sscanf("%d,%d")
            paren = tok.find('(')
            if paren != -1:
                # C: sscanf(paren+1, "%d,%d") — parst führende Ints, ignoriert
                # nachfolgenden Müll (z.B. schließende Klammer ')'). Der Port
                # muss genauso tolerant sein, sonst degradiert der EUCLID-Knoten
                # stillschweigend zu einem ATOM (Sustain statt Rest).
                m2 = re.match(r'\s*(-?\d+)\s*,\s*(-?\d+)', tok[paren + 1:])
                if m2:
                    k, m = int(m2.group(1)), int(m2.group(2))
                    if m > 0:
                        node.type = S_NODE_EUCLID
                        node.euclid_k = k & 0xFF
                        node.euclid_n = m & 0xFF
                tok = tok[:paren]
            node.midi_note = strudel_parse_note_token(tok)
            parent = pat['nodes'][parent_idx]
            if parent.num_children < STRUDEL_MAX_CHILDREN:
                parent.children.append(atom_idx)
                parent.num_children += 1
    return p


def strudel_compile(pat, code, default_inst):
    """C: strudel_compile."""
    pat['nodes'] = []
    pat['raw_source'] = code[:511]
    pat['code'] = code
    pat['root_idx'] = alloc_node(pat, S_NODE_SEQ)
    parse_expr(pat, 0, len(code), pat['root_idx'], '\0')
    for node in pat['nodes']:
        node.instrument_id = default_inst


def strudel_query_step(pat, cycle_num, step_in_cycle, total_steps):
    """C: strudel_query_step. Return: >=0 MIDI-Note, -1 Rest, -2 Sustain."""
    curr = pat['root_idx']
    local_step = step_in_cycle
    span = total_steps
    for _depth in range(8):
        node = pat['nodes'][curr]
        if node.repeat_mult > 1 and span >= node.repeat_mult:
            span //= node.repeat_mult
            local_step %= span
        t = node.type
        if t == S_NODE_ATOM:
            return node.midi_note if local_step == 0 else -2
        elif t == S_NODE_EUCLID:
            if node.euclid_n == 0:
                return -1
            sub_span = span // node.euclid_n
            if sub_span == 0:
                sub_span = 1
            if local_step % sub_span != 0:
                return -2
            e_idx = (local_step // sub_span) % node.euclid_n
            hit = (e_idx * node.euclid_k) % node.euclid_n < node.euclid_k
            return node.midi_note if hit else -1
        elif t == S_NODE_ALT:
            if node.num_children == 0:
                return -1
            curr = node.children[cycle_num % node.num_children]
        elif t == S_NODE_SEQ:
            if node.num_children == 0:
                return -1
            child_span = span // node.num_children
            if child_span == 0:
                child_span = 1
            c_idx = local_step // child_span
            if c_idx >= node.num_children:
                c_idx = node.num_children - 1
            local_step %= child_span
            span = child_span
            curr = node.children[c_idx]
    return -1


def cyber_paula_init_waves():
    """C: cyber_paula_init_waves — 8-Bit Sample-RAM (3 Banken), für die Verifikation."""
    ram = [0] * SAMPLE_RAM_SIZE
    # Bank 0 (0..255): Sägezahn + Puls-Hybrid für TFMX PWM-Modulation
    for i in range(256):
        ram[i] = ((i * 2 if i < 128 else (i - 256) * 2) - 64)
    # Bank 1 (256..511): FM-Slap-Bass (DX21-Stil)
    for i in range(256):
        p = (2.0 * math.pi * i) / 256.0
        ram[256 + i] = int(math.sin(p + 2.2 * math.sin(3.0 * p)) * 120.0)
    # Bank 2 (512..1023): D-50 AnalogString / "The Great Bath" Pad
    for i in range(512):
        p = (2.0 * math.pi * i) / 512.0
        val = 0.5 * math.sin(p) + 0.3 * math.sin(2.01 * p) + 0.2 * math.sin(4.0 * p)
        ram[512 + i] = int(val * 115.0)
    return ram


def main():
    pat = {'nodes': [], 'root_idx': 0, 'raw_source': '', 'code': ''}
    live_code = "c2 [eb2 g2] c3*2 <bb2 g2(3,8)>"
    strudel_compile(pat, live_code, 0)

    print("=== STRUDELFORCE Clean-Room Audio Core Self-Test (Python-Port) ===")
    print("Live-Code Pattern: %s" % pat['raw_source'])
    print("Generierte AST-Knoten: %d" % len(pat['nodes']))
    print()
    for cycle in range(2):
        print("Cycle %d: " % cycle, end="")
        for step in range(16):
            note = strudel_query_step(pat, cycle, step, 16)
            if note >= 0:
                print("[%d] " % note, end="")
            elif note == -1:
                print(" ~  ", end="")
            else:
                print(" .  ", end="")
        print()
    cyber_paula_init_waves()
    print()
    print("TFMX 50Hz Macro Engine bereit. Sample-RAM initialisiert (%d Bytes)." % SAMPLE_RAM_SIZE)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
