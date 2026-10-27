# ROADMAP_AUDIO — Audio-Roadmap STRUDELFORCE

> Stand: 2026-10-27 (post-Merge). Diese Roadmap dokumentiert den Ist-Zustand des
> Audio-Stacks und die nächsten Ausbaustufen. Alle P0-/P1-Steps sind bewusst so geschnitten,
> dass sie innerhalb der 3 Eisernen Gesetze (s. AGENTS.md) und der Amiga-Chip-RAM-Budgets
> (64KB Sample-RAM, 8 Paula-Voices) umsetzbar sind.

---

## 0. Architektur-Ist (nach Merge)

```text
Strudel (.strudel) ──► strudel_cleanroom (AST) ──► user_music / livecode
        │                                    │
        ▼ (tfmx2strudel / strudel2tfmx)      ▼
   TFMX 1.5/Pro/7V ◄────► tfmx_player.c (C89, Oracle-mirror)
   MOD (ProTracker) ◄────► mod_player.c   (C89, Oracle-mirror)
   Sample-Banken (8-bit) ◄► smpl-Bank (TFMX) / assets/samples
        │
        ▼
   sf_mixer.c (TFMX + MOD + Synth, Gain-Ramps, 44.1k→49.722k 50Hz-Raster)
        │  via GameAudio.ext_render (v3-Hook)
        ▼
   game_audio.c (8-Voice Paula-Mixer, TFMX-Makros, reaktive Matrix,
                 Low-HP-Lowpass, 4-Bit-Quantisierung, 16-Bit WAV-Export)
        │
        ├──► win_audio.c (WaveOut, real-time, --audio)
        └──► headless_dump (strudelforce_headless, 49722Hz/16bit WAV/JSON)
```

Zusätzlich (sf-Seite, Amiga/PC-Targets): `cyber_paula.c` (8-Stimmen-TFMX-Mixer mit
Paula-DMA-Feed) und `tfmx_macro_synth.h` (50Hz-Makros, geteilt von beiden Seiten).

### Abgeschlossen (v3 + Merge)
- [x] TFMX-Player **v2**: 1.5/Pro/7-Voice, `mdat`/`smpl`/`TFHD`/`TFMXPAK`, C89,
      deterministisch, bit-genauer Python-Mirror (`tfmx_oracle.py`, 396/396-Scan).
- [x] MOD-Player (ProTracker 4/6/8 Kanäle, C89, `mod_oracle.py`).
- [x] `sf_mixer` mit Gain-Ramps + `ext_render`-Hook in `game_audio`.
- [x] `.sid`-Header-Reader (`sid_header.c`, No-Playback).
- [x] 8-Bit-Samples in `assets/samples/`, `smpl`-Bank-Struktur im TFMX-Format.
- [x] Reaktive Audio-Matrix (Waffen/Schild/Low-HP) mit Python-Oracle (`verify_audio_matrix.py`).
- [x] Live-Coding → Zyklusrand-Swap (Smith) / `compile_swap` (sf).
- [x] `c_vs_oracle`: C- vs. Python-Referenzplayer bit-genau (8/8 Sample-Sets).

---

## 1. P0 — Nächstes Quartal (Stabilität & Abdeckung)

1. **TFMXPAK-Packer auf C-Seite**
   Der Reader existiert (`tfmx_player`), das Python-Pack-Tool (`kiwi_handoff.py pack`)
   auch. → Kleines `tfmx_pak_tool.c` (Header + deflater via eingebetteten LZ77-Substätt),
   damit Amiga-Targets Pakete *laden* können. Test via `tfmx_oracle --scan` + neuer
   `c_vs_oracle`-Set.
2. **Sample-Bank-End-to-End im Game**
   `smpl`-Bank parsing ist da; die Spiel-Events (Kristall, Truhe, Treffer) sollen per
   `ext_render` echte 8-Bit-Samples aus `assets/samples/` abspielen. Ziel: deterministisch
   reproduzierbare WAV-Dumps (`--audio`) mit Samples, Oracle-Abgleich erweitert.
