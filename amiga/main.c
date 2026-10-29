/*
 * main.c - STRUDELFORCE Amiga-Port, Bare-Metal-Entry (OCS, 320x256, 5 Bitplanes)
 * ---------------------------------------------------------------------------
 * 100% Amiga-500 Parity mit dem PC-Frontend (v0.3.1):
 *   1. State Machine: Cracktro-Intro -> Menue -> Loading -> Spiel
 *   2. Cracktro-Intro: 3D-Wireframe-Vektor-Wuerfel, schwebende Copper-Baender,
 *      Sinus-Scroller ("PRESS FIRE TO START").
 *   3. Hauptmenue (Turrican-2-Stil): 2-Ton-Titel (Gold ueber Dunkelrot),
 *      Modus-Auswahl (Kampagne 5 Welten vs. Endlos Roguelike), Steuerung, Credits.
 *   4. Ladebildschirm: Amiga-Block-Fortschrittsbalken mit Lichtblenden und
 *      mehrphasigen Hardware-Statusmeldungen.
 *   5. Sound-Engine: ThePlayer 6.1a (Paula 4-Kanal Hardware DMA Music).
 *   6. 50Hz Fixed-Point Gameplay-Simulation (Smith-Kern).
 */

#include "support/gcc8_c_support.h"
#include "strudelforce.h"
#include "retro_font.h"
#include "sprite_data.h"

extern void  memclr(void *dest, unsigned long len);
extern void *memcpy(void *dest, const void *src, unsigned long len);
extern void  sys_boot_banner(void);
extern int   sys_take(void);
extern void  sys_free(void);
extern void  sys_waitvbl(void);
extern void *sys_alloc_chip(unsigned long bytes);
extern void  sys_free_chip(void *ptr, unsigned long bytes);
extern int   level_load_campaign(GameState *s);

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
#define ROW_WORDS (SCR_W / 16)              /* 20 Woerter pro Zeile           */
#define PLANE_BYTES (ROW_WORDS * 2 * SCR_H) /* 10240 Bytes pro Plane          */

/* Welt: 1 Einheit = 8 Pixel, y nach oben, Boden bei y=0 */
#define PX_PER_UNIT_SHIFT 3
#define GROUND_SY 224
#define WORLD_PX (WORLD_W_UNITS * 8)         /* 576 Pixel                     */

/* ---- Palette (OCS 12 Bit 0x0RGB) - Chrome-Tempest-Basis ------------------ */
enum {
    C_SKY = 0, C_WHITE = 1, C_GR_L = 2, C_GR_M = 3, C_GR_D = 4,
    C_PL_L = 5, C_PL_D = 6, C_P_BODY = 7, C_P_DARK = 8, C_P_GLOW = 9,
    C_E1_L = 10, C_E1_D = 11, C_E2_L = 12, C_E2_D = 13, C_E3_L = 14, C_E3_D = 15,
    C_PELLET = 16, C_BOUNCE = 17, C_ESHOT = 18, C_PART = 19, C_GOLD = 20,
    C_HUD_G = 21, C_HUD_R = 22, C_HUD_BG = 23, C_PILL_L = 24, C_PILL_D = 25,
    C_SHIELD = 26, C_CYAN = 27, C_BLUE = 28, C_DARK_RED = 29
};

static const uint16_t palette[32] = {
    0x000, 0xFFF, 0x8AC, 0x57A, 0x235, 0xBBB, 0x667, 0x0CF,
    0x069, 0xAFF, 0xF44, 0xA11, 0xFA0, 0xA60, 0xA4F, 0x62A,
    0xFF4, 0x4F4, 0xF6C, 0xF80, 0xFD0, 0x4F8, 0xF33, 0x223,
    0x446, 0x224, 0x4AF, 0x0DF, 0x126, 0x500, 0x000, 0x000
};

/* Himmel: Copper-Farbbaender (Amiga-Copper-Raster) */
static const uint16_t sky_bands[12] = {
    0x001, 0x002, 0x103, 0x204, 0x305, 0x406,
    0x517, 0x628, 0x739, 0x84A, 0x95B, 0xA6C
};

/* Integer Sine LUT (32 Eintraege, Amplitude [-127, 127]) */
static const int8_t s_sin32[32] = {
    0, 25, 49, 71, 90, 106, 117, 125, 127, 125, 117, 106, 90, 71, 49, 25,
    0, -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25
};

static int8_t sin_get(int idx) { return s_sin32[idx & 31]; }
static int8_t cos_get(int idx) { return s_sin32[(idx + 8) & 31]; }

/* ---- Bildspeicher, Copperliste (Chip-RAM) -------------------------------- */
static uint8_t  *planes[2][NPLANES];
static uint16_t *copper;
static uint16_t *cop_bpl;
static int       draw_idx;

#define COP_WORDS 256

/* ---- ThePlayer 6.1a (Paula 4-Kanal DMA Musik) ----------------------------- */
static unsigned char player_bin[] __attribute__((aligned(2))) = {
#embed "player610.6.no_cia.bin"
};
EMBED_CHIP module_bin[] = {
#embed "testmod.p61"
};

static uint8_t *s_p61_chip_mod = 0;
static int      s_p61_active   = 0;

static int p61_init(const void* m) {
    register volatile const void* _a0 __asm__("a0") = m;
    register volatile const void* _a1 __asm__("a1") = 0;
    register volatile const void* _a2 __asm__("a2") = 0;
    register volatile const void* _a3 __asm__("a3") = player_bin;
    register volatile const void* _a6 __asm__("a6") = (void*)0xdff000;
    register int _d0 __asm__("d0");
    __asm__ volatile (
        "movem.l %%d1-%%d7/%%a0-%%a6,-(%%sp)\n"
        "jsr 0(%%a3)\n"
        "movem.l (%%sp)+,%%d1-%%d7/%%a0-%%a6"
    : "=r" (_d0), "+rf"(_a0), "+rf"(_a1), "+rf"(_a2), "+rf"(_a3), "+rf"(_a6)
    :
    : "cc", "memory");
    return _d0;
}

static void p61_music(void) {
    register volatile const void* _a3 __asm__("a3") = player_bin;
    register volatile const void* _a6 __asm__("a6") = (void*)0xdff000;
    __asm__ volatile (
        "movem.l %%d0-%%d7/%%a0-%%a6,-(%%sp)\n"
        "jsr 4(%%a3)\n"
        "movem.l (%%sp)+,%%d0-%%d7/%%a0-%%a6"
    : "+rf"(_a3), "+rf"(_a6)
    :
    : "cc", "memory");
}

