# AGENT 5 SPEC: Amiga 68000 / ECS / AGA Backport (`src/amiga_hw.c`)

## Zielplattform
Kompilierbar mit `m68k-amigaos-gcc` (Bebbo Toolchain oder Bartman VSCode Amiga Extension) für Amiga 500 (1 MB RAM) und Amiga 1200.

## Hardware-Mapping des C89 Cores
- **Grafik (Voxel -> Bitplanes):** Ein Offline-Konverter rendert die Voxel-Sprites beim Build-Prozess in 32-Farben-Isometrie-Bitplanes (5 Bitplanes + Dual-Playfield oder Copper-Split für den Parallax-Himmel).
- **Audio (Paula `$DFF0A0` - `$DFF0DF`):**
  - Die `tfmx_macro_synth.h` läuft im **50Hz VBlank-Interrupt (`INTB_VERTB`)** und schreibt Perioden (`AUDxPER`), Lautstärke (`AUDxVOL`, `0..64`) und Sample-Pointer (`AUDxLCH/LCL`) direkt in die Paula-Register.
- **Der Amiga-Unterwasser-Trick (`$BFE001`):**
  - Sobald `state->biome == BIOME_UNDERWATER` aktiv ist, setzt der Amiga-Port Bit 1 im CIA-A Register (`*((volatile uint8_t*)0xBFE001) &= ~0x02;`). Dadurch schaltet der echte Amiga 500/1200 seinen **analogen 7kHz Hardware-Tiefpassfilter** ein (und ändert die Helligkeit der Power-LED)!