3. **44.1k-Export-Option**
   `game_audio` rendert intern 49722Hz (50Hz-PAL). WAV-Export soll optional
   `--sample-rate 44100` mit 16-Bit-Lineresampling (nur im Dumper, nicht im Hot-Path)
   anbieten — wichtig für moderne Abspielfrontends.

## 2. P1 — Audio-Kern-Ausbau

4. **SID-Playback (Phase A: Replay-Engine)**
   `sid_header.c` liefert bereits Register-Event-Sequenzen. → Minimaler 6581-Replay-Player
   (PWM/Saw/Pulse/DAC-Sample) in C89 + Python-Oracle. *Kein* kompletter SID-Synth in der
   Spielzeit — Replay der originalen Register-Sequenzen reicht für die vorgegebenen Songs.
5. **Paula-DMA-Integration (Amiga-Target)**
   `amiga_hw.c` hat die DMA-Struktur. *(Status 2026-10-27: TFMX-Musik streamt bereits
   über Paula-Voice-0 (16574 Hz, 8-Bit, 331 Samples/VBlank) in der M68K-Sektion —
   5 Turrican-2-Tracks auf der ADF, Track-Switch per `M`.)* Offener Rest: die volle
   8-Stimmen-Matrix über echte Paula-DMA ($BFD000+), inkl. Sample-RAM-Map in
   Chip-RAM (≤64KB). Lint via `make amiga_lint`, Runtime-Test in WinUAE.
6. **Stereo/Positionierung (2.5D)**
   Layer-1-Entitäten (Gegner/Schüsse) erhalten pro Voice einen Pan-Wert (0..255),
   deterministisch aus X-Position abgeleitet. Im WAV-Dumper als zweiter Channel (16-bit
   Stereo) auslesbar; auf Amiga: Mono + Lautstärke-Täuschung (Paula hat kein Stereo).

## 3. P2 — Langfristig (Optik für die Demoszene)

7. **Effektstufe 2: Reverb + Chorus (8-Bit-Budget)**
   256-Byte Ringbuffer-Reverb (Schmidt) pro Bus, nur auf der Synth-Mixer-Ebene.
   Strudel-Makro `verb(0.3)` als optionales Attribut.
8. **Live-Coding → TFMX-Export-Rundreise schließen**
   `strudel2tfmx` (Python) existiert; → In-Game-F3-Export nach TFMX-Paar (`*.mdat`+`*.smpl`)
   mit deterministischem Header (TFHD), so dass jedes Live-Edit sofort als Modul-Asset
   gespeichert und vom TFMX-Player wieder abspielbar ist.
9. **Waffen-SFX-Kit (8-bit, Paula-kompatibel)**
   Pro Waffe 2–4 kurze Sample-Events (Spread/Laser/Bounce/Schild), im TFMX-`smpl`-Format,
   Oracle-verifiziert. Design-Constraint: ≤ 16KB Sample-RAM-Summe.
10. **Musik-Editor-UI in der GUI**
    `gui_main.c` hat Explorer + Studio; → Track-Selektion pro Level (`levels.json`
    `music_ref`), Lautstärken-Bus-Regler (persistiert via `user_music/presets.json`).

---

## 4. Regeln & Budget (verbindlich)

| Ressource | Budget | Status |
|---|---|---|
| Sample-RAM (Paula) | 64 KB Chip-RAM | P1-4/-9 müssen dieses Budget einhalten |
| Stimmen | 8 Paula-Voices | `sf_mixer`/`game_audio` schon 8-stimmig |
| Render-Rate | 49722 Hz (50Hz-PAL) im Core | 44.1k nur im Export-Dumper (P0-3) |
| C-Standard | C89, keine malloc im Hot-Path | P0/P1-Items entsprechend designen |
| Determinismus | bit-genau vs. Python-Oracle | jedes neue Modul bekommt einen Oracle-Mirror |

**Priorisierungslogik:** P0 = "funktioniert & ist messbar", P1 = "Amiga-Authentizität",
P2 = "Politur". Jedes Item wird erst dann als "done" gezählt, wenn sein Oracle-Mirror
(bit-genau) grüner als `tools/selftest.py` läuft.