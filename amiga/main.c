/*
 * main.c - STRUDELFORCE Amiga-Port, Bare-Metal-Entry (OCS, 320x256, 5 Bitplanes)
 * ---------------------------------------------------------------------------
 * Nutzt den *Smith-Kern* (src/game_logic.c) - identisches Gameplay zum PC.
 * Pro VBlank genau ein core_tick() (50Hz PAL). Render = flache Rects/Glyphen
 * in 5 Bitplanes (Doppelpuffer, 32 Farben), Himmel als Copper-Verlauf
 * (Gesetz 2: nur Paletten, keine Texturen). Sprites/Blitter folgen.
 *
 * Steuerung (Joystick Port 2 / WinUAE: Tastatur-Emulation):
 *   Links/Rechts = bewegen, Hoch = Sprung, Feuer = Schuss,
 *   2. Taste (Port-2-Pot) = Halt-Strahl, Runter = Morph.
 *   Linke Maustaste (Port 1) = Beenden (zurueck ins Workbench/Kickstart).
 */
#include "strudelforce.h"
#include "retro_font.h"

extern void  memclr(void *dest, unsigned long len);
extern int   sys_take(void);
extern void  sys_free(void);
extern void  sys_waitvbl(void);
extern void *sys_alloc_chip(unsigned long bytes);

/* ---- Custom-Chip-Register (Offsets ab 0xDFF000) ------------------------- */
#define CUSTOM_BASE 0xDFF000UL
#define REG16(off) (*(volatile uint16_t *)(CUSTOM_BASE + (off)))
#define REG32(off) (*(volatile uint32_t *)(CUSTOM_BASE + (off)))
#define R_JOY1DAT 0x00C
#define R_POTGOR  0x016
#define R_COP1LC  0x080
#define R_COPJMP1 0x088
#define R_DIWSTRT 0x08E
#define R_DIWSTOP 0x090
#define R_DDFSTRT 0x092
#define R_DDFSTOP 0x094
#define R_DMACON  0x096
#define R_INTENA  0x09A
#define R_INTREQ  0x09C
#define R_BPLCON0 0x100
#define R_BPLCON1 0x102
#define R_BPLCON2 0x104
#define R_BPL1MOD 0x108
#define R_BPL2MOD 0x10A
#define R_COLOR   0x180
#define CIAA_PRA  (*(volatile uint8_t *)0xBFE001UL)

#define SCR_W 320
#define SCR_H 256
#define NPLANES 5
#define ROW_WORDS (SCR_W / 16)            /* 20 Woerter / Zeile            */
#define PLANE_BYTES (ROW_WORDS * 2 * SCR_H) /* 10240                       */

/* Welt: 1 Einheit = 8 Pixel, y nach oben, Boden bei y=0. */
#define PX_PER_UNIT_SHIFT 3
#define GROUND_SY 224
#define WORLD_PX (WORLD_W_UNITS * 8)       /* 576 */

/* ---- Palette (OCS 12 Bit 0x0RGB) - Chrome-Tempest-Basis ------------------ */
enum {
    C_SKY = 0, C_WHITE = 1, C_GR_L = 2, C_GR_M = 3, C_GR_D = 4,
    C_PL_L = 5, C_PL_D = 6, C_P_BODY = 7, C_P_DARK = 8, C_P_GLOW = 9,
    C_E1_L = 10, C_E1_D = 11, C_E2_L = 12, C_E2_D = 13, C_E3_L = 14, C_E3_D = 15,
    C_PELLET = 16, C_BOUNCE = 17, C_ESHOT = 18, C_PART = 19, C_GOLD = 20,
    C_HUD_G = 21, C_HUD_R = 22, C_HUD_BG = 23, C_PILL_L = 24, C_PILL_D = 25,
    C_SHIELD = 26
};
static const uint16_t palette[32] = {
    0x000, 0xFFF, 0x8AC, 0x57A, 0x235, 0xBBB, 0x667, 0x0CF,
    0x069, 0xAFF, 0xF44, 0xA11, 0xFA0, 0xA60, 0xA4F, 0x62A,
    0xFF4, 0x4F4, 0xF6C, 0xF80, 0xFD0, 0x4F8, 0xF33, 0x223,
    0x446, 0x224, 0x4AF, 0x000, 0x000, 0x000, 0x000, 0x000
};

