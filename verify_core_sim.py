#!/usr/bin/env python3
"""
verify_core_sim.py — Python-Port (Oracle) des STRUDELFORCE Gameplay-Cores.

Portiert 1:1 die Logik von src/core_sim.c (16.16 Fixed-Point emuliert mit
ganzzahligen Python-Ints). Dient als Headless-Verifikation, weil diese Sandbox
KEINE C-Toolchain hat (kein gcc/make, kein Netzwerk).

WICHTIG: Python-Ints sind beliebig groesst — das emuliert das 64-bit
int64-Zwischenstadium von fix16_mul. Fuer 32-bit-Ueberlauf-Tests gibt es
separat verify_overflow.py.

Referenz: src/core_sim.c (Meilenstein 2). Aenderungen an core_sim.c MUESSEN
hier synchronisiert werden, sonst verliert das Oracle seine Gueltigkeit.
"""

# ================= 16.16 Konstanten (Wert * 65536, ALLE int!) =================
WORLD_W_UNITS = 72
WORLD_H_UNITS = 30

GRAVITY_FP = int(0.45 * 65536)
MAX_FALL_FP = int(8.0 * 65536)
JUMP_V_FP = int(2.6 * 65536)
MOVE_ACC_FP = int(0.25 * 65536)
MAX_SPEED_FP = int(2.4 * 65536)
FRICTION_FP = int(0.80 * 65536)
STOP_EPSILON = int(0.02 * 65536)

PLAYER_HALF_W = 1 * 65536
PLAYER_HALF_H = 1 * 65536
MORPH_HALF_W = 32768

SPREAD_COOLDOWN = 12
LASER_COOLDOWN = 8
BOUNCE_COOLDOWN = 15
BEAM_DAMAGE_TICK = 4
WAVE_INTRO_TICKS = 100
SHIELD_REGEN_TICK = 10
MAX_BOUNCES = 4

PELLET_SPEED_FP = 8 * 65536
BOUNCE_SPEED_FP = 6 * 65536
BOUNCE_DIAG_FP = 278046  # 6*cos(45deg)*65536 = 278045.69 -> 278046 (Review-Fund Meeseeks #2)

DIR8_DX = [6 * 65536, BOUNCE_DIAG_FP, 0, -BOUNCE_DIAG_FP,
           -6 * 65536, -BOUNCE_DIAG_FP, 0, BOUNCE_DIAG_FP]
DIR8_DY = [0, BOUNCE_DIAG_FP, 6 * 65536, BOUNCE_DIAG_FP,
           0, -BOUNCE_DIAG_FP, -6 * 65536, -BOUNCE_DIAG_FP]

SPREAD_FAN = [0, 2 * 65536, 3 * 65536, 6 * 65536,
              -2 * 65536, -3 * 65536, -6 * 65536]
FAN_IDX = [
    [0, 0, 0, 0, 0, 0, 0],
    [1, 4, 0, 0, 0, 0, 0],
    [1, 0, 4, 0, 0, 0, 0],
    [2, 1, 0, 4, 5, 0, 0],
    [3, 2, 1, 0, 4, 5, 6],
]
SPREAD_COUNTS = [1, 2, 3, 5, 7]
LASER_THICKNESS = [2, 4, 8, 14, 22]
LASER_LENGTH = [24, 36, 48, 60, 72]

MAX_PROJECTILES = 64
MAX_ENEMIES = 32
MAX_PARTICLES = 128
MAX_PICKUPS = 8

START_LIVES = 3
MAX_LIVES = 9
RESPAWN_INVULN_TICKS = 100

WEAPON_SPREAD, WEAPON_LASER, WEAPON_BOUNCE = 0, 1, 2
STATE_WAVE_INTRO, STATE_PLAYING, STATE_GAMEOVER, STATE_LEVEL_CLEAR, STATE_VICTORY = 0, 1, 2, 3, 4

# Spielmodes (muss 1:1 zu core_sim.h bleiben!)
MODE_ENDLESS, MODE_CAMPAIGN = 0, 1
CAMPAIGN_LEVELS = 5
CAMPAIGN_WAVES_PER_LEVEL = 3
LEVEL_CLEAR_TICKS = 150
PROJ_PELLET, PROJ_BOUNCE, PROJ_ENEMY = 0, 1, 2
ENEMY_WALKER, ENEMY_SHOOTER, ENEMY_TANK = 0, 1, 2
ENEMY_WUG = 3  # v0.3.2: Wug (spiegelt src/strudelforce.h)


def fix16_from_int(v):
    return v << 16


