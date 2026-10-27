#ifndef VOXEL_RENDER_H
#define VOXEL_RENDER_H

#include "sf_interfaces.h"

typedef struct {
    const char *name;
    int w;
    int h;
    const uint8_t *colors;
    const uint8_t *z_depth;
} SpriteDef;

extern const SpriteDef sprite_player;
extern const SpriteDef sprite_grunt;
extern const SpriteDef sprite_flyer;
extern const SpriteDef sprite_golem;
extern const SpriteDef sprite_chest;
extern const SpriteDef sprite_crystal;

void voxel_expose_sprites(void);

#endif /* VOXEL_RENDER_H */