/* Himmel: Copper-Farbbaender, oberste Zeile bis Boden (Amiga-Copper-Raster). */
static const uint16_t sky_bands[12] = {
    0x001, 0x002, 0x103, 0x204, 0x305, 0x406,
    0x517, 0x628, 0x739, 0x84A, 0x95B, 0xA6C
};

/* ---- Bildspeicher, Copperliste (Chip-RAM) -------------------------------- */
static uint8_t  *planes[2][NPLANES];
static uint16_t *copper;
static uint16_t *cop_bpl;     /* Zeiger auf die 5 BPLxPT-Eintraege (10 Woerter) */
static int       draw_idx;    /* aktueller Zeichenpuffer                        */

#define COP_WORDS 256

/* ---- Pixel-Primitive ------------------------------------------------------ */
static void fill_rect(int x, int y, int w, int h, int c)
{
    int x2, wl, wr, p, j;
    uint16_t ml, mr;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0) return;
    x2 = x + w - 1;
    wl = x >> 4;
    wr = x2 >> 4;
    ml = (uint16_t)(0xFFFFu >> (x & 15));
    mr = (uint16_t)(0xFFFFu << (15 - (x2 & 15)));
    for (p = 0; p < NPLANES; p++) {
        uint16_t *row = (uint16_t *)planes[draw_idx][p] + y * ROW_WORDS + wl;
        int set = (c >> p) & 1;
        for (j = 0; j < h; j++) {
            if (wl == wr) {
                uint16_t m = (uint16_t)(ml & mr);
                if (set) *row |= m; else *row &= (uint16_t)~m;
            } else {
                uint16_t *q = row + 1;
                int k;
                if (set) *row |= ml; else *row &= (uint16_t)~ml;
                for (k = wl + 1; k < wr; k++) { *q = set ? 0xFFFFu : 0u; q++; }
                if (set) *q |= mr; else *q &= (uint16_t)~mr;
            }
            row += ROW_WORDS;
        }
    }
}

/* 8x8-Glyph (RETRO_FONT), Farbe c, transparent. x muss in 0..311 liegen. */
static void draw_glyph(int x, int y, unsigned char ch, int c)
{
    int r, p;
    if (ch > 127 || x < 0 || x > SCR_W - 9 || y < 0 || y > SCR_H - 8) return;
    for (r = 0; r < 8; r++) {
        uint32_t bits = ((uint32_t)RETRO_FONT[ch][r] << 24) >> (x & 15);
        uint16_t m0 = (uint16_t)(bits >> 16), m1 = (uint16_t)bits;
        if (!(m0 | m1)) continue;
        for (p = 0; p < NPLANES; p++) {
            uint16_t *w = (uint16_t *)planes[draw_idx][p] + (y + r) * ROW_WORDS + (x >> 4);
            if ((c >> p) & 1) { w[0] |= m0; w[1] |= m1; }
            else              { w[0] &= (uint16_t)~m0; w[1] &= (uint16_t)~m1; }
        }
    }
}

static void draw_text(int x, int y, const char *s, int c)
{
    while (*s) { draw_glyph(x, y, (unsigned char)*s, c); x += 8; s++; }
}

static void draw_number(int x, int y, uint32_t v, int c)
{
    char buf[11];
    int i = 10;
    buf[i] = 0;
    do { buf[--i] = (char)('0' + (v % 10u)); v /= 10u; } while (v && i > 0);
    draw_text(x, y, &buf[i], c);
}