def fix16_to_int(v):
    # C: (v + 0x8000) >> 16  (arithmetischer Shift, negative Werte runden zur Unendlichkeit)
    return (v + 0x8000) >> 16


def fix16_mul(a, b):
    # C: (int64)a * (int64)b >> 16 — arithmetischer Shift
    return (a * b) >> 16


def dir8_from_vec(dx, dy):
    if dx >= 0:
        if dy >= 0:
            return 0 if dx >= dy else 1
        return 7 if dx >= -dy else 6
    if dy >= 0:
        return 4 if -dx >= dy else 3
    return 5 if -dx >= -dy else 6


class GameState:
    def __init__(self, seed):
        self.px = fix16_from_int(WORLD_W_UNITS // 2)
        self.py = 0
        self.pvx = 0
        self.pvy = 0
        self.on_ground = True
        self.facing = 1
        self.morphing = 0
        self.lives = START_LIVES
        self.respawn_timer = 0
        self.hp = 100
        self.max_hp = 100
        self.shield = 50
        self.xp = 0
        self.level = 1
        self.weapon = WEAPON_SPREAD
        self.weapon_level = 1
        self.fire_cooldown = 0
        self.beam_active = 0
        self.beam_angle = 0
        self.world_w = fix16_from_int(WORLD_W_UNITS)
        self.world_h = fix16_from_int(WORLD_H_UNITS)
        self.tick = 0
        self.score = 0
        self.wave = 0
        self.state = STATE_WAVE_INTRO
        self.state_timer = WAVE_INTRO_TICKS
        self.mode = MODE_ENDLESS
        self.campaign_level = 1
        self.projectiles = [None] * MAX_PROJECTILES
        self.enemies = [None] * MAX_ENEMIES
        self.particles = [None] * MAX_PARTICLES
        self.pickups = [None] * MAX_PICKUPS
        self.proj_head = 0
        self.enemy_head = 0
        self.part_head = 0
        self.rng_state = seed if seed else 1
        self.shots_fired = 0
        self.wug_headbutts = 0
        self.wug_two_dialog = 0
        self.enemies_killed = 0
        self.bounces_total = 0
        self.laser_len = 0
        self.laser_dir = 1
        self.laser_half = 0

    # --- PRNG (LCG, exakt wie C) ---
    def rng(self):
        self.rng_state = (self.rng_state * 1664525 + 1013904223) & 0xFFFFFFFF
        return self.rng_state


class Projectile:
    __slots__ = ('x', 'y', 'vx', 'vy', 'active', 'kind', 'level',
                 'bounces', 'life', 'damage')


class Enemy:
    __slots__ = ('x', 'y', 'vx', 'active', 'kind', 'hp', 'score_val',
                 'xp_val', 'hit_timer', 'shoot_timer', 'state', 'state_timer', 'facing')

# v0.3.2: Wug-Zustandsautomat (spiegelt src/strudelforce.h 1:1)
WUG_ST_WADDLE = 0
WUG_ST_STOP = 1
WUG_ST_CHARGE = 2
WUG_ST_FIRE = 3
WUG_ST_COOLDOWN = 4
WUG_ST_FLEE = 5
WUG_ST_DYING = 6
WUG_FLEE_SPEED = int(0.75 * 65536)


class Particle:
    __slots__ = ('x', 'y', 'vx', 'vy', 'active', 'color', 'life')


class Pickup:
    __slots__ = ('x', 'y', 'active')


def core_campaign_biome(s):
    # v0.3.2: spiegelt C 1:1 (src/game_logic.c: uint8_t core_campaign_biome(const GameState *s))
    if s.mode == MODE_CAMPAIGN and 1 <= s.campaign_level <= CAMPAIGN_LEVELS:
        return s.campaign_level - 1
    return 0


def proj_spawn(s, x, y, vx, vy, kind, level, damage, life):
    p = s.projectiles[s.proj_head]
    if p is None:
        p = Projectile()
        s.projectiles[s.proj_head] = p
    s.proj_head = (s.proj_head + 1) % MAX_PROJECTILES
    p.x, p.y, p.vx, p.vy = x, y, vx, vy
    p.active, p.kind, p.level = 1, kind, level
    p.bounces, p.life, p.damage = 0, life, damage


def enemy_spawn(s, kind, x):
    e = s.enemies[s.enemy_head]
    if e is None:
        e = Enemy()
        s.enemies[s.enemy_head] = e
    s.enemy_head = (s.enemy_head + 1) % MAX_ENEMIES
    e.x, e.y, e.vx = x, 0, 0
    e.active, e.kind = 1, kind
    e.hit_timer, e.shoot_timer = 0, 40
    # Balancing (2026-10-27): Grunts/Shooter = 1 Treffer, Tank (Mini-Boss) = 15
    e.state, e.state_timer, e.facing = 0, 0, 1
    if kind == ENEMY_WALKER:
        e.hp, e.score_val, e.xp_val = 1, 100, 10
    elif kind == ENEMY_SHOOTER:
        e.hp, e.score_val, e.xp_val = 1, 250, 20
    elif kind == ENEMY_WUG:
        e.hp, e.score_val, e.xp_val = 12, 150, 20  # v0.3.2 Balancing-Iter 2a: 4->12
        e.state = WUG_ST_WADDLE
        e.state_timer = 120 + (s.rng() & 119)
        e.facing = -1 if e.x > s.px else 1
    else:
        e.hp, e.score_val, e.xp_val = 15, 500, 40


def part_burst(s, x, y, color, n):
    for _ in range(n):
        pt = s.particles[s.part_head]
        if pt is None:
            pt = Particle()
            s.particles[s.part_head] = pt
        s.part_head = (s.part_head + 1) % MAX_PARTICLES
        pt.x, pt.y = x, y
        pt.vx = ((s.rng() & 0xFFFF) - 32768) * 4
        pt.vy = ((s.rng() & 0xFFFF) - 32768) * 4 + 65536
        pt.active, pt.color = 1, color
        pt.life = 15 + (s.rng() & 15)


def wave_pickup_spawn(s):
    # 2 Extraleben pro Welle an den Welt-Raendern (deterministisch, kein RNG)
    for i in range(MAX_PICKUPS):
        s.pickups[i] = None
    p0 = Pickup()
    p0.x, p0.y, p0.active = fix16_from_int(3), 0, 1
    s.pickups[0] = p0
    p1 = Pickup()
    p1.x, p1.y, p1.active = s.world_w - fix16_from_int(3), 0, 1
    s.pickups[1] = p1


def pickup_update(s):
    for p in s.pickups:
        if p is None or not p.active:
            continue
        dx = s.px - p.x
        adx = -dx if dx < 0 else dx
        dy = s.py - p.y
        ady = -dy if dy < 0 else dy
        if adx < 2 * 65536 and ady < 3 * 65536:
            p.active = 0
            if s.lives < MAX_LIVES:
                s.lives += 1
            s.score += 250
            part_burst(s, p.x, p.y + 65536, 6, 8)


def wave_spawn(s):
    n = 3 + 2 * s.wave
    for i in range(n):
        kind = ENEMY_WALKER
        if s.wave >= 2 and i % 3 == 0:
            kind = ENEMY_SHOOTER
        if s.wave >= 3 and i == n - 1:
            kind = ENEMY_TANK
        enemy_spawn(s, kind, fix16_from_int(1) if i % 2 == 0 else s.world_w - fix16_from_int(1))
    # v0.3.2: Wug-Wellen-Spawn (max 2, ab Welle 2, Biome-Gate)
    if s.wave >= 2:
        wug_active = sum(1 for e in s.enemies if e is not None and e.active and e.kind == ENEMY_WUG)
        spawn_wug = True
        if s.mode == MODE_CAMPAIGN:
            b = core_campaign_biome(s)
            spawn_wug = (b != 1)
        if spawn_wug:
            while wug_active < 2:
                enemy_spawn(s, ENEMY_WUG, s.world_w - fix16_from_int(1) if s.px < s.world_w // 2 else fix16_from_int(1))
                wug_active += 1
    wave_pickup_spawn(s)


def player_respawn(s):
    s.px = fix16_from_int(WORLD_W_UNITS // 2)
    s.py = 0
    s.pvx = 0
    s.pvy = 0
    s.on_ground = True
    s.hp = s.max_hp
    s.shield = 100
    s.respawn_timer = RESPAWN_INVULN_TICKS
    for p in s.projectiles:
        if p is not None and p.active and p.kind == PROJ_ENEMY:
            p.active = 0


def player_hurt(s, dmg):
    if s.morphing:
        return
    if s.respawn_timer > 0:
        return
    if s.shield > 0:
        if s.shield >= dmg:
            s.shield -= dmg
            return
        dmg -= s.shield
        s.shield = 0
    s.hp = max(0, s.hp - dmg)
    if s.hp == 0:
        if s.lives > 1:
            s.lives -= 1
            player_respawn(s)
        else:
            s.state = STATE_GAMEOVER


def player_fire(s):
    if s.weapon == WEAPON_SPREAD:
        lvl = s.weapon_level - 1
        n = SPREAD_COUNTS[lvl]
        s.fire_cooldown = SPREAD_COOLDOWN
        s.shots_fired += n
        for i in range(n):
            dy = SPREAD_FAN[FAN_IDX[lvl][i]]
            proj_spawn(s, s.px + (PLAYER_HALF_W if s.facing > 0 else -PLAYER_HALF_W),
                       s.py + 65536,
                       PELLET_SPEED_FP if s.facing > 0 else -PELLET_SPEED_FP, dy,
                       PROJ_PELLET, s.weapon_level, 1, 120)
    elif s.weapon == WEAPON_LASER:
        lvl = s.weapon_level - 1
        length = LASER_LENGTH[lvl]
        half_band = LASER_THICKNESS[lvl] // 2
        dmg = 2 + s.weapon_level
        pierce = s.weapon_level >= 3
        hit_list = []
        nearest = -1
        nearest_d = 0x7FFFFFFF
        for i, e in enumerate(s.enemies):
            if e is None or not e.active:
                continue
            dx = e.x - s.px
            if (dx < 0) if s.facing > 0 else (dx > 0):
                continue
            adx = -dx if dx < 0 else dx
            if adx > fix16_from_int(length):
                continue
            dy = e.y - s.py
            ady = -dy if dy < 0 else dy
            if ady > fix16_from_int(half_band + 1):
                continue
            hit_list.append(i)
            if adx < nearest_d:
                nearest_d = adx
                nearest = i
        s.fire_cooldown = LASER_COOLDOWN
        s.shots_fired += 1
        s.laser_len = fix16_from_int(length)
        s.laser_dir = s.facing
        s.laser_half = half_band
        if pierce:
            for i in hit_list:
                e = s.enemies[i]
                enemy_damage(s, e, dmg)
                part_burst(s, e.x, e.y + 65536, 3, 4)
        elif nearest >= 0:
            e = s.enemies[nearest]
            enemy_damage(s, e, dmg)
            part_burst(s, e.x, e.y + 65536, 3, 4)
    else:  # WEAPON_BOUNCE
        lvl = s.weapon_level
        fx = BOUNCE_SPEED_FP if s.facing > 0 else -BOUNCE_SPEED_FP
        x0 = s.px + (PLAYER_HALF_W if s.facing > 0 else -PLAYER_HALF_W)
        y0 = s.py + 65536
        s.fire_cooldown = BOUNCE_COOLDOWN
        s.shots_fired += 1
        proj_spawn(s, x0, y0,
                   BOUNCE_DIAG_FP if s.facing > 0 else -BOUNCE_DIAG_FP, BOUNCE_DIAG_FP,
                   PROJ_BOUNCE, s.weapon_level, 2, 200)
        if lvl >= 2:
            proj_spawn(s, x0, y0, fx, 0, PROJ_BOUNCE, s.weapon_level, 2, 200)
        if lvl >= 3:
            proj_spawn(s, x0, y0, 0, BOUNCE_SPEED_FP, PROJ_BOUNCE, s.weapon_level, 2, 200)
            proj_spawn(s, x0, y0, 0, -BOUNCE_SPEED_FP, PROJ_BOUNCE, s.weapon_level, 2, 200)


def bounce_hit_wall(s, p):
    p.bounces += 1
    s.bounces_total += 1
    part_burst(s, p.x, p.y, 5, 3)
    if p.bounces > MAX_BOUNCES:
        p.active = 0
        return
    if p.level >= 5:
        best = -1
        best_d = 0x7FFFFFFF
        for i, e in enumerate(s.enemies):
            if e is None or not e.active:
                continue
            dx = e.x - p.x
            adx = -dx if dx < 0 else dx
            dy = e.y - p.y
            ady = -dy if dy < 0 else dy
            if adx + ady < best_d:
                best_d = adx + ady
                best = i
        if best >= 0:
            e = s.enemies[best]
            d8 = dir8_from_vec(e.x - p.x, e.y - p.y)
            p.vx, p.vy = DIR8_DX[d8], DIR8_DY[d8]
    elif p.level >= 4:
        svx, svy = p.vx, p.vy
        p.vx, p.vy = svy, svx
        proj_spawn(s, p.x, p.y, svx, svy, PROJ_BOUNCE, p.level, p.damage, p.life)
    elif p.level == 1:
        p.vx = BOUNCE_DIAG_FP if p.vx > 0 else (-BOUNCE_DIAG_FP if p.vx < 0 else p.vx)
        p.vy = BOUNCE_DIAG_FP if p.vy > 0 else (-BOUNCE_DIAG_FP if p.vy < 0 else p.vy)


def proj_update(s):
    for p in s.projectiles:
        if p is None or not p.active:
            continue
        p.x += p.vx
        p.y += p.vy
        # Wandabprall: beide Achsen erst vollstaendig klammern, dann Bounce
        # EINMAL pro Tick (Bugfix 2026-10-27: Eckentreffer-Endlossplit)
        wall = 0
        if p.x <= 0:
            p.x = 0
            p.vx = -p.vx
            wall = 1
        elif p.x >= s.world_w:
            p.x = s.world_w
            p.vx = -p.vx
            wall = 1
        if p.y <= 0:
            p.y = 0
            p.vy = -p.vy
            wall = wall | 2
        elif p.y >= s.world_h:
            p.y = s.world_h
            p.vy = -p.vy
            wall = wall | 2
        if wall and p.kind == PROJ_BOUNCE:
            bounce_hit_wall(s, p)
        if not p.active:
            continue
        p.life -= 1
        if p.life == 0:
            p.active = 0
            part_burst(s, p.x, p.y, 2, 3)
            continue
        if p.kind == PROJ_ENEMY:
            dx = p.x - s.px
            adx = -dx if dx < 0 else dx
            dy = p.y - s.py
            ady = -dy if dy < 0 else dy
            if adx < PLAYER_HALF_W + 65536 and ady < PLAYER_HALF_H + 65536:
                p.active = 0
                player_hurt(s, p.damage)
                if p.damage >= 20:
                    s.pvy = int(1.5 * 65536)
                    s.on_ground = False
        else:
            for e in s.enemies:
                if e is None or not e.active:
                    continue
                dx = p.x - e.x
                adx = -dx if dx < 0 else dx
                dy = p.y - e.y
                ady = -dy if dy < 0 else dy
                if adx < 2 * 65536 and ady < 2 * 65536:
                    enemy_damage(s, e, p.damage)
                    p.active = 0
                    part_burst(s, p.x, p.y, 3, 4)
                    break


def enemy_damage(s, e, dmg):
    # v0.3.2: generischer Feind-Schaden mit Wug-Sonderregeln (spiegelt C 1:1)
    if not e.active:
        return
    if e.kind == ENEMY_WUG and e.state == WUG_ST_COOLDOWN:
        dmg *= 2
    e.hp = e.hp - dmg if e.hp > dmg else 0
    if e.kind == ENEMY_WUG and e.hp > 0 and e.state not in (WUG_ST_FLEE, WUG_ST_CHARGE, WUG_ST_FIRE):
        e.state = WUG_ST_FLEE


def enemy_update(s):
    for e in s.enemies:
        if e is None or not e.active:
            continue
        if e.hp == 0:
            e.active = 0
            s.score += e.score_val
            s.xp += e.xp_val
            s.enemies_killed += 1
            part_burst(s, e.x, e.y + 65536, 4, 8)
            continue
        if e.hit_timer > 0:
            e.hit_timer -= 1
        # v0.3.2: Wug-Zustandsautomat
        if e.kind == ENEMY_WUG:
            wdx = s.px - e.x
            wdir = 1 if wdx > 0 else (-1 if wdx < 0 else 0)
            e.facing = -wdir if e.state == WUG_ST_FLEE else wdir
            if e.state == WUG_ST_FLEE:
                e.x += WUG_FLEE_SPEED if e.facing > 0 else -WUG_FLEE_SPEED
                if e.x < fix16_from_int(1):
                    e.x = fix16_from_int(1); e.state = WUG_ST_WADDLE; e.state_timer = 120 + (s.rng() & 119)
                elif e.x > s.world_w - fix16_from_int(1):
                    e.x = s.world_w - fix16_from_int(1); e.state = WUG_ST_WADDLE; e.state_timer = 120 + (s.rng() & 119)
            else:
                e.state_timer -= 1
                if e.state == WUG_ST_WADDLE:
                    e.x += fix16_from_int(1) // 2 if e.facing > 0 else -fix16_from_int(1) // 2
                    if e.x < fix16_from_int(1): e.x = fix16_from_int(1)
                    if e.x > s.world_w - fix16_from_int(1): e.x = s.world_w - fix16_from_int(1)
                    if e.state_timer == 0: e.state = WUG_ST_STOP; e.state_timer = 30
                elif e.state == WUG_ST_STOP:
                    if e.state_timer == 0: e.state = WUG_ST_CHARGE; e.state_timer = 60
                elif e.state == WUG_ST_CHARGE:
                    if e.state_timer == 0: e.state = WUG_ST_FIRE; e.state_timer = 40
                elif e.state == WUG_ST_FIRE:
                    if e.state_timer == 39:
                        proj_spawn(s, e.x, e.y + 65536,
                                   fix16_from_int(8) if e.facing > 0 else -fix16_from_int(8), 0,
                                   PROJ_ENEMY, 0, 25, 150)
                    if e.state_timer == 0: e.state = WUG_ST_COOLDOWN; e.state_timer = 50
                elif e.state == WUG_ST_COOLDOWN:
                    if e.state_timer == 0: e.state = WUG_ST_WADDLE; e.state_timer = 120 + (s.rng() & 119)
                else:
                    e.state = WUG_ST_WADDLE; e.state_timer = 120
            cdx = s.px - e.x; acdx = -cdx if cdx < 0 else cdx
            cdy = s.py - e.y; acdy = -cdy if cdy < 0 else cdy
            if acdx < PLAYER_HALF_W + 65536 and acdy < PLAYER_HALF_H + 65536 and e.hit_timer == 0:
                e.hit_timer = 30
                player_hurt(s, 10)
            continue
        dx = s.px - e.x
        dirn = 1 if dx > 0 else (-1 if dx < 0 else 0)
        spd = 26214 if e.kind == ENEMY_TANK else 52428  # 0.4*65536=26214.4->26214, 0.8*65536=52428.8->52428
        if e.kind == ENEMY_SHOOTER:
            dist = dx if dx > 0 else -dx
            if dist < fix16_from_int(8):
                dirn = -dirn
            elif dist > fix16_from_int(14):
                pass  # anmauern
            else:
                dirn = 0
            e.shoot_timer -= 1
            if e.shoot_timer == 0:
                e.shoot_timer = 40
                d8 = dir8_from_vec(dx, s.py - e.y)
                proj_spawn(s, e.x, e.y + 65536,
                           DIR8_DX[d8] // 2, DIR8_DY[d8] // 2,
                           PROJ_ENEMY, 0, 5, 150)
        e.vx = spd if dirn > 0 else (-spd if dirn < 0 else 0)
        e.x += e.vx
        if e.x < fix16_from_int(1):
            e.x = fix16_from_int(1)
        if e.x > s.world_w - fix16_from_int(1):
            e.x = s.world_w - fix16_from_int(1)
        cdx = s.px - e.x
        acdx = -cdx if cdx < 0 else cdx
        cdy = s.py - e.y
        acdy = -cdy if cdy < 0 else cdy
        if acdx < PLAYER_HALF_W + 65536 and acdy < PLAYER_HALF_H + 65536 and e.hit_timer == 0:
            e.hit_timer = 30
            player_hurt(s, 20 if e.kind == ENEMY_TANK else 10)


def part_update(s):
    for pt in s.particles:
        if pt is None or not pt.active:
            continue
        pt.x += pt.vx
        pt.y += pt.vy
        pt.vy -= 13107  # 0.2 * 65536 = 13107.2 -> 13107 (C: FIX(0.2) = 0.2*65536 = 13107.2 -> int 13107)
        if pt.y < 0:
            pt.y = 0
            pt.vy = 0
        pt.life -= 1
        if pt.life == 0:
            pt.active = 0


def xp_update(s):
    # C: while (s->xp >= 50u * s->level) — XP-Loop
    while s.xp >= 50 * s.level:
        s.xp -= 50 * s.level
        s.level += 1
        if s.level % 2 == 1:
            if s.weapon_level < 5:
                s.weapon_level += 1
        else:
            s.max_hp += 10
            s.hp += 10


INPUT_LEFT = 0x0001
INPUT_RIGHT = 0x0002
INPUT_UP = 0x0004
INPUT_DOWN = 0x0008
INPUT_FIRE = 0x0010
INPUT_SECOND = 0x0020
INPUT_WEAPSW = 0x0040
INPUT_JUMP = 0x0400


def core_tick(s, input_mask):
    if s.state in (STATE_GAMEOVER, STATE_VICTORY):
        return
    s.tick += 1
    if s.respawn_timer > 0:
        s.respawn_timer -= 1

    if s.state == STATE_WAVE_INTRO:
        s.state_timer -= 1
        if s.state_timer == 0:
            s.wave += 1
            wave_spawn(s)
            s.state = STATE_PLAYING
    elif s.state == STATE_PLAYING:
        any_active = any(e is not None and e.active for e in s.enemies)
        if not any_active:
            if s.mode == MODE_CAMPAIGN and (s.wave % CAMPAIGN_WAVES_PER_LEVEL) == 0:
                s.state = (STATE_VICTORY if s.wave >= CAMPAIGN_LEVELS * CAMPAIGN_WAVES_PER_LEVEL
                           else STATE_LEVEL_CLEAR)
                s.state_timer = LEVEL_CLEAR_TICKS
                if s.state == STATE_LEVEL_CLEAR:
                    s.score += 1000 * s.campaign_level
            else:
                s.state = STATE_WAVE_INTRO
                s.state_timer = WAVE_INTRO_TICKS
    elif s.state == STATE_LEVEL_CLEAR:
        s.state_timer -= 1
        if s.state_timer == 0:
            if s.campaign_level < CAMPAIGN_LEVELS:
                s.campaign_level += 1
            s.state = STATE_WAVE_INTRO
            s.state_timer = WAVE_INTRO_TICKS

    # --- Spieler ---
    dirn = 0
    if input_mask & INPUT_LEFT:
        dirn -= 1
    if input_mask & INPUT_RIGHT:
        dirn += 1
    s.morphing = 1 if (input_mask & INPUT_DOWN) else 0
    if dirn != 0:
        s.pvx += MOVE_ACC_FP if dirn > 0 else -MOVE_ACC_FP
        if s.pvx > MAX_SPEED_FP:
            s.pvx = MAX_SPEED_FP
        if s.pvx < -MAX_SPEED_FP:
            s.pvx = -MAX_SPEED_FP
        s.facing = dirn
    else:
        s.pvx = fix16_mul(s.pvx, FRICTION_FP)
        if 0 < s.pvx < STOP_EPSILON:
            s.pvx = 0
        if -STOP_EPSILON < s.pvx < 0:
            s.pvx = 0
    if (input_mask & (INPUT_UP | INPUT_JUMP)) and s.on_ground:
        s.pvy = JUMP_V_FP
        s.on_ground = 0
    s.pvy -= GRAVITY_FP
    if s.pvy < -MAX_FALL_FP:
        s.pvy = -MAX_FALL_FP
    s.px += s.pvx
    s.py += s.pvy
    # v0.3.2: Wug-Headbutt (spiegelt C 1:1; pvy<0 = fallend, Fuessel am Wug-Kopf)
    if s.pvy < 0:
        for e in s.enemies:
            if e is None or not e.active or e.kind != ENEMY_WUG:
                continue
            hdx = s.px - e.x; ahdx = -hdx if hdx < 0 else hdx
            head = e.y + 65536
            if ahdx < fix16_from_int(1) + 32768 and s.py <= head + 32768:
                enemy_damage(s, e, 5)
                s.pvy = JUMP_V_FP // 2
                s.on_ground = False
                s.wug_headbutts += 1
    if s.py <= 0:
        s.py = 0
        if s.pvy < 0:
            s.pvy = 0
        s.on_ground = 1
    else:
        s.on_ground = 0
    hw = MORPH_HALF_W if s.morphing else PLAYER_HALF_W
    if s.px < hw:
        s.px = hw
        if s.pvx < 0:
            s.pvx = 0
    if s.px > s.world_w - hw:
        s.px = s.world_w - hw
        if s.pvx > 0:
            s.pvx = 0

    if input_mask & INPUT_WEAPSW:
        s.weapon = (s.weapon + 1) % 3
        s.fire_cooldown = 4
    if s.fire_cooldown > 0:
        s.fire_cooldown -= 1

    s.beam_active = 1 if (input_mask & INPUT_SECOND) else 0
    if s.beam_active:
        s.pvx = 0
        s.beam_angle = (s.beam_angle + 4) & 255
        d8 = (s.beam_angle // 32) & 7
        if s.tick % BEAM_DAMAGE_TICK == 0:
            for e in s.enemies:
                if e is None or not e.active:
                    continue
                dx = e.x - s.px
                dy = e.y - s.py
                adx = -dx if dx < 0 else dx
                ady = -dy if dy < 0 else dy
                if adx > fix16_from_int(5) or ady > fix16_from_int(5):
                    continue
                if dir8_from_vec(dx, dy) == d8:
                    enemy_damage(s, e, 1)

    if (input_mask & INPUT_FIRE) and s.fire_cooldown == 0:
        player_fire(s)

    if s.tick % SHIELD_REGEN_TICK == 0 and s.shield < 100:
        s.shield += 1

    # v0.3.2: "Always two there are"
    if not s.wug_two_dialog:
        wc = sum(1 for e in s.enemies if e is not None and e.active and e.kind == ENEMY_WUG)
        if wc >= 2:
            s.wug_two_dialog = 1
    proj_update(s)
    enemy_update(s)
    pickup_update(s)
    part_update(s)
    xp_update(s)


def bot_input(s, tick):
    m = INPUT_FIRE
    best = None
    best_d = 0
    for e in s.enemies:
        if e is None or not e.active:
            continue
        dx = e.x - s.px
        adx = -dx if dx < 0 else dx
        if best is None or adx < best_d:
            best_d = adx
            best = e
    if best is not None:
        dx = best.x - s.px
        if dx > fix16_from_int(2):
            m |= INPUT_RIGHT
        elif dx < -fix16_from_int(2):
            m |= INPUT_LEFT
    if tick % 97 == 0 and s.on_ground:
        m |= INPUT_UP
    if tick % 300 < 60:
        m |= INPUT_SECOND
    if tick % 250 == 0:
        m |= INPUT_WEAPSW
    return m


def main():
    import sys
    seed = 1234
    ticks = 2500
    campaign = False
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--seed" and i + 1 < len(args):
            seed = int(args[i + 1], 0)  # erlaubt 0x-Notation
            i += 2
        elif args[i] == "--ticks" and i + 1 < len(args):
            ticks = int(args[i + 1])
            i += 2
        elif args[i] == "--campaign":
            campaign = True
            i += 1
        else:
            i += 1

    def run(state, n):
        for t in range(n):
            core_tick(state, bot_input(state, t))

    s = GameState(seed)
    if campaign:
        # Kampagne: Bot spielt alle 5 Level (15 Wellen) bis SIEG (oder Zeitgrenze)
        s.mode = MODE_CAMPAIGN
        t = 0
        while s.state not in (STATE_VICTORY, STATE_GAMEOVER) and t < 120000:
            core_tick(s, bot_input(s, t))
            t += 1
        ticks = t
    else:
        run(s, ticks)

    print("=== STRUDELFORCE Core Sim Self-Test (Python-Oracle) ===")
    print(f"seed={seed} ticks={ticks} mode={'KAMPAGNE' if campaign else 'ENDLESS'}")
    print(f"tick={s.tick} state={s.state} wave={s.wave}")
    if campaign:
        print(f"campaign_level={s.campaign_level}/{CAMPAIGN_LEVELS}")
    print(f"score={s.score} kills={s.enemies_killed} bounces={s.bounces_total}")
    print(f"hp={s.hp}/{s.max_hp} shield={s.shield} lives={s.lives} level={s.level} xp={s.xp}")
    print(f"weapon={s.weapon} weapon_level={s.weapon_level} shots={s.shots_fired}")
    # Determinismus-Check: zweiter Run mit gleichem Seed
    s2 = GameState(seed)
    if campaign:
        s2.mode = MODE_CAMPAIGN
        t = 0
        while s2.state not in (STATE_VICTORY, STATE_GAMEOVER) and t < 120000:
            core_tick(s2, bot_input(s2, t))
            t += 1
    else:
        run(s2, ticks)
    det = (s.score == s2.score and s.enemies_killed == s2.enemies_killed
           and s.bounces_total == s2.bounces_total and s.hp == s2.hp
           and s.rng_state == s2.rng_state and s.state == s2.state
           and s.campaign_level == s2.campaign_level)
    print(f"determinism_check={'OK' if det else 'FEHLER'}")
    if campaign:
        # Level-Fortschritt: campaign_level muss (wave-1)//3 + 1 sein (bis zum Sieg)
        expected = min((s.wave - 1) // CAMPAIGN_WAVES_PER_LEVEL + 1, CAMPAIGN_LEVELS) if s.wave else 1
        camp_ok = (s.state == STATE_VICTORY
                   and s.wave == CAMPAIGN_LEVELS * CAMPAIGN_WAVES_PER_LEVEL
                   and s.campaign_level == CAMPAIGN_LEVELS
                   and expected == s.campaign_level
                   and s.score >= 1000 * (1 + 2 + 3 + 4))  # Level-Boni 1..4 (kein Bonus auf Sieg)
        print(f"campaign_check={'OK' if camp_ok else 'FEHLER'}")
        if not camp_ok:
            return 1
    # Invarianten
    inv_ok = True
    for p in s.projectiles:
        if p is not None and p.active:
            if p.x < 0 or p.x > s.world_w or p.y < 0 or p.y > s.world_h:
                inv_ok = False
    for e in s.enemies:
        if e is not None and e.active:
            if e.x < fix16_from_int(1) or e.x > s.world_w - fix16_from_int(1):
                inv_ok = False
    if s.px < 0 or s.px > s.world_w:
        inv_ok = False
    print(f"invariants={'OK' if inv_ok else 'FEHLER'}")
    return 0 if (det and inv_ok) else 1


if __name__ == '__main__':
    raise SystemExit(main())
