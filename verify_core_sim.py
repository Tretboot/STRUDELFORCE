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

GRAVITY_FP = int(0.5 * 65536)
MAX_FALL_FP = int(8.0 * 65536)
JUMP_V_FP = int(3.0 * 65536)
MOVE_ACC_FP = int(0.35 * 65536)
MAX_SPEED_FP = int(3.0 * 65536)
FRICTION_FP = int(0.75 * 65536)
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

WEAPON_SPREAD, WEAPON_LASER, WEAPON_BOUNCE = 0, 1, 2
STATE_WAVE_INTRO, STATE_PLAYING, STATE_GAMEOVER = 0, 1, 2
PROJ_PELLET, PROJ_BOUNCE, PROJ_ENEMY = 0, 1, 2
ENEMY_WALKER, ENEMY_SHOOTER, ENEMY_TANK = 0, 1, 2


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
        self.projectiles = [None] * MAX_PROJECTILES
        self.enemies = [None] * MAX_ENEMIES
        self.particles = [None] * MAX_PARTICLES
        self.proj_head = 0
        self.enemy_head = 0
        self.part_head = 0
        self.rng_state = seed if seed else 1
        self.shots_fired = 0
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
                 'xp_val', 'hit_timer', 'shoot_timer')


class Particle:
    __slots__ = ('x', 'y', 'vx', 'vy', 'active', 'color', 'life')


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
    if kind == ENEMY_WALKER:
        e.hp, e.score_val, e.xp_val = 3, 100, 10
    elif kind == ENEMY_SHOOTER:
        e.hp, e.score_val, e.xp_val = 5, 250, 20
    else:
        e.hp, e.score_val, e.xp_val = 12, 500, 40


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


def wave_spawn(s):
    n = 3 + 2 * s.wave
    for i in range(n):
        kind = ENEMY_WALKER
        if s.wave >= 2 and i % 3 == 0:
            kind = ENEMY_SHOOTER
        if s.wave >= 3 and i == n - 1:
            kind = ENEMY_TANK
        enemy_spawn(s, kind,
                    fix16_from_int(1) if i % 2 == 0 else s.world_w - fix16_from_int(1))


def player_hurt(s, dmg):
    if s.morphing:
        return
    if s.shield > 0:
        if s.shield >= dmg:
            s.shield -= dmg
            return
        dmg -= s.shield
        s.shield = 0
    s.hp = max(0, s.hp - dmg)
    if s.hp == 0:
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
                e.hp = max(0, e.hp - dmg)
                part_burst(s, e.x, e.y + 65536, 3, 4)
        elif nearest >= 0:
            e = s.enemies[nearest]
            e.hp = max(0, e.hp - dmg)
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
        if p.x <= 0:
            p.x = 0
            p.vx = -p.vx
            if p.kind == PROJ_BOUNCE:
                bounce_hit_wall(s, p)
        elif p.x >= s.world_w:
            p.x = s.world_w
            p.vx = -p.vx
            if p.kind == PROJ_BOUNCE:
                bounce_hit_wall(s, p)
        if p.y <= 0:
            p.y = 0
            p.vy = -p.vy
            if p.kind == PROJ_BOUNCE:
                bounce_hit_wall(s, p)
        elif p.y >= s.world_h:
            p.y = s.world_h
            p.vy = -p.vy
            if p.kind == PROJ_BOUNCE:
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
        else:
            for e in s.enemies:
                if e is None or not e.active:
                    continue
                dx = p.x - e.x
                adx = -dx if dx < 0 else dx
                dy = p.y - e.y
                ady = -dy if dy < 0 else dy
                if adx < 2 * 65536 and ady < 2 * 65536:
                    e.hp = max(0, e.hp - p.damage)
                    p.active = 0
                    part_burst(s, p.x, p.y, 3, 4)
                    break


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


def core_tick(s, input_mask):
    if s.state == STATE_GAMEOVER:
        return
    s.tick += 1

    if s.state == STATE_WAVE_INTRO:
        s.state_timer -= 1
        if s.state_timer == 0:
            s.wave += 1
            wave_spawn(s)
            s.state = STATE_PLAYING
    elif s.state == STATE_PLAYING:
        any_active = any(e is not None and e.active for e in s.enemies)
        if not any_active:
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
    if (input_mask & INPUT_UP) and s.on_ground:
        s.pvy = JUMP_V_FP
        s.on_ground = 0
    s.pvy -= GRAVITY_FP
    if s.pvy < -MAX_FALL_FP:
        s.pvy = -MAX_FALL_FP
    s.px += s.pvx
    s.py += s.pvy
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
                    e.hp = max(0, e.hp - 1)

    if (input_mask & INPUT_FIRE) and s.fire_cooldown == 0:
        player_fire(s)

    if s.tick % SHIELD_REGEN_TICK == 0 and s.shield < 100:
        s.shield += 1

    proj_update(s)
    enemy_update(s)
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
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--seed" and i + 1 < len(args):
            seed = int(args[i + 1], 0)  # erlaubt 0x-Notation
            i += 2
        elif args[i] == "--ticks" and i + 1 < len(args):
            ticks = int(args[i + 1])
            i += 2
        else:
            i += 1

    s = GameState(seed)
    for t in range(ticks):
        core_tick(s, bot_input(s, t))

    print("=== STRUDELFORCE Core Sim Self-Test (Python-Oracle) ===")
    print(f"seed={seed} ticks={ticks}")
    print(f"tick={s.tick} state={s.state} wave={s.wave}")
    print(f"score={s.score} kills={s.enemies_killed} bounces={s.bounces_total}")
    print(f"hp={s.hp}/{s.max_hp} shield={s.shield} level={s.level} xp={s.xp}")
    print(f"weapon={s.weapon} weapon_level={s.weapon_level} shots={s.shots_fired}")
    # Determinismus-Check: zweiter Run mit gleichem Seed
    s2 = GameState(seed)
    for t in range(ticks):
        core_tick(s2, bot_input(s2, t))
    det = (s.score == s2.score and s.enemies_killed == s2.enemies_killed
           and s.bounces_total == s2.bounces_total and s.hp == s2.hp
           and s.rng_state == s2.rng_state)
    print(f"determinism_check={'OK' if det else 'FEHLER'}")
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