/* ---- Welt -> Bildschirm ---------------------------------------------------- */
static int cam_x;
static int wx(fix16_t v) { return (int)(v >> (16 - PX_PER_UNIT_SHIFT)) - cam_x; }
static int wy(fix16_t v) { return GROUND_SY - (int)(v >> (16 - PX_PER_UNIT_SHIFT)); }

static void render(const GameState *s)
{
    int i, bg;
    /* Hintergrund: Mid-Layer-Saeulen, 1/4 Parallax (Layer 2) */
    bg = cam_x >> 2;
    for (i = 0; i < 9; i++) {
        int x = i * 72 - bg;
        int h = 60 + ((i * 37) & 31);
        fill_rect(x, GROUND_SY - h, 26, h, C_PILL_D);
        fill_rect(x + 2, GROUND_SY - h, 6, h, C_PILL_L);
    }
    /* Boden */
    fill_rect(0, GROUND_SY, SCR_W, 4, C_GR_L);
    fill_rect(0, GROUND_SY + 4, SCR_W, 4, C_GR_M);
    fill_rect(0, GROUND_SY + 8, SCR_W, SCR_H - GROUND_SY - 8, C_GR_D);
    /* Plattformen (y = Oberkante, h = Dicke in Einheiten) */
    for (i = 0; i < s->platform_count; i++) {
        const LevelPlatform *pl = &s->platforms[i];
        int x = wx(pl->x), y = wy(pl->y);
        int w = (int)(pl->w >> (16 - PX_PER_UNIT_SHIFT));
        int h = (int)(pl->h >> (16 - PX_PER_UNIT_SHIFT));
        if (h < 4) h = 4;
        if (x + w < 0 || x > SCR_W) continue;
        fill_rect(x, y, w, h, pl->kind == 2 ? C_E1_D : C_PL_D);
        fill_rect(x, y, w, 2, pl->kind == 2 ? C_E1_L : C_PL_L);
    }
    /* Pickups */
    for (i = 0; i < MAX_PICKUPS; i++) {
        const Pickup *p = &s->pickups[i];
        if (!p->active) continue;
        fill_rect(wx(p->x) - 3, wy(p->y) - 8, 6, 8, C_GOLD);
        fill_rect(wx(p->x) - 1, wy(p->y) - 7, 2, 2, C_WHITE);
    }
    /* Gegner */
    for (i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &s->enemies[i];
        int x, y, w, h, cl, cd;
        if (!e->active) continue;
        switch (e->kind) {
        case ENEMY_SHOOTER: w = 8;  h = 12; cl = C_E2_L; cd = C_E2_D; break;
        case ENEMY_TANK:    w = 16; h = 16; cl = C_E3_L; cd = C_E3_D; break;
        default:            w = 8;  h = 8;  cl = C_E1_L; cd = C_E1_D; break;
        }
        x = wx(e->x) - w / 2;
        y = wy(e->y) - h;
        if (e->hit_timer & 2) { cl = C_WHITE; cd = C_WHITE; }
        fill_rect(x, y, w, h, cd);
        fill_rect(x + 1, y + 1, w - 2, h / 2, cl);
        fill_rect(x + w / 4, y + 2, 2, 2, C_WHITE);
        fill_rect(x + w - w / 4 - 2, y + 2, 2, 2, C_WHITE);
    }
    /* Projektile */
    for (i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &s->projectiles[i];
        int c;
        if (!p->active) continue;
        c = (p->kind == PROJ_ENEMY) ? C_ESHOT : (p->kind == PROJ_BOUNCE) ? C_BOUNCE : C_PELLET;
        fill_rect(wx(p->x) - 2, wy(p->y) - 2, 4, 4, c);
        fill_rect(wx(p->x) - 1, wy(p->y) - 1, 2, 2, C_WHITE);
    }
    /* Spieler (blinkt waehrend Respawn-Schutz) */
    if (s->state != STATE_GAMEOVER && !(s->respawn_timer && (s->tick & 4))) {
        int px = wx(s->px), py = wy(s->py);
        if (s->morphing) {
            fill_rect(px - 4, py - 8, 8, 8, C_P_DARK);
            fill_rect(px - 3, py - 7, 6, 4, C_P_BODY);
        } else {
            fill_rect(px - 4, py - 14, 8, 14, C_P_DARK);
            fill_rect(px - 3, py - 13, 6, 6, C_P_BODY);          /* Torso/Kopf    */
            fill_rect(px + (s->facing > 0 ? 1 : -3), py - 12, 2, 2, C_P_GLOW); /* Visier */
            fill_rect(px + (s->facing > 0 ? 4 : -9), py - 9, 5, 2, C_PL_L);    /* Arm-Kanone */
        }
        if (s->shield) fill_rect(px - 6, py - 16, 12, 1, C_SHIELD);
    }
    /* Partikel */
    for (i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &s->particles[i];
        if (!p->active) continue;
        fill_rect(wx(p->x), wy(p->y), 2, 2, C_PART);
    }
    /* Laser-/Halt-Strahl */
    if (s->beam_active && s->laser_len > 0) {
        int len = (int)(s->laser_len >> (16 - PX_PER_UNIT_SHIFT));
        int x = wx(s->px), y = wy(s->py) - 10;
        if (s->laser_dir >= 0) fill_rect(x + 4, y, len, 2, C_WHITE);
        else                   fill_rect(x - 4 - len, y, len, 2, C_WHITE);
    }

    /* HUD */
    fill_rect(0, 0, SCR_W, 12, C_HUD_BG);
    draw_text(4, 2, "SCORE", C_HUD_G);
    draw_number(52, 2, s->score, C_WHITE);
    draw_text(150, 2, "HP", C_HUD_G);
    fill_rect(170, 3, (int)((s->max_hp ? s->hp * 50 / s->max_hp : 0)), 6, s->hp * 4 < s->max_hp ? C_HUD_R : C_HUD_G);
    draw_text(236, 2, "LV", C_HUD_G);
    for (i = 0; i < (int)s->lives && i < 9; i++) fill_rect(256 + i * 6, 3, 4, 6, C_P_BODY);
    if (s->state == STATE_GAMEOVER) draw_text(116, 120, "GAME OVER", C_HUD_R);
    if (s->state == STATE_WAVE_INTRO) { draw_text(124, 100, "WAVE", C_WHITE); draw_number(164, 100, s->wave, C_WHITE); }
    if (s->state == STATE_VICTORY) draw_text(124, 120, "VICTORY!", C_GOLD);
}