static void p61_end(void) {
    register volatile const void* _a3 __asm__("a3") = player_bin;
    register volatile const void* _a6 __asm__("a6") = (void*)0xdff000;
    __asm__ volatile (
        "movem.l %%d0-%%d7/%%a0-%%a6,-(%%sp)\n"
        "jsr 8(%%a3)\n"
        "movem.l (%%sp)+,%%d0-%%d7/%%a0-%%a6"
    : "+rf"(_a3), "+rf"(_a6)
    :
    : "cc", "memory");
}

/* ---- Pixel- & Grafik-Primitive -------------------------------------------- */
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

/* Schneller Bresenham-Linienalgorithmus in Bitplanes */
static void draw_line(int x0, int y0, int x1, int y1, int c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int sx = (dx >= 0) ? 1 : -1;
    int sy = (dy >= 0) ? 1 : -1;
    int err;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    err = (dx > dy ? dx : -dy) / 2;

    while (1) {
        if (x0 >= 0 && x0 < SCR_W && y0 >= 0 && y0 < SCR_H) {
            int wl = x0 >> 4;
            uint16_t mask = (uint16_t)(0x8000u >> (x0 & 15));
            int p;
            for (p = 0; p < NPLANES; p++) {
                uint16_t *w = (uint16_t *)planes[draw_idx][p] + y0 * ROW_WORDS + wl;
                if ((c >> p) & 1) *w |= mask;
                else              *w &= (uint16_t)~mask;
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

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

/* 2-Ton Turrican-2 Titel-Schrift (2x skaliert, 16x16 Pixel pro Zeichen) */
static void draw_char2x(int x, int y, unsigned char ch, int top_c, int bot_c)
{
    int r, col;
    if (ch > 127 || x < 0 || x > SCR_W - 16 || y < 0 || y > SCR_H - 16) return;
    for (r = 0; r < 8; r++) {
        uint8_t bits = RETRO_FONT[ch][r];
        int c = (r < 4) ? top_c : bot_c;
        if (!bits) continue;
        for (col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                fill_rect(x + col * 2, y + r * 2, 2, 2, c);
            }
        }
    }
}

static void draw_text2x(int x, int y, const char *s, int top_c, int bot_c)
{
    while (*s) {
        draw_char2x(x, y, (unsigned char)*s, top_c, bot_c);
        x += 16;
        s++;
    }
}

static void draw_number(int x, int y, uint32_t v, int c)
{
    char buf[11];
    int i = 10;
    buf[i] = 0;
    do { buf[--i] = (char)('0' + (v % 10u)); v /= 10u; } while (v && i > 0);
    draw_text(x, y, &buf[i], c);
}

/* ---- Sprite- & Animations-Rendering (Amiga OCS 5-Bitplane) ---------------- */
static void draw_sprite(int x, int y, const SFSprite *spr, int flip_x)
{
    int sw, sh, top_x, top_y, r, c;
    /* Globpalette -> OCS Palette-Indizes */
    static const uint8_t s_pal_map[16] = {
        0, 4, 3, 2, 1, 11, 10, 12, 13, 13, 23, 6, 29, 5, 6, 0
    };
    if (!spr) return;
    sw = spr->w;
    sh = spr->h;
    top_x = x - spr->ax;
    top_y = y - spr->ay;

    for (r = 0; r < sh; r++) {
        int py = top_y + r;
        int row_off;
        if (py < 0 || py >= SCR_H) continue;
        row_off = r * sw;

        for (c = 0; c < sw; c++) {
            int src_x = flip_x ? (sw - 1 - c) : c;
            uint8_t pidx = spr->px[row_off + src_x];
            uint8_t gcol;
            int col, px, wl, p;
            uint16_t mask;

            if (pidx == SF_SPRITE_TRANSPARENT || pidx >= spr->n_colors) continue;

            px = top_x + c;
            if (px < 0 || px >= SCR_W) continue;

            gcol = spr->pal[pidx];
            col = s_pal_map[gcol & 15];

            wl = px >> 4;
            mask = (uint16_t)(0x8000u >> (px & 15));
            for (p = 0; p < NPLANES; p++) {
                uint16_t *w = (uint16_t *)planes[draw_idx][p] + py * ROW_WORDS + wl;
                if ((col >> p) & 1) *w |= mask;
                else              *w &= (uint16_t)~mask;
            }
        }
    }
}

static void draw_anim(int x, int y, const SFAnim *anim, int frame_tick, int flip_x)
{
    int f;
    if (!anim || anim->n_frames == 0) return;
    f = frame_tick % anim->n_frames;
    draw_sprite(x, y, anim->frames[f], flip_x);
}

/* ---- Welt -> Bildschirm ---------------------------------------------------- */
static int cam_x;
static int wx(fix16_t v) { return (int)(v >> (16 - PX_PER_UNIT_SHIFT)) - cam_x; }
static int wy(fix16_t v) { return GROUND_SY - (int)(v >> (16 - PX_PER_UNIT_SHIFT)); }

/* ============================================================================
 * State Machine
 * ============================================================================ */
typedef enum {
    AMI_STATE_INTRO = 0,    /* Amiga Cracktro Intro (3D-Logo, Copper, Sinus) */
    AMI_STATE_MENU,         /* Turrican-2 Hauptmenue */
    AMI_STATE_CONTROLS,     /* Steuerung-Anzeige */
    AMI_STATE_CREDITS,      /* Credits-Anzeige */
    AMI_STATE_LOADING,      /* Verbose Amiga Shell Ladebalken mit echtem Feedback */
    AMI_STATE_STORY,        /* "THE WUG" - Star Wars Perspective Lore Crawl */
    AMI_STATE_GAME          /* 50Hz Fixed-Point Spiel */
} AmiState;

static AmiState s_ami_state = AMI_STATE_INTRO;
static int      s_menu_sel  = 0;
static int      s_mode      = MODE_CAMPAIGN;
static int      s_load_tick = 0;
static int      s_story_scroll = 0;
static int      s_story_paused = 0;
static uint32_t s_frame_cnt = 0;

#define LOAD_TICKS_TOTAL 84 /* ~1.7 Sekunden Loading (6 Phasen a 14 Ticks) */

/* ---- Demoscene 3D Vector Cube Vertices & Edges (100% Integer Math) -------- */
static const int8_t s_cube_v[8][3] = {
    {-36, -36, -36}, { 36, -36, -36},
    { 36,  36, -36}, {-36,  36, -36},
    {-36, -36,  36}, { 36, -36,  36},
    { 36,  36,  36}, {-36,  36,  36}
};

static const uint8_t s_cube_e[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

/* Scroller-Text im Cracktro Intro */
static const char INTRO_SCROLLER[] =
    "     +++ STRUDELFORCE 0.3.1 +++ 100% AMIGA 500 BARE-METAL EDITION +++ "
    "PAULA 4-VOICE DMA MUSIC VIA THEPLAYER 6.1A +++ 16.16 FIXED-POINT CORE BY FRANK MITZKATIS & AI CODING AGENTS +++ "
    "GREETS TO FACTOR 5, RAINBOW ARTS, CHRIS HUELSBECK AND COMMODORE +++ PRESS FIRE TO ENTER MAIN MENU +++     ";

/* ---- State Renderers ------------------------------------------------------ */

static void render_intro(uint32_t f)
{
    int i, scroller_len, scroll_px, char_idx, fine_x;
    int px[8], py[8];
    int ax = (int)(f & 31);
    int ay = (int)((f * 2) & 31);
    int az = (int)((f >> 1) & 31);

    int cosX = cos_get(ax), sinX = sin_get(ax);
    int cosY = cos_get(ay), sinY = sin_get(ay);
    int cosZ = cos_get(az), sinZ = sin_get(az);

    /* 1. Header Banner (ohne Abdunkelkasten) */
    draw_text(16, 7, "* COMMODORE AMIGA 500 DEMO INTRO *", C_CYAN);

    /* 2. 3D Rotating Vector Cube in Bitplanes */
    for (i = 0; i < 8; i++) {
        int x = s_cube_v[i][0];
        int y = s_cube_v[i][1];
        int z = s_cube_v[i][2];

        int x1 = (x * cosY - z * sinY) / 127;
        int z1 = (x * sinY + z * cosY) / 127;
        int y1 = (y * cosX - z1 * sinX) / 127;
        int z2 = (y * sinX + z1 * cosX) / 127;
        int x2 = (x1 * cosZ - y1 * sinZ) / 127;
        int y2 = (x1 * sinZ + y1 * cosZ) / 127;

        int dist = z2 + 160;
        if (dist < 20) dist = 20;
        px[i] = 160 + (x2 * 130) / dist;
        py[i] = 100 + (y2 * 130) / dist;
    }

    for (i = 0; i < 12; i++) {
        int v0 = s_cube_e[i][0];
        int v1 = s_cube_e[i][1];
        draw_line(px[v0], py[v0], px[v1], py[v1], C_CYAN);
        draw_line(px[v0] + 1, py[v0], px[v1] + 1, py[v1], C_WHITE);
    }

    /* 3. Blinkender Call to Action */
    if ((f / 20) & 1) {
        draw_text(68, 172, "PRESS FIRE BUTTON OR SPACE", C_GOLD);
    } else {
        draw_text(68, 172, "PRESS FIRE BUTTON OR SPACE", C_WHITE);
    }

    /* 4. Amiga Sinus Scroller */
    scroller_len = 0;
    while (INTRO_SCROLLER[scroller_len]) scroller_len++;
    scroll_px = (int)f * 2;
    char_idx = (scroll_px / 8) % scroller_len;
    fine_x = scroll_px & 7;

    for (i = 0; i < 41; i++) {
        char ch = INTRO_SCROLLER[(char_idx + i) % scroller_len];
        int sx = i * 8 - fine_x;
        int sy = 214 + (sin_get(i * 2 + (int)(f & 31)) * 6) / 127;
        if (sx >= -8 && sx < SCR_W) {
            draw_glyph(sx, sy, (unsigned char)ch, C_WHITE);
        }
    }
}

static void render_menu(int sel, uint32_t f)
{
    int i;
    /* Top Bar (ohne dunklen Kasten) */
    draw_text(8, 4, "[>] STRUDELFORCE 0.3.1 - AMIGA 500 PAL", C_HUD_G);

    /* Turrican-2 Style 2-Ton Titel (Schlagschatten + Front) */
    draw_text2x(66, 26, "STRUDELFORCE", C_DARK_RED, C_DARK_RED);
    draw_text2x(64, 24, "STRUDELFORCE", C_GOLD, C_HUD_R);

    draw_text(40, 58, "A TURRICAN TRIBUTE FOR AMIGA 500", C_PL_L);
    draw_text(32, 72, "DISK 1 : STRUDELFORCE.ADF (SINGLE DISC)", C_CYAN);

    /* Menue-Eintraege: Jeder mit eigener lebhafter Farbe (ohne dunklen Hintergrund) */
    const char *items[4] = {
        "KAMPAGNE (5 WELTEN)",
        "ENDLOS ROGUELIKE",
        "STEUERUNG / CONTROLS",
        "CREDITS & INFO"
    };
    const uint8_t item_colors[4] = {
        C_GOLD,
        C_CYAN,
        C_HUD_G,
        C_ESHOT
    };

    for (i = 0; i < 4; i++) {
        int y = 104 + i * 20;
        if (i == sel) {
            int blink_c = ((f / 8) & 1) ? C_WHITE : C_GOLD;
            draw_text(38, y, "> ", blink_c);
            draw_text(58, y, items[i], C_WHITE);
        } else {
            draw_text(58, y, items[i], item_colors[i]);
        }
    }

    /* Bedienhinweise */
    draw_text(36, 198, "JOYSTICK / PFEILTASTEN : AUSWAHL", C_WHITE);
    draw_text(40, 214, "FEUER / SPACE / ENTER  : START", C_GOLD);
    draw_text(72, 234, "ESC : BEENDEN ZUR WORKBENCH", C_GR_M);
}

static void render_controls(void)
{
    /* Top Header */
    draw_text(8, 4, "[STEUERUNG / CONTROLS]", C_HUD_G);

    /* Grosser 2-Ton Titel */
    draw_text2x(76, 18, "STEUERUNG", C_DARK_RED, C_DARK_RED);
    draw_text2x(74, 16, "STEUERUNG", C_GOLD, C_HUD_R);
    draw_text(88, 38, "TURRICAN AMIGA 500 KEYS", C_CYAN);

    /* Farbige Tags pro Eingabe-Art */
    draw_text(24, 60,  "[STICK]", C_CYAN);
    draw_text(88, 60,  ": JOYSTICK PORT 2 / WASD", C_WHITE);

    draw_text(24, 78,  "[SPRUNG]", C_HUD_G);
    draw_text(88, 78,  ": HOCH / W = SPRINGEN", C_WHITE);

    draw_text(24, 96,  "[MORPH]", C_BOUNCE);
    draw_text(88, 96,  ": RUNTER / S = MORPH-BALL", C_WHITE);

    draw_text(24, 114, "[FEUER]", C_GOLD);
    draw_text(88, 114, ": FEUER / SPACE / ENTER", C_WHITE);

    draw_text(24, 132, "[BEAM]", C_PELLET);
    draw_text(88, 132, ": 2. TASTE / ALT / C", C_WHITE);

    draw_text(24, 150, "[MAUS]", C_PL_L);
    draw_text(88, 150, ": PORT 1 LMB = FEUER / AUSWAHL", C_WHITE);

    draw_text(24, 168, "[MENUE]", C_ESHOT);
    draw_text(88, 168, ": ESC = PAUSE / HAUPTMENUE", C_WHITE);

    draw_text(48, 222, "DRUECKE FEUER / SPACE ZUM MENUE", C_GOLD);
}

typedef struct {
    const char *tag;
    const char *text;
    uint8_t     tag_col;
    uint8_t     txt_col;
    uint8_t     is_hdr;
} AmiCreditLine;

static const AmiCreditLine AMI_CREDITS[] = {
    {"",           "STRUDELFORCE",                    C_GOLD,   C_HUD_R,  1},
    {"",           "AMIGA 500 BARE-METAL EDITION",    C_CYAN,   C_CYAN,   0},
    {"",           "VERSION 0.3.1 (50HZ PAL)",        C_GOLD,   C_GOLD,   0},
    {"",           "",                                0,        0,        0},
    {"[PROJEKT]",  "FRANK MITZKATIS",                 C_GOLD,   C_WHITE,  0},
    {"[KONZEPT]",  "TURRICAN II BENCHMARK TRIBUTE",   C_CYAN,   C_PL_L,   0},
    {"[CORE SIM]", "16.16 FIXED-POINT ZERO-FLOAT",    C_HUD_G,  C_WHITE,  0},
    {"[AUDIO]",    "THEPLAYER 6.1A REPLAYER",         C_ESHOT,  C_WHITE,  0},
    {"[HARDWARE]", "PAULA 4-VOICE DMA 16574 HZ",      C_ESHOT,  C_PL_L,   0},
    {"[GRAFIK]",   "OCS 5 BITPLANES COPPER DUAL-BUF", C_E2_L,   C_WHITE,  0},
    {"[LEVELS]",   "197 CHUNKS ROGUE GENERATOR",      C_PELLET, C_WHITE,  0},
    {"[AI CORE]",  "AUTONOMOUS AGENTS PAIR-CODING",   C_CYAN,   C_WHITE,  0},
    {"",           "",                                0,        0,        0},
    {"",           "SPECIAL THANKS",                  C_GOLD,   C_HUD_R,  1},
    {"[HERO]",     "CHRIS HUELSBECK (TFMX SOUND)",    C_PELLET, C_WHITE,  0},
    {"[TRIBUTE]",  "MANFRED TRENZ & FACTOR 5",        C_GOLD,   C_WHITE,  0},
    {"[LEGENDS]",  "RAINBOW ARTS & COMMODORE",        C_CYAN,   C_WHITE,  0},
    {"[SCENE]",    "FAIRLIGHT, SKID ROW, RAZOR 1911", C_ESHOT,  C_PL_L,   0},
    {"[SCENE]",    "PARADOX, THE SILENTS, RED SECT",  C_E3_L,   C_PL_L,   0},
    {"",           "",                                0,        0,        0},
    {"",           "SYSTEM SPECS",                    C_GOLD,   C_HUD_R,  1},
    {"[CPU]",      "MOTOROLA 68000 @ 7.09 MHZ PAL",   C_HUD_G,  C_WHITE,  0},
    {"[CHIP-RAM]", "512 KB BASE RAM (OFS BOOT)",      C_CYAN,   C_WHITE,  0},
    {"[DISK]",     "SINGLE 880K FLOPPY (STRUDELOS)",  C_GOLD,   C_WHITE,  0},
    {"",           "",                                0,        0,        0},
    {"",           "TURRICAN FOREVER",                C_GOLD,   C_HUD_R,  1},
    {"",           "THANK YOU FOR PLAYING!",          C_WHITE,  C_CYAN,   0},
    {"",           "",                                0,        0,        0}
};
#define AMI_CREDITS_COUNT ((int)(sizeof(AMI_CREDITS) / sizeof(AMI_CREDITS[0])))

static struct {
    int16_t x, y;
    int8_t  sp;
    uint8_t col;
} s_ami_stars[24];
static int s_ami_stars_init = 0;
static int s_credit_scroll_y = 0;
static int s_credit_paused = 0;

static void render_credits(uint32_t f, uint16_t in)
{
    int i;
    int line_h = 16;
    int total_scroll = AMI_CREDITS_COUNT * line_h + SCR_H + 40;

    /* 1. Sterne im Hintergrund ohne jedes Abdunkeln */
    if (!s_ami_stars_init) {
        for (i = 0; i < 24; i++) {
            s_ami_stars[i].x = (int16_t)((i * 37 + 13) % (SCR_W - 8));
            s_ami_stars[i].y = (int16_t)((i * 29 + 7) % SCR_H);
            s_ami_stars[i].sp = (int8_t)(1 + (i % 3));
            s_ami_stars[i].col = (i % 3 == 0) ? C_WHITE : ((i % 3 == 1) ? C_CYAN : C_PELLET);
        }
        s_ami_stars_init = 1;
    }
    for (i = 0; i < 24; i++) {
        if (!s_credit_paused) {
            s_ami_stars[i].y += s_ami_stars[i].sp;
            if (s_ami_stars[i].y >= SCR_H) s_ami_stars[i].y = 0;
        }
        draw_glyph(s_ami_stars[i].x, s_ami_stars[i].y, '.', s_ami_stars[i].col);
    }

    /* 2. Scroll-Steuerung */
    if (in & (INPUT_MORPH | INPUT_DOWN)) { /* Down / S: Schneller vorwaerts */
        s_credit_scroll_y += 5;
    } else if (in & (INPUT_JUMP | INPUT_UP)) { /* Up / W: Zurueck */
        s_credit_scroll_y -= 5;
        if (s_credit_scroll_y < 0) s_credit_scroll_y = 0;
    } else if (!s_credit_paused) {
        s_credit_scroll_y += 2; /* Fluessiger, zuegiger 50Hz-Scroll */
    }

    if (s_credit_scroll_y > total_scroll) s_credit_scroll_y = total_scroll;

    /* 3. Textzeilen zeichnen */
    for (i = 0; i < AMI_CREDITS_COUNT; i++) {
        const AmiCreditLine *cl = &AMI_CREDITS[i];
        int ly = SCR_H - 30 - s_credit_scroll_y + i * line_h;
        if (ly < 16 || ly > SCR_H - 20) continue;

        if (cl->is_hdr) {
            /* Grosse 2-Ton Turrican-2 Ueberschrift */
            draw_text2x(42, ly - 2, cl->text, C_DARK_RED, C_DARK_RED);
            draw_text2x(40, ly - 4, cl->text, cl->tag_col, cl->txt_col);
        } else if (cl->tag[0]) {
            /* Farbiger Tag + Text */
            draw_text(16, ly, cl->tag, cl->tag_col);
            draw_text(104, ly, cl->text, cl->txt_col);
        } else if (cl->text[0]) {
            draw_text(36, ly, cl->text, cl->txt_col);
        }
    }

    /* 4. Top & Bottom Info (ohne Kasten) */
    draw_text(8, 4, "[>] STRUDELFORCE 0.3.1 CREDITS", C_GOLD);
    draw_text(220, 4, s_credit_paused ? "[PAUSIERT]" : "[AMIGA 500]", C_CYAN);

    draw_text(16, 242, "FEUER/ESC: MENUE  SPACE: PAUSE  PFEILE: TEMPO", C_GR_L);
}

static const char * const LOAD_CONSOLE_LOGS[6] = {
    "1.Workbench:> execute S:Startup-Sequence",
    "> 1. STRUDELOS: CHIP-RAM & HARDWARE INIT... OK",
    "> 2. COPPER: 5 OCS BITPLANES & 32-COL PALETTE... OK",
    "> 3. LEVEL-ENGINE: 197 CHUNKS & BIOMES PARSED... OK",
    "> 4. PAULA DMA: 4-VOICE REPLAYER SYNCED AT 50HZ... OK",
    "> 5. CORE SIM: 16.16 FIXED-POINT ZERO-FLOAT... OK"
};

static void render_loading(int ticks)
{
    int i, bw = 240, bh = 12, bx = 40, by = 186;
    int phase = ticks / 14; /* 6 Phasen a 14 Ticks = 84 Ticks total (~1.7s) */
    int pct = (ticks * 100) / LOAD_TICKS_TOTAL;
    int fill;
    if (pct > 100) pct = 100;
    fill = (bw * pct) / 100;

    /* Titel */
    draw_text2x(64, 14, "STRUDELFORCE", C_DARK_RED, C_DARK_RED);
    draw_text2x(62, 12, "STRUDELFORCE", C_GOLD, C_HUD_R);
    draw_text(40, 36, "DISK 1 : STRUDELOS.ADF (AMIGA 500 BARE-METAL)", C_CYAN);

    /* Authentisches Amiga Shell-Fenster */
    draw_line(18, 52, 302, 52, C_HUD_G);
    draw_line(18, 172, 302, 172, C_HUD_G);
    draw_line(18, 52, 18, 172, C_HUD_G);
    draw_line(302, 52, 302, 172, C_HUD_G);

    draw_text(24, 56, "[ AmigaShell - StrudelOS v0.3.1 ]", C_HUD_G);
    draw_line(18, 66, 302, 66, C_HUD_G);

    /* Verbose Shell Logs (zeilenweiser Aufbau mit echtem Feedback) */
    draw_text(24, 72, LOAD_CONSOLE_LOGS[0], C_WHITE);
    if (phase >= 1) draw_text(24, 86, LOAD_CONSOLE_LOGS[1], C_CYAN);
    if (phase >= 2) draw_text(24, 100, LOAD_CONSOLE_LOGS[2], C_CYAN);
    if (phase >= 3) draw_text(24, 114, LOAD_CONSOLE_LOGS[3], C_CYAN);
    if (phase >= 4) draw_text(24, 128, LOAD_CONSOLE_LOGS[4], C_CYAN);
    if (phase >= 5) draw_text(24, 142, LOAD_CONSOLE_LOGS[5], C_HUD_G);
    if (phase >= 6) draw_text(24, 156, "> 6. BEREIT: STARTE THE WUG INTRO...", C_GOLD);

    /* Progress-Bar */
    draw_line(bx - 2, by - 2, bx + bw + 2, by - 2, C_CYAN);
    draw_line(bx - 2, by + bh + 1, bx + bw + 2, by + bh + 1, C_CYAN);
    draw_line(bx - 2, by - 2, bx - 2, by + bh + 1, C_CYAN);
    draw_line(bx + bw + 2, by - 2, bx + bw + 2, by + bh + 1, C_CYAN);

    if (fill > 0) {
        fill_rect(bx, by, fill, bh, C_E2_L);
        for (i = 0; i < 6; i++) {
            int sx = ((i * 42 + ticks * 5) % (fill + 10)) - 10;
            if (sx >= 0 && sx < fill - 4) {
                fill_rect(bx + sx, by, 4, bh, C_WHITE);
            }
        }
    }

    draw_number(136, 206, pct, C_WHITE);
    draw_text(164, 206, "% GELADEN", C_WHITE);

    draw_text(36, 226, "SYSTEM-CHECK: 512KB CHIP + OCS BLITTER OK", C_PL_L);
}

static const char * const STORY_CRAWL[] = {
    "ES HERRSCHT KRIEG IM AMIGA-SEKTOR!",
    "",
    "Aus den tiefsten Datenspeichern des",
    "kybernetischen Universums ist eine",
    "raetselhafte Spezies erwacht:",
    "",
    "           >>> DIE WUGS <<<",
    "",
    "Einst als harmloser linguistischer",
    "Test erschaffen, haben sie ein",
    "kollektives Bewusstsein entwickelt.",
    "",
    "Ihre watschelnden Koerper taeuschen",
    "ueber ihre toedliche Gefahr hinweg:",
    "Sobald ihre Augen rot aufgluehen,",
    "entfesseln sie zerstoererische",
    "Photonen-Laser!",
    "",
    "Die fuenf Biome von Strudelforce",
    "stehen kurz vor dem Kollaps:",
    "Chrome Tempest, The Great Bath und",
    "der gluehende Mechanical Hive.",
    "",
    "Du bist Bren McGuire (Unit-7).",
    "Ausgeruestet mit 16.16-Fixed-Point",
    "Physik, dem 360-Grad Blitz-Laser",
    "und dem rotierenden Morph-Kreisel",
    "bist du die letzte Hoffnung!",
    "",
    "Kaempfe dich durch die Wellen,",
    "zerstoere die feindlichen Titanen,",
    "und befreie das Turrican-Reich!",
    "",
    "      MOEGE DER COPPER MIT DIR SEIN!",
    "",
    ""
};
#define STORY_CRAWL_COUNT ((int)(sizeof(STORY_CRAWL) / sizeof(STORY_CRAWL[0])))

static void render_story(uint32_t f, uint16_t in)
{
    int i;
    int line_h = 16;
    int total_scroll = STORY_CRAWL_COUNT * line_h + SCR_H + 40;

    /* 1. Sterne im tiefen Weltraum */
    if (!s_ami_stars_init) {
        for (i = 0; i < 24; i++) {
            s_ami_stars[i].x = (int16_t)((i * 37 + 13) % (SCR_W - 8));
            s_ami_stars[i].y = (int16_t)((i * 29 + 7) % SCR_H);
            s_ami_stars[i].sp = (int8_t)(1 + (i % 3));
            s_ami_stars[i].col = (i % 3 == 0) ? C_WHITE : ((i % 3 == 1) ? C_CYAN : C_PELLET);
        }
        s_ami_stars_init = 1;
    }
    for (i = 0; i < 24; i++) {
        if (!s_story_paused) {
            s_ami_stars[i].y += s_ami_stars[i].sp;
            if (s_ami_stars[i].y >= SCR_H) s_ami_stars[i].y = 0;
        }
        draw_glyph(s_ami_stars[i].x, s_ami_stars[i].y, '.', s_ami_stars[i].col);
    }

    /* 2. Star Wars Scroll-Steuerung */
    if (in & INPUT_MORPH) { /* Down / S: Schneller */
        s_story_scroll += 3;
    } else if (in & INPUT_JUMP) { /* Up / W: Zurueck */
        s_story_scroll -= 2;
        if (s_story_scroll < 0) s_story_scroll = 0;
    } else if (!s_story_paused) {
        if ((f & 1) == 0) s_story_scroll++; /* Sanfter 25-50Hz Scroll */
    }

    /* 3. Grosses "THE WUG" Star Wars Intro-Logo */
    draw_text2x(106, 18, "THE WUG", C_DARK_RED, C_DARK_RED);
    draw_text2x(104, 16, "THE WUG", C_GOLD, C_HUD_R);
    draw_text(68, 38, "EPISODE I: DIE INVASION", C_CYAN);
    draw_line(48, 12, 272, 12, C_GOLD);
    draw_line(48, 48, 272, 48, C_GOLD);

    /* 4. Star Wars 3D Perspective Crawl */
    for (i = 0; i < STORY_CRAWL_COUNT; i++) {
        const char *l = STORY_CRAWL[i];
        if (!l || l[0] == '\0') continue;

        int wy = SCR_H + 40 - s_story_scroll + i * line_h;
        int y_horiz = 54;
        int y_base  = 220;

        if (wy < y_horiz || wy > y_base + 35) continue;

        /* Perspektiv-Skalierung (0 bis 256) */
        int t = ((wy - y_horiz) * 256) / (y_base - y_horiz);
        if (t < 24) t = 24;
        if (t > 256) t = 256;

        int sy = y_horiz + (t * (y_base - y_horiz)) / 256;

        /* Zeichenbreite schrumpft perspektivisch zur Mitte hin */
        int cw = 4 + (t * 4) / 256;
        int len = 0;
        while (l[len]) len++;
        int line_w = len * cw;
        int sx = 160 - line_w / 2;

        /* Farbe je nach Tiefe */
        int col = (sy > 170) ? C_GOLD : ((sy > 115) ? C_PELLET : C_CYAN);
        if (i == 6) col = C_HUD_R;

        for (int c = 0; c < len; c++) {
            draw_glyph(sx + c * cw, sy, (unsigned char)l[c], col);
        }
    }

    /* 5. Bottom Skip Hinweis */
    draw_text(28, 240, "FEUER / SPACE / ESC : MISSION STARTEN", C_WHITE);
}

static void render_gameplay(const GameState *s)
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

    /* Plattformen */
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

    /* Pickups (Turrican Diamanten / Icons) */
    for (i = 0; i < MAX_PICKUPS; i++) {
        const Pickup *p = &s->pickups[i];
        int icon_idx;
        if (!p->active) continue;
        icon_idx = (i + (s->tick / 6)) % 3;
        draw_anim(wx(p->x), wy(p->y), (icon_idx == 0 ? &ICON_0_ANIM : icon_idx == 1 ? &ICON_1_ANIM : &ICON_2_ANIM), 0, 0);
    }

    /* Gegner mit Turrican-Sprites */
    for (i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &s->enemies[i];
        int ex, ey, flip;
        if (!e->active) continue;
        ex = wx(e->x);
        ey = wy(e->y);
        flip = (e->vx < 0);
        switch (e->kind) {
        case ENEMY_SHOOTER:
            draw_anim(ex, ey, &ENEMY_SHOOT_ANIM, (s->tick + i * 7) / 4, flip);
            break;
        case ENEMY_TANK:
            draw_anim(ex, ey, &ENEMY_TANK_ANIM, (s->tick + i * 5) / 5, flip);
            break;
        default:
            draw_anim(ex, ey, &ENEMY_WALK_ANIM, (s->tick + i * 4) / 4, flip);
            break;
        }
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

    /* Spieler mit lebendigem Turrican-Sprite & Animation */
    if (s->state != STATE_GAMEOVER && !(s->respawn_timer && (s->tick & 4))) {
        int px = wx(s->px), py = wy(s->py);
        int flip = (s->facing < 0);
        if (s->morphing) {
            /* Rotierender Turrican Morph-Ball */
            draw_anim(px, py, &PLAYER_MORPH_ANIM, s->tick / 3, flip);
        } else if (!s->on_ground) {
            /* Sprung: Aim-Up Frame */
            draw_sprite(px, py, &PLAYER_AIM_UP_0, flip);
        } else if (s->pvx != 0) {
            /* Lauf-Animation (14 Frames) */
            draw_anim(px, py, &PLAYER_RUN_ANIM, s->tick / 3, flip);
        } else {
            /* Stand-Pose */
            draw_sprite(px, py, &PLAYER_STAND_0, flip);
        }
        if (s->shield) fill_rect(px - 10, py - 36, 20, 2, C_SHIELD);
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

    if (s->state == STATE_WAVE_INTRO) {
        draw_text(124, 100, "WAVE", C_WHITE);
        draw_number(164, 100, s->wave, C_WHITE);
    }

    /* Game Over & Victory Overlays */
    if (s->state == STATE_GAMEOVER) {
        fill_rect(60, 90, 200, 50, C_HUD_BG);
        fill_rect(62, 92, 196, 46, C_GR_D);
        draw_text(116, 100, "GAME OVER", C_HUD_R);
        draw_text(72, 120, "DRUECKE FEUER ZUM MENUE", C_WHITE);
    }
    if (s->state == STATE_VICTORY) {
        fill_rect(60, 90, 200, 50, C_HUD_BG);
        fill_rect(62, 92, 196, 46, C_GR_D);
        draw_text(124, 100, "VICTORY!", C_GOLD);
        draw_text(72, 120, "DRUECKE FEUER ZUM MENUE", C_WHITE);
    }
}

/* ---- Copper-Setup --------------------------------------------------------- */
static void build_copper(void)
{
    uint16_t *c = copper;
    int i;
    cop_bpl = c;
    for (i = 0; i < NPLANES; i++) {
        *c++ = (uint16_t)(0x0E0 + i * 4);     *c++ = 0;
        *c++ = (uint16_t)(0x0E0 + i * 4 + 2); *c++ = 0;
    }
    for (i = 1; i < 32; i++) { *c++ = (uint16_t)(R_COLOR + i * 2); *c++ = palette[i]; }

    /* Himmel-Verlauf */
    *c++ = R_COLOR; *c++ = sky_bands[0];
    for (i = 1; i < 12; i++) {
        *c++ = (uint16_t)(((0x2C + i * 16) << 8) | 0x01); *c++ = 0xFFFE;
        *c++ = R_COLOR; *c++ = sky_bands[i];
    }
    /* Boden-Zeile */
    *c++ = (uint16_t)(((0x2C + GROUND_SY) << 8) | 0x01); *c++ = 0xFFFE;
    *c++ = R_COLOR; *c++ = 0x000;
    *c++ = 0xFFFF; *c++ = 0xFFFE;
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

/* ---- Tastatur: CIA-A 0xBFEC01 (SDR), 0xBFED01 (ICR), 0xBFEE01 (CRA) ----- */
#define CIAA_ICR (*(volatile uint8_t *)0xBFED01UL)
#define CIAA_SDR (*(volatile uint8_t *)0xBFEC01UL)
#define CIAA_CRA (*(volatile uint8_t *)0xBFEE01UL)

static uint8_t s_keys[128];

static void poll_keyboard(void)
{
    uint8_t icr = CIAA_ICR;
    if (icr & 0x08) {
        uint8_t raw = (uint8_t)~CIAA_SDR;
        uint8_t key = (raw >> 1) & 0x7Fu;
        int released = raw & 1;
        if (key < 128) {
            s_keys[key] = (uint8_t)(!released);
        }
        CIAA_CRA |= 0x40;
        for (volatile int d = 0; d < 40; d++) { }
        CIAA_CRA &= ~0x40;
    }
}

/* ---- Eingabe: Joystick Port 2 + Tastatur + Maus --------------------------- */
static uint16_t read_input(void)
{
    poll_keyboard();

    uint16_t j = REG16(R_JOY1DAT), m = 0;
    int right = (j >> 1) & 1, left = (j >> 9) & 1;
    int down = right ^ ((j >> 0) & 1), up = left ^ ((j >> 8) & 1);

    /* Bewegung: Joystick Port 2, Pfeiltasten (0x4F,0x4E,0x4C,0x4D), A/D/W/S (0x20,0x22,0x11,0x21), Space (0x40) */
    if (left  || s_keys[0x4F] || s_keys[0x20]) m |= INPUT_LEFT;
    if (right || s_keys[0x4E] || s_keys[0x22]) m |= INPUT_RIGHT;
    if (up    || s_keys[0x4C] || s_keys[0x11] || s_keys[0x40]) m |= (INPUT_UP | INPUT_JUMP);
    if (down  || s_keys[0x4D] || s_keys[0x21]) m |= (INPUT_DOWN | INPUT_MORPH);

    /* Feuer: Port-2 Feuer, Enter (0x44), Pad-Enter (0x43), Ctrl (0x63), Port 1 LMB */
    if (!(CIAA_PRA & 0x80) || s_keys[0x44] || s_keys[0x43] || s_keys[0x63] || !(CIAA_PRA & 0x40)) {
        m |= INPUT_FIRE;
    }

    /* 2. Taste / Beam: Port-2 2. Taste, Alt-Links (0x64), Alt-Rechts (0x65), X (0x32) */
    if (!(REG16(R_POTGOR) & (1u << 14)) || s_keys[0x64] || s_keys[0x65] || s_keys[0x32]) {
        m |= INPUT_SECOND;
    }

    /* Waffenwechsel: C (0x33), Tab (0x42) */
    if (s_keys[0x33] || s_keys[0x42]) {
        m |= INPUT_WEAPSW;
    }

    return m;
}

static GameState g_state;
static int       s_running = 1;

int main(void)
{
    int i, front = 0;
    uint8_t *mem;
    uint16_t prev_input = 0;
    unsigned long chip_bytes = (unsigned long)PLANE_BYTES * NPLANES * 2 + COP_WORDS * 2;

    /* AmigaOS CLI-Begruessung anzeigen */
    sys_boot_banner();

    /* Chip-RAM fuer Doppelpuffer + Copper-Liste VOR sys_take anfordern (sauber & OS-konform) */
    mem = (uint8_t *)sys_alloc_chip(chip_bytes);
    if (!mem) return 20;

    if (!sys_take()) {
        sys_free_chip(mem, chip_bytes);
        return 20;
    }

    for (i = 0; i < NPLANES; i++) {
        planes[0][i] = mem + (unsigned long)PLANE_BYTES * i;
        planes[1][i] = mem + (unsigned long)PLANE_BYTES * (NPLANES + i);
    }
    copper = (uint16_t *)(mem + (unsigned long)PLANE_BYTES * NPLANES * 2);

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

    /* Hardware DMA fuer Paula Audio & Copper/Bitplanes aktivieren */
    REG16(R_INTENA) = 0x8000 | 0x4000 | 0x2000;          /* SETCLR | INTEN | EXTER (CIA-B Level 6 fuer P61; kein VERTB da sys_waitvbl gepollt wird) */
    REG16(R_DMACON) = 0x8000 | 0x0200 | 0x0100 | 0x0080 | 0x0040 | 0x000F; /* SETCLR | DMAEN | BPLEN | COPEN | BLTEN | AUD0-3 */

    /* ThePlayer 6.1a Replayer initialisieren mit Chip-RAM Modul (JETZT, wo Hardware aktiv ist!) */
    if (p61_init(module_bin) == 0) {
        s_p61_active = 1;
    }

    while (s_running) {
        uint16_t in = read_input();
        uint16_t trig = in & ~prev_input;     /* Flankenerkennung fuer Menue   */
        prev_input = in;

        draw_idx = front ^ 1;
        memclr(planes[draw_idx][0], (unsigned long)PLANE_BYTES * NPLANES);

        switch (s_ami_state) {
        case AMI_STATE_INTRO:
            render_intro(s_frame_cnt);
            /* Feuer, Space, Enter, LMB, ESC oder Attract-Timeout nach 500 Frames (~10s) -> Menue */
            if ((trig & INPUT_FIRE) || (s_frame_cnt > 500) || s_keys[0x45]) {
                s_ami_state = AMI_STATE_MENU;
                s_keys[0x45] = 0;
            }
            break;

        case AMI_STATE_MENU:
            if (s_keys[0x45]) { /* ESC beendet sauber zur Workbench */
                s_running = 0;
                break;
            }
            if (trig & INPUT_JUMP)  s_menu_sel = (s_menu_sel + 3) % 4;
            if (trig & INPUT_MORPH) s_menu_sel = (s_menu_sel + 1) % 4;
            if (trig & INPUT_FIRE) {
                switch (s_menu_sel) {
                case 0: /* Kampagne */
                    s_mode = MODE_CAMPAIGN;
                    s_load_tick = 0;
                    s_ami_state = AMI_STATE_LOADING;
                    break;
                case 1: /* Endlos */
                    s_mode = MODE_ENDLESS;
                    s_load_tick = 0;
                    s_ami_state = AMI_STATE_LOADING;
                    break;
                case 2: /* Steuerung */
                    s_ami_state = AMI_STATE_CONTROLS;
                    break;
                case 3: /* Credits */
                    s_credit_scroll_y = 0;
                    s_credit_paused = 0;
                    s_ami_state = AMI_STATE_CREDITS;
                    break;
                }
            }
            render_menu(s_menu_sel, s_frame_cnt);
            break;

        case AMI_STATE_CONTROLS:
            render_controls();
            if ((trig & INPUT_FIRE) || s_keys[0x45]) {
                s_ami_state = AMI_STATE_MENU;
                s_keys[0x45] = 0;
            }
            break;

        case AMI_STATE_CREDITS:
            if (s_keys[0x40]) { /* Space toggelt Pause */
                s_credit_paused = !s_credit_paused;
                s_keys[0x40] = 0;
            }
            render_credits(s_frame_cnt, in);
            if ((trig & INPUT_FIRE) || s_keys[0x44] || s_keys[0x43] || s_keys[0x45]) {
                s_ami_state = AMI_STATE_MENU;
                s_keys[0x45] = 0;
                s_keys[0x44] = 0;
                s_keys[0x43] = 0;
            }
            break;

        case AMI_STATE_LOADING:
            s_load_tick++;
            /* Echte Subsystem-Aktionen waehrend der Ladephasen ausfuehren */
            if (s_load_tick == 35) {
                if (s_mode == MODE_CAMPAIGN) {
                    g_state.campaign_level = 1;
                    level_load_campaign(&g_state);
                }
            } else if (s_load_tick == 65) {
                core_init(&g_state, 0x5EEDCAFEu);
                g_state.mode = s_mode;
                if (s_mode == MODE_CAMPAIGN) {
                    g_state.campaign_level = 1;
                    level_load_campaign(&g_state);
                }
            }

            render_loading(s_load_tick);

            if (s_load_tick >= LOAD_TICKS_TOTAL) {
                if (s_mode == MODE_CAMPAIGN) {
                    s_story_scroll = 0;
                    s_story_paused = 0;
                    s_ami_state = AMI_STATE_STORY;
                } else {
                    s_ami_state = AMI_STATE_GAME;
                }
            }
            break;

        case AMI_STATE_STORY:
            if (s_keys[0x40]) { /* Space toggelt Pause */
                s_story_paused = !s_story_paused;
                s_keys[0x40] = 0;
            }
            render_story(s_frame_cnt, in);
            /* Feuer, Enter, Space oder ESC startet die Mission */
            if ((trig & INPUT_FIRE) || s_keys[0x44] || s_keys[0x43] || s_keys[0x45] ||
                (s_story_scroll >= STORY_CRAWL_COUNT * 16 + SCR_H + 40)) {
                s_ami_state = AMI_STATE_GAME;
                s_keys[0x45] = 0;
                s_keys[0x44] = 0;
                s_keys[0x43] = 0;
            }
            break;

        case AMI_STATE_GAME: {
            int px;
            if (s_keys[0x45]) { /* ESC im Spiel -> Zurueck zum Menue */
                s_ami_state = AMI_STATE_MENU;
                s_keys[0x45] = 0;
                break;
            }
            core_tick(&g_state, in);

            px = (int)(g_state.px >> (16 - PX_PER_UNIT_SHIFT));
            cam_x = px - SCR_W / 2;
            if (cam_x < 0) cam_x = 0;
            if (cam_x > WORLD_PX - SCR_W) cam_x = WORLD_PX - SCR_W;

            render_gameplay(&g_state);

            /* Zurueck zum Menue bei Game Over oder Sieg */
            if ((g_state.state == STATE_GAMEOVER || g_state.state == STATE_VICTORY) && (trig & INPUT_FIRE)) {
                s_ami_state = AMI_STATE_MENU;
            }
            break;
        }
        }

        /* 50Hz PAL VBlank Synchronisation & Paula DMA Music Tick */
        sys_waitvbl();
        if (s_p61_active) p61_music();

        front = draw_idx;
        set_front(front);
        s_frame_cnt++;
    }

    /* Paula DMA & Audio beenden */
    if (s_p61_active) p61_end();

    REG16(R_DMACON) = 0x078F;                 /* Bitplane, Copper, Paula aus   */
    sys_free();
    sys_free_chip(mem, chip_bytes);
    return 0;
}
