# WUG — Gegner-Design (Bibel, Exit-Stand 2026-10-30)
[Quelle: wip/WUG.md (Extrakt aus Checkpoint-Kette). Eine Zeile ergaenzt: HP-Wert
auf Batch-2a-Stand (12) gesetzt; die wip-Kopie war stale (HP 4).]

Vorlage: `ref/wug-test.png` (Berko 1958, *Wug Test*; lokal, nicht im Repo). Ein kleiner, runder
Vogel auf zwei duennen Beinen: rundlicher Koerper, ein Punktauge, spitzer Schnabel, Schwanz-Zipfel.
Gemischt mit den watschelnden/stehenden Gegnern aus Turrican (Walker mit Telegraph-Phase).

## Verhalten (Zustandsautomat, 50 Hz, Fixed-Point, deterministisch)

| Zustand | Dauer (Ticks) | Verhalten |
|---|---|---|
| `WUG_WADDLE` | 120-240 (RNG) | Watschelt Richtung Spieler (0,5 Einh./Tick), Koerper wippt (Sprite-Frame-Wechsel alle 8 Ticks) |
| `WUG_STOP` | 30 | Bleibt stehen, wendet sich dem Spieler zu, "schaut" |
| `WUG_CHARGE` | 60 (Telegraph) | **Augen gluehen rot**, Pulsieren (Palettenindex-Wechsel alle 4 Ticks), Spieler kann noch ausweichen/zeroerstören |
| `WUG_FIRE` | 40 | **Riesiger Laser** (horizontal, ~12 Einheiten lang, 3 Einh. dick), Schaden 25 + Knockback |
| `WUG_COOLDOWN` | 50 | Erschoepft, Augen dunkel; verwundbar (2x Schaden) -> zurueck zu `WAIT` |

Regel "wenn man nichts macht": Wird der Wug in `WUG_CHARGE` getroffen (Treffer-Timer) -> **Abbruch**
(Laser faellt aus, Stun 40 Ticks). Wer also aktiv schießt, ent schaerft ihn.

## Werte (final, Playtester-Beurteilung 2026-10-30: alle BEHALTEN)
- HP 12 (stirbt an 12 Pellets / 4 Laser-L1-Schuesse), Score 150, XP 20.
- Erscheint ab Welle 2 (Kampagne: ab Level 2), nie mehr als 2 gleichzeitig.
- Spawn: deterministisch ueber `core_rng`, wie die anderen Gegner (Oracle-Spiegelung Pflicht).

## Umsetzung (Reihenfolge)
1. `src/strudelforce.h`: `ENEMY_WUG = 3`, `Enemy` um `state`/`state_timer` erweitern (FERTIG, Batch 1).
2. `src/game_logic.c`: `wug_update()` in `enemy_update()`, Laser als eigene Gefahr-Box (FERTIG, Batch 1).
3. `verify_core_sim.py`: gleiche Logik spiegeln, Seed-Vergleich Endlos + Kampagne (FERTIG, 13/13 Paritaet).
4. `test_suite.c`: Zustandsuebergänge + Endless-Golden-Values (FERTIG, 43300/245/47833).
5. Sprites: [ABGEBROCHENER 16x16-PLAN] — User-Entscheidung: VECTOR/SPLINE-Route (2D, a la Elite),
   Vector-Kontur-Overlay fuer Charge + Effekte. FERTIG als draw_enemy_wug (Subagent), NICHT im ZIP.
6. Audio: Charge = steigender PWM-Ton (Cyber-Paula-Makro), Fire = Laser-Sweep + Sub-Bass-Hit (OFFEN).

## Optik
- Amiga: rotes Gluehen = Palettenindex-Wechsel, kein Textur-Look (Gesetz 2); Laser = Blitter-Linie + Copper-Farbflackern.
- PC: gleiche Sprite-Daten, zusaetzlich Bloom/Glow der Augen und Laser-Partikel.

## ZU ERGAENZEN (User-Auftrag, offen)
- Assembler-Section: M68k-Injektion nur an kritischen Hotspots (VS-Code-Toolchain lokal):
  (a) fix_mul 16.16 (~25 Cycles via muls vs 60-100 Libcall), (b) enemy_update (30-40 % Frame-Budget,
  8 Gegner), (c) proj_update Hit-Loop. Minimales ABI (in D0/D1/A0, out D0, clobber D0-D5/A0-A1/Y,
  kein Stack-Frame, leading underscore). Determinismus-Gate: #ifdef __mc68000__ + 256-Paar
  Golden-Vector-Tabelle aus Python-Oracle + 500-Frame-Checksumme byte-identisch PC-C vs Amiga-Asm.
  Erster Schritt: asm/fixmul.s.
- Frame-Decoupling: Gegner/Kollisionslogik @25 Hz (jedes 2. Frame), Spieler-Steuerung + Display @50 Hz.
