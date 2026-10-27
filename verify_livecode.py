#!/usr/bin/env python3
"""
verify_livecode.py — Headless-Verifikation der ESC-Live-Coding-Console (Meilenstein 4).
Gerettet aus Smith Primes Iteration-5-Entwurf.
"""
import os
import tempfile
import verify_headless as vh
import verify_core_sim as vcs

STEPS = 16
TICKS_PER_STEP = 5
DEFAULT_PATTERN = "c2 [c2 eb2] [f2 g2] <bb2 [g2 f2]>"
BATH_PATTERN = "<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>"

def compile_pattern(code):
    pat = {'nodes': [], 'root_idx': 0, 'raw_source': '', 'code': ''}
    vh.strudel_compile(pat, code, 0)
    return pat

class LiveCodeUI:
    def __init__(self, default_pattern):
        self.buffer = default_pattern
        self.cursor = len(default_pattern)
        self.active = compile_pattern(default_pattern)
        self.pending = None
        self.have_pending = 0
        self.tick = 0
        self.cycle_num = 0
        self.step_in_cycle = 0
        self.editor_open = 0
        self.chest_opened = 0
        self.biome = 0

    def step_tick(self):
        self.tick += 1
        if self.tick % TICKS_PER_STEP == 0:
            self.step_in_cycle += 1
            if self.step_in_cycle >= STEPS:
                self.step_in_cycle = 0
                self.cycle_num += 1
                if self.have_pending:
                    self.active = self.pending
                    self.have_pending = 0

    def current_note(self):
        return vh.strudel_query_step(self.active, self.cycle_num, self.step_in_cycle, STEPS)

    def edit_char(self, c):
        self.buffer = self.buffer[:self.cursor] + c + self.buffer[self.cursor:]
        self.cursor += 1

    def compile_pending(self):
        self.pending = compile_pattern(self.buffer)
        self.have_pending = 1

    def open_chest(self):
        if self.chest_opened:
            return
        self.chest_opened = 1
        self.biome = 1
        self.buffer = BATH_PATTERN
        self.cursor = len(BATH_PATTERN)
        self.active = compile_pattern(BATH_PATTERN)
        self.cycle_num = 0
        self.step_in_cycle = 0
        self.have_pending = 0

def main():
    failures = []
    lc = LiveCodeUI(DEFAULT_PATTERN)
    seq = []
    for _ in range(STEPS * 2):
        seq.append(lc.current_note())
        for _ in range(TICKS_PER_STEP):
            lc.step_tick()
    def fmt(n):
        return f"[{n}]" if n >= 0 else ("~" if n == -1 else ".")
    c0 = " ".join(fmt(seq[i]) for i in range(0, STEPS))
    c1 = " ".join(fmt(seq[i]) for i in range(STEPS, 2 * STEPS))
    exp_c0 = "[36] . . . [36] . [39] . [41] . [43] . [46] . . ."
    exp_c1 = "[36] . . . [36] . [39] . [41] . [43] . [43] . [41] ."
    print("=== STRUDELFORCE Live-Code Console Self-Test (Python-Oracle) ===")
    print(f"Zyklus 0: {c0}")
    print(f"Zyklus 1: {c1}")
    if c0 != exp_c0: failures.append(f"Zyklus0: {c0} != {exp_c0}")
    if c1 != exp_c1: failures.append(f"Zyklus1: {c1} != {exp_c1}")

    lc = LiveCodeUI(DEFAULT_PATTERN)
    lc.open_chest()
    first = lc.current_note()
    print(f"chest: biome={lc.biome} chest_opened={lc.chest_opened} erste_note={first} (erwartet 48=c3)")
    if lc.biome != 1 or not lc.chest_opened or first != 48:
        failures.append("chest-Logik fehlerhaft")

    print("livecode_check=" + ("OK" if not failures else "FEHLER: " + "; ".join(failures)))
    return 0 if not failures else 1

if __name__ == '__main__':
    raise SystemExit(main())

