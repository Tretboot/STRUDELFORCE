# Audio Integration Guide - STRUDELFORCE MASTER

## Quick Start

### 1. Initialize Audio System
Add to `engine_init()`:
```c
audio_manager_init();
```

### 2. Update Audio Every Frame
Add to `engine_update()`:
```c
audio_manager_update();
```

### 3. Shutdown Audio
Add to `engine_shutdown()`:
```c
audio_manager_shutdown();
```

### 4. Play Music in Levels
```c
// In level_load()
audio_play_level_music(level_id);

// In boss_spawn()
audio_play_boss_music();

// In game_over()
audio_play_game_over_music();

// In victory_screen()
audio_play_victory_music();
```

### 5. Play SFX in Game Logic
```c
// In player_update()
if (player_jumped) audio_play_jump();
if (player_shot) audio_play_shoot();

// In enemy_update()
if (enemy_hit) audio_play_enemy_hit();
if (enemy_died) audio_play_enemy_die();

// In game_logic_update()
if (powerup_collected) audio_play_powerup();
if (explosion) audio_play_explosion();
```

## Audio Settings Menu
```c
// Toggle music
audio_set_music_enabled(menu_music_toggle);

// Toggle SFX
audio_set_sfx_enabled(menu_sfx_toggle);

// Set volumes
audio_set_master_volume(master_volume);
audio_set_music_volume(music_volume);
audio_set_sfx_volume(sfx_volume);
```

## Best Practices

1. **Always call audio_manager_update()** every frame
2. **Check audio enabled** before playing critical SFX
3. **Stop current music** before starting new track
4. **Use appropriate volumes** for different SFX types
5. **Test on Amiga** for Paula DMA compatibility

## Troubleshooting

### No Sound
- Check `audio_is_music_enabled()` and `audio_is_sfx_enabled()`
- Verify `audio_manager_init()` was called
- Ensure `audio_manager_update()` is called every frame

### Distorted Audio
- Check Paula DMA channel limits (4 channels max)
- Verify sample rates match (22050 Hz)
- Ensure volumes are not too high (max 255)

### Music Not Looping
- Check track loop flag in TFMX header
- Verify pattern count and row count

## Performance Tips

1. **Pre-load tracks** during level load, not during gameplay
2. **Reuse SFX samples** instead of reloading
3. **Limit concurrent SFX** to 4 (Paula channels)
4. **Use lower volumes** for background SFX
5. **Disable audio** when not needed (pause menu)

---
*Integration Guide v1.0*