/* ---- Copper ---------------------------------------------------------------- */
static void build_copper(void)
{
    uint16_t *c = copper;
    int i;
    /* Bitplane-Pointer (werden pro Frame gepatcht) */
    cop_bpl = c;
    for (i = 0; i < NPLANES; i++) {
        *c++ = (uint16_t)(0x0E0 + i * 4);     *c++ = 0;
        *c++ = (uint16_t)(0x0E0 + i * 4 + 2); *c++ = 0;
    }
    for (i = 1; i < 32; i++) { *c++ = (uint16_t)(R_COLOR + i * 2); *c++ = palette[i]; }
    /* Himmel-Verlauf: Farbregister 0, je ca. 16 Zeilen (Anzeige ab Zeile 0x2C) */
    *c++ = R_COLOR; *c++ = sky_bands[0];
    for (i = 1; i < 12; i++) {
        *c++ = (uint16_t)(((0x2C + i * 16) << 8) | 0x01); *c++ = 0xFFFE;
        *c++ = R_COLOR; *c++ = sky_bands[i];
    }
    /* Boden-Zeile: Farbe 0 = Schwarz (Bitplane deckt ohnehin ab) */
    *c++ = (uint16_t)(((0x2C + GROUND_SY) << 8) | 0x01); *c++ = 0xFFFE;
    *c++ = R_COLOR; *c++ = 0x000;
    *c++ = 0xFFFF; *c++ = 0xFFFE;          /* Copper-Ende */
}

