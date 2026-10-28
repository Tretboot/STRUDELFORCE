# STRUDELFORCE MASTER - Implementation Plan

## Phase 1: Visual Overhaul & Animation Pipeline (CURRENT)

### 1.1 Sprite Generator (tools/gen_sprites.py)
- [x] Create sprite generator script
- [ ] Generate XPM sprite data for Player (6 frames)
- [ ] Generate XPM sprite data for Weapons (Plasma, Laser, Spread)
- [ ] Integrate sprite loading in engine

### 1.2 Animation System (src/engine_core.c)
- [x] Add anim_state_t to entity_t
- [x] Implement anim_init(), anim_update(), anim_get_frame()
- [ ] Implement actual sprite rendering (render_sprite())
- [ ] Couple animation frames with velocity

### 1.3 Sprite Rendering (src/render_sprites.c) - NEW FILE
- [ ] Parse XPM data
- [ ] Implement pixel-by-pixel rendering
- [ ] Handle transparency
- [ ] Integrate with main render loop

## Phase 2: Defensive Programming & Code Hygiene

### 2.1 NULL-Pointer & Boundary Checks
- [ ] Add NULL checks to all pointer dereferences
- [ ] Add array boundary checks
- [ ] Implement STRUDEL_ERROR macro

### 2.2 C89 Code Hygiene
- [ ] Move all variable declarations to scope start
- [ ] Replace magic numbers with #define constants
- [ ] Extract long functions into helpers

### 2.3 Test Suite Preparation
- [ ] Create tests/test_fixed_point.c
- [ ] Create tests/test_entity.c
- [ ] Create tests/test_collision.c

## Phase 3: Level System & TFMX Integration

### 3.1 Level Loading
- [ ] Parse levels/levels.json
- [ ] Build tilemaps from JSON
- [ ] Implement tile collision

### 3.2 TFMX Audio
- [ ] Load TFMX tracks from assets/TFMX Professional/
- [ ] Implement Paula DMA simulation
- [ ] Add sample playback for SFX

## Current Status: Phase 1.1-1.2 in Progress

## Next Immediate Actions:
1. Complete sprite rendering implementation
2. Run static analysis on current code
3. Prepare test suite structure
