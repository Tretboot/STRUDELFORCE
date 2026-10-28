# STRUDELFORCE MASTER - Items & Levelup System

## Overview
The Items & Levelup system allows the player to upgrade their armor and weapons by collecting Voxel Parts dropped by enemies.

## XP System
- XP is gained by defeating enemies.
- Level thresholds: 0 (0 XP), 1 (1000 XP), 2 (2500 XP), 3 (4500 XP), 4 (7000 XP), 5 (10000 XP), 6 (14000 XP), 7 (19000 XP), 8 (25000 XP).
- Max Level: 8.

## Voxel Parts (16 Total)
Each part has 3-4 tiers, improving stats with each tier.

### Legs (4 Tiers)
| Tier | Name | Speed | Jump | Defense |
|------|------|-------|------|---------|
| 1 | Light Legs | +10% | +5% | +2 |
| 2 | Medium Legs | +20% | +10% | +5 |
| 3 | Heavy Legs | +30% | +15% | +10 |
| 4 | Titan Legs | +40% | +20% | +15 |

### Arms (3 Tiers)
| Tier | Name | Fire Rate | Reload | Defense |
|------|------|-----------|--------|---------|
| 1 | Light Arms | +5% | +5% | +2 |
| 2 | Medium Arms | +10% | +10% | +5 |
| 3 | Heavy Arms | +15% | +15% | +10 |

### Weapons (3 Tiers)
| Tier | Name | Damage | Range | Special |
|------|------|--------|-------|---------|
| 1 | Plasma Blaster | +10% | +5% | None |
| 2 | Ion Cannon | +20% | +10% | Pierce |
| 3 | Quantum Rifle | +30% | +15% | Homing |

### Core (3 Tiers)
| Tier | Name | Health | Shield | Regen |
|------|------|--------|--------|-------|
| 1 | Basic Core | +10 HP | +5 SH | +0.1/s |
| 2 | Enhanced Core | +20 HP | +10 SH | +0.2/s |
| 3 | FORCE Core | +30 HP | +15 SH | +0.3/s |

### Special (2 Items)
- **Stealth Module**: Reduces enemy detection range by 50%.
- **Overdrive Chip**: Doubles fire rate for 5 seconds (cooldown: 30s).

## Loot Table
When an enemy dies, it drops a random Voxel Part based on rarity:
- **Common (60%)**: Tier 1 parts.
- **Rare (30%)**: Tier 2 parts.
- **Epic (9%)**: Tier 3 parts.
- **Legendary (1%)**: Tier 4 parts or Special items.

## Integration with Dynamic Music
- Equipping higher-tier weapons increases the `weapon_level` parameter in `get_music_params()`.
- This triggers the Super Saw effect in the music when `weapon_level > 2`.
- Boss fights automatically set `boss_active = 1`, increasing tempo and adding distortion.

## Next Steps
1. Implement XP tracking in `core_sim.c`.
2. Implement loot drop system in enemy AI.
3. Implement equip system in `player_voxel.c`.
4. Connect weapon level to dynamic music engine.