static void set_front(int idx)
{
    int i;
    for (i = 0; i < NPLANES; i++) {
        uint32_t a = (uint32_t)(uintptr_t)planes[idx][i];
        cop_bpl[i * 4 + 1] = (uint16_t)(a >> 16);
        cop_bpl[i * 4 + 3] = (uint16_t)a;
    }
}

/* ---- Eingabe: Joystick Port 2 -> Kern-Maske -------------------------------- */
static uint16_t read_input(void)
{
    uint16_t j = REG16(R_JOY1DAT), m = 0;
    int right = (j >> 1) & 1, left = (j >> 9) & 1;
    int down = right ^ ((j >> 0) & 1), up = left ^ ((j >> 8) & 1);
    if (left)  m |= INPUT_LEFT;
    if (right) m |= INPUT_RIGHT;
    if (up)    m |= INPUT_JUMP;
    if (down)  m |= INPUT_MORPH;
    if (!(CIAA_PRA & 0x80)) m |= INPUT_FIRE;               /* Port-2-Feuer   */
    if (!(REG16(R_POTGOR) & (1u << 14))) m |= INPUT_SECOND; /* Port-2-Taste 2 */
    return m;
}

static GameState g_state;

int main(void)
{
    int i, front = 0;
    uint8_t *mem;
    if (!sys_take()) return 20;

    mem = (uint8_t *)sys_alloc_chip((unsigned long)PLANE_BYTES * NPLANES * 2 + COP_WORDS * 2);
    if (!mem) { sys_free(); return 20; }
    for (i = 0; i < NPLANES; i++) {
        planes[0][i] = mem + (unsigned long)PLANE_BYTES * i;
        planes[1][i] = mem + (unsigned long)PLANE_BYTES * (NPLANES + i);
    }
    copper = (uint16_t *)(mem + (unsigned long)PLANE_BYTES * NPLANES * 2);

    core_init(&g_state, 0x5EEDCAFEu);

    build_copper();
    set_front(front);

    REG16(R_DIWSTRT) = 0x2C81;
    REG16(R_DIWSTOP) = 0x2CC1;
    REG16(R_DDFSTRT) = 0x0038;
    REG16(R_DDFSTOP) = 0x00D0;
    REG16(R_BPLCON0) = (uint16_t)((NPLANES << 12) | 0x0200);
    REG16(R_BPLCON1) = 0;
    REG16(R_BPLCON2) = 0x0024;
    REG16(R_BPL1MOD) = 0;
    REG16(R_BPL2MOD) = 0;
    REG32(R_COP1LC) = (uint32_t)(uintptr_t)copper;
    REG16(R_COPJMP1) = 0;
    REG16(R_DMACON)  = 0x8000 | 0x0200 | 0x0100 | 0x0080;    /* DMAEN BPLEN COPEN */

    while (CIAA_PRA & 0x40) {                 /* LMB (Port 1) beendet           */
        int px;
        draw_idx = front ^ 1;
        core_tick(&g_state, read_input());

        px = (int)(g_state.px >> (16 - PX_PER_UNIT_SHIFT));
        cam_x = px - SCR_W / 2;
        if (cam_x < 0) cam_x = 0;
        if (cam_x > WORLD_PX - SCR_W) cam_x = WORLD_PX - SCR_W;

        memclr(planes[draw_idx][0], (unsigned long)PLANE_BYTES * NPLANES); /* nur zusammenhaengend, wenn Ebenen adjazent */
        render(&g_state);

        sys_waitvbl();
        front = draw_idx;
        set_front(front);
    }

    REG16(R_DMACON) = 0x0780;                 /* Bitplane/Copper aus            */
    sys_free();
    return 0;
}
