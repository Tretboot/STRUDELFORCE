# STRUDELFORCE MASTER - Music System

## Overview
The music system for STRUDELFORCE MASTER is based on the Turrican 2 TFMX format, adapted for the Amiga 500 (68k) and PC (C89/C99) targets. It features:

1. **TFMX Player**: A modified MOD player that supports TFMX patterns and envelopes.
2. **Dynamic Music**: Music parameters change based on biome, enemy count, and weapon level.
3. **Super Saw Effect**: A synthesized "Super Saw" effect for thick lead sounds.
4. **Sample Bank**: A compact set of 16 samples inspired by Turrican 2.

## Files
- `src/mod_player.c`: MOD/TFMX player with BPM/timestretch support.
- `src/dynamic_music.c/h`: Dynamic music engine.
- `src/super_saw.h`: Super Saw macro for synthesized leads.
- `src/sample_bank.h`: Sample bank definitions.

## Dynamic Music Parameters
The `get_music_params()` function returns a `MusicParams` struct based on:
- **Biome ID**: Determines tempo and instrument set.
- **Enemy Count**: If > 10, tempo increases by 10 and distortion is added.
- **Weapon Level**: If > 2, Super Saw effect is enabled.

## Biome Mapping
| Biome ID | Name | Tempo | Instrument Set |
|----------|------|-------|----------------|
| 0 | Chrome Tempest | 130 | 0 |
| 1 | Great Bath | 110 | 1 |
| 2 | Mech Hive | 140 | 2 |
| 3 | Void Reach | 120 | 3 |
| 4 | Core Furnace | 150 | 4 |

## Super Saw Effect
The Super Saw effect is implemented as a macro in `super_saw.h`. It mixes three sawtooth oscillators with slight detune (+/- 5 cents) to create a thick, rich sound. This is useful for lead synths and can be enabled dynamically based on weapon level.

## Sample Bank
The sample bank contains 16 samples, including drums, bass, leads, pads, and FX. These are placeholders; actual sample data should be loaded from disk or embedded.

## Next Steps
1. Implement TFMX pattern decompression in `mod_player.c`.
2. Add sample data to `sample_bank.h`.
3. Integrate `dynamic_music.c` into the game loop.
4. Test Super Saw effect in the Amiga 500 target.
