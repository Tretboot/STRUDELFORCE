# STRUDELFORCE — todo.md (Priorisierte Arbeitspakete)

> Legende: `[ ]` offen · `[~]` aktiv · `[x]` erledigt
> Stand: Iteration 1 (Bootstrap + Umgebungs-Audit)

## Phase 0 — Projekt-Initialisierung
- [x] Altsystem-Artefakte im übergeordneten Verzeichnis auditieren (nur Bootstrap-Skript `gemini-code-1793055820039.py` gefunden; keine separaten .txt-Logs / .c-Teilergebnisse)
- [x] Bootstrap-Skript ausführen → Projekt-Verzeichnis erzeugt (`STRUDELFORCE_Kit` → `STRUDELFORCE_Smith_Core` renamed)
- [x] Drei zentrale Steuerdateien anlegen (todo.md, handoff.md, captains_log.md)

## Phase 1 — Headless-Verifikation des C-Audio-Kerns
- [x] Toolchain-Gap bestätigen (kein gcc/make/cc/tcc/clang in der Sandbox, kein Netzwerk)
- [x] Python-Port `verify_headless.py` des Headless-Tests (Strudel-Parser + TFMX-Engine) bauen und ausführen
- [x] `BUILD.md` mit sauberen Kompilier-Anweisungen (gcc/make, PC + Amiga-Dual-Target) schreiben
- [x] Test-Output gegen Referenz-Trace (AST=9, Cycle-Grid) verifizieren — **ALLE OK** (inkl. EUCLID-sscanf-Fix im Port, s. captains_log.md Iteration 2)

## Phase 2 — Clean-Room C89/C99 Kern ausbauen
- [x] `src/core_sim.h/.c` (Fixed-Point Physik, Waffen, Bounce 2.0, statische Ringbuffer, 50Hz `core_tick`) — Meilenstein 2
- [x] `src/game_main.c` (Terminal-Frontend, 50Hz, WASD/Z/X/C, `--demo`-Modus) — spielbar
- [x] Python-Oracle `verify_core_sim.py` (1:1-Port, Determinismus- + Invarianten-Checks) — ALLE OK
- [x] Meeseeks #2 Review (inline, Hintergrund): BOUNCE_DIAG_FP 278000→278046 fixt
- [x] C89/C99-Striktheit: kein float/double, kein malloc im Core (grep-verifiziert)
- [ ] C-Build auf echter Maschine: `make game && ./strudelforce_game` (hier nicht testbar, s. WORKFLOW_ESSAY.md)

## Phase 3 — Audio-Engine vertiefen
- [x] Reaktive Gameplay-Patch-Matrix (`game_audio_update`, Waffe→Macro/Arp, Schild→PWM, HP→Cutoff/Quant, SFX-Diffs) — Meilenstein 3, Oracle-OK
- [x] 8-Stimmen-Mixer mit 1-Pole-Low-Pass (16.16) + 4-Bit-Quantisizer (HP<25%)
- [x] `--demo N --audio out.wav`: deterministischer 16-Bit-WAV-Dump (hörbare C-Verifikation auf echter Maschine)
- [ ] Biquad/SVF-Filter in Fixed-Point (Aktuell: 1-Pole; SVF nur im Altsystem-Header)
- [ ] .MOD-Loader (pocketmod) + .strudel/.tfmx BYO-Loader anbinden

## Phase 4 — Pakete & Lieferung
- [x] Alles in `STRUDELFORCE_Smith_Core.zip` packen (Iteration 3: inkl. core_sim, game_main, Essay, Oracle; s. handoff.md)
- [x] Finale Übergabe an den User (Code + Kompilier-Hinweis + Zip)
- [x] **Workflow-Essay** (`WORKFLOW_ESSAY.md`) mit A/B-Vergleichen, Iteration-4-Audio-Mustern & Persistenz-Protokoll
- [ ] **Nächste Iteration:** Voxel-Renderer (AGENT_3-Spec, `voxel_extrude_sprite`) oder ESC-Live-Coding-Overlay (AGENT_4-Spec)
- [ ] EUCLID-Semantik-Feinschliff: Degenerations-Fall `sub_span = span/euclid_n → 0 → 1` bewusst machen (s. captains_log.md Stolperstein #3)
