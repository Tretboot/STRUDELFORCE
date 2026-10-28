/* sf_interfaces.h - unified shim */
#ifndef SF_INTERFACES_H
#define SF_INTERFACES_H
#include "strudelforce.h"

typedef struct {
    int16_t  hp;
    int16_t  shield;
    uint8_t  level;
    int32_t  xp;
    uint8_t  weapon;
    uint8_t  wpn_lvl;
    fix16_t  x, y;
    fix16_t  vx, vy;
    int8_t   facing;
    uint8_t  on_ground;
    uint8_t  morph;
    fix16_t  beam_angle;
    uint8_t  buff_laser_w;
    uint8_t  buff_shield_res;
    uint8_t  buff_drone;
} SFPlayer;

typedef struct {
    uint8_t  active;
    uint8_t  kind;
    fix16_t  x, y;
    fix16_t  vx, vy;
    fix16_t  w, h;
    int16_t  hp;
    uint8_t  shatter;
    fix16_t  base_y;
    uint16_t fire_cd;
} SFEnemy;

typedef struct {
    uint8_t  active;
    uint8_t  etype;
    uint8_t  wpn;
    uint8_t  lvl;
    fix16_t  x, y;
    fix16_t  vx, vy;
    fix16_t  w, h;
    int16_t  dmg;
    uint16_t ttl;
    uint8_t  bounces;
    uint8_t  pal;
} SFEntity;

#define SF_ETYPE_BULLET  1
#define SF_ETYPE_LASER   2
#define SF_ETYPE_DEBRIS  3
#define SF_ETYPE_CRYSTAL 4

#endif
