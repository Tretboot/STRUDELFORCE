/*
 * ============================================================================
 * amiga_hw.c - STRUDELFORCE Amiga 68k Hardware Backport (AGENT 5)
 * ============================================================================
 * PURPOSE:
 *   Thin hardware layer between the sf core (core_sim_sf.c, voxel_render.c)
 *   and a real Amiga 500/OCS:
 *     - 6-plane, 320x256, 32-color bitplane framebuffer in Chip-RAM
 *     - Copper palette (32 of 64 registers, RGB10)
 *     - Paula sample playback: TFMX (Turrican 2) 8-Bit-Stream auf Voice 0
 *       (16574 Hz, 331 Samples/VBlank - nahtloser Chunk-Stream)
 *     - INTB (50Hz) vertical-blank interrupt -> 50Hz Takt + Low-HP-Flag
 *     - Port-A paddle/joystick input, CIA-A DRA bit 1 = 7kHz analog low-pass
 *       (the "underwater" hardware effect, side effect: Power-LED dimming)
 *   Die M68K-Variante enthaelt den einzigen Amiga-Entry-Point (main());
 *   der PC-Build laeuft als Software-Stub mit identischer API (s. unten).
 *
 * BUILD VARIANTS:
 *   #ifdef M68K  -> real hardware registers (m68k-amigaos-gcc, AmigaOS).
 *   #else        -> software stub with the IDENTICAL public API:
 *                    amiga_hw_hardware_present() == 0, all functions no-op,
 *                    rendering into a static 320x256 color-index buffer so the
 *                    PC build links and the render path stays testable.
 *
 * 68K RULES: C99, no malloc (AmigaOS AllocMem/FreeMem only), no VLA,
 *            no float anywhere in this file (pure int32/int64 math).
 *
 * RAM BUDGET (Chip-RAM, Amiga 500 has 512KB):
 *   6 bitplanes .... 6 x 20480 B = 120 KB
 *   Paula sample RAM (frozen engine) ............ 64 KB
 *   Core (GameState pools, statics) ............. ~10 KB
 *   Sample buffer (this file, AllocMem'd) ....... 32 KB
 *   ----------------------------------------------
 *   TOTAL ....................................... ~226 KB < 512 KB Chip  [OK]
 *
 * ORCHESTRATOR-NOTE (Phase 1.1): Voice-Access laeuft ueber den
 * Kontrakt-Typ SF_AudioVoice (sf_interfaces.h) — der frozen tfmx-Typ
 * bleibt intern in cyber_paula.c. Sample-Buffer wird per AllocMem in
 * Chip-RAM gelegt (Paula-DMA kann BSS nicht sicher lesen).
 * ============================================================================
 */
#include "sf_interfaces.h"
#include "voxel_render.h"
#include "tfmx_player.h"

/* ----------------------------------------------------------------------------
 * Clean-room AmigaOS declarations (no OS headers on the 68k target).
 * ---------------------------------------------------------------------------- */
extern void *AllocMem(long size, long flags);
extern void *FreeMem(void *mem, long size);
#define MEMF_CHIP  0x1000L
#define MEMF_CLEAR 0x2000L

/* ----------------------------------------------------------------------------
 * Shared static state (both build variants)
 * ---------------------------------------------------------------------------- */
static uint32_t vblank_count;          /* 50Hz tick counter (INTB on HW)      */
static GameState *state_ptr;           /* core state, used by the vblank path */

/* ----------------------------------------------------------------------------
 * put_px - the single pixel sink.
 * M68K: 6-plane bitplane write. STUB: color-index write into a static buffer.
 * ---------------------------------------------------------------------------- */
#ifdef M68K
/* Plane geometry (OCS, 320x256): one scanline = 40 words = 80 bytes,
 * 256 scanlines -> 20480 bytes per plane. Pixel (px,py) lives in word
 *   base[plane] + py*80 + (px/16)*4, bit = 15 - (px & 15)  (MSB = leftmost). */
static uint32_t *plane[6];
static uint8_t  low_hp;                      /* interlace-jitter flag (50Hz)   */

static void put_px(int x, int y, int c)
{
    int p, bit;
    if (plane[0] == 0) return;   /* headless: Chip-RAM allocation failed */
    if (x < 0 || x > 319 || y < 0 || y > 255) return;
    /* Low-HP interlace look: shift odd rows by 1px (classic Amiga jitter). */
    if (low_hp && (y & 1)) x = (x + 1) & 319;
    bit = 15 - (x & 15);
    for (p = 0; p < 6; p++) {
        volatile uint32_t *w =
            plane[p] + (size_t)y * 40 + (size_t)(x >> 4);
        if (c & (1 << p)) *w |= (uint32_t)(1u << bit);
        else               *w &= ~(uint32_t)(1u << bit);
    }
}
#else
static uint8_t stub_fb[256 * 320];     /* color-index framebuffer (PC link) */

static void put_px(int x, int y, int c)
{
    if (x < 0 || x > 319 || y < 0 || y > 255) return;
    stub_fb[y * 320 + x] = (uint8_t)(c & 31);
}
#endif

/* ----------------------------------------------------------------------------
 * Shared software raster (identical in both variants):
 * World 1280x720 -> screen 320x256: sx = wx >> 2, sy = wy / 3 (int math).
 * Parallax per layer (camera = player.x - 640 world px):
 *   layer 3 (Z=-20): cam >> 5   (1/32)
 *   layer 2 (Z=-6) : cam >> 2   (1/4)
 *   layer 1 (Z=0)  : cam        (1:1)
 *   layer 0 (Z=+2) : cam * 3/2  (1.5x foreground)
 * Item kinds: 0/2 = filled rect, 1 = sprite, 3 = 1-2px dot, 4 = UI (skipped).
 * Plan order is back-to-front, so items are drawn in plan order (no z-sort).
 * ---------------------------------------------------------------------------- */
static void draw_rect(int x, int y, int w, int h, int c)
{
    int i, j;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x > 319 || y > 255 || w <= 0 || h <= 0) return;
    if (x + w > 320) w = 320 - x;
    if (y + h > 256) h = 256 - y;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            put_px(i, j, c);
}

static void draw_sprite(const SpriteDef *sp, int x, int y)
{
    int i, j;
    for (j = 0; j < (int)sp->h; j++) {
        for (i = 0; i < (int)sp->w; i++) {
            uint8_t c = sp->colors[j * (int)sp->w + i];
            if (c) put_px(x + i, y + j, c);   /* 0 = transparent */
        }
    }
}

int amiga_hw_render(const VoxelRenderItem *items, uint16_t n, const GameState *s)
{
    int cam, drawn = 0, i;

    cam = sf_unfix(s->player.x) - 640;

    for (i = 0; i < (int)n; i++) {
        const VoxelRenderItem *it = &items[i];
        int wx = sf_unfix(it->x), wy = sf_unfix(it->y);
        int ww = sf_unfix(it->w), wh = sf_unfix(it->h);
        int sx, sy, sw, sh, off;

        if (it->kind == 4) continue;         /* UI: PC-only, not on Amiga */

        switch (it->layer) {
        case 3:  off = cam >> 5;             break;
        case 2:  off = cam >> 2;             break;
        case 1:  off = cam;                  break;
        default: off = (int)(((int32_t)cam * 3) / 2); break;
        }

        sx = (wx >> 2) - off;
        sy = wy / 3;
        sw = ww >> 2;
        sh = wh / 3;

        if (it->kind == 1) {                 /* sprite, 1:1 screen pixels */
            const SpriteDef *sp;
            if (it->pal == 16)      sp = &sprite_player;
            else if (it->pal == 12) sp = &sprite_chest;
            else if (it->pal == 20) sp = &sprite_crystal;
            else                    sp = &sprite_grunt;
            draw_sprite(sp, sx, sy);
        } else if (it->kind == 3) {          /* particle / debris dot */
            put_px(sx, sy, it->pal & 31);
            put_px(sx + 1, sy, it->pal & 31);
        } else {                             /* kind 0 block, kind 2 laser */
            draw_rect(sx, sy, sw, sh, it->pal & 31);
        }
        drawn++;
    }
    return drawn;
}

/* ============================================================================
 * M68K HARDWARE VARIANT
 * ============================================================================ */
#ifdef M68K

/* ----------------------------------------------------------------------------
 * Clean-room hardware register map (Amiga 500/OCS).
 * ---------------------------------------------------------------------------- */
#define HW_PAULA_BASE   0xDFF0A0u           /* AUDxLCH+0 .. AUDxST+6, step 8  */
#define HW_COPPER_PAL   0xDFF090u           /* 64 x 32-bit RGB10 registers    */
#define HW_PLN0         0x00015000u         /* bitplane base addresses        */
#define HW_PLN_STRIDE   8u                  /* PLN0..PLN5, 8 bytes apart      */
#define HW_DMAPR        0x00FC52u           /* DMA enable (16-bit)            */
#define HW_INTBENAR_IE  0x00F148u           /* INTB enable  (16-bit)          */
#define HW_INTBENAR_IC  0x00F14Au           /* INTB clear   (8-bit)           */
#define HW_INTB_BIT     0x10u               /* bit 4 = vertical blank         */
#define HW_INTB_VECTOR  25                   /* vector table @0x20 + 25*4=0xE4 */
#define HW_JOY_PADDLE_X 0xBFE201u            /* Port A, 16-bit paddle read     */
#define HW_JOY_PADDLE_Y 0xBFE202u
#define HW_JOY_FIRE     0xBFE201u            /* 8-bit read, bit 7 = fire       */
#define HW_CIA_A_DRA    0xBFE001u            /* bit 1 = 7kHz analog low-pass   */

#define PLANE_BYTES 20480                    /* 40 words * 256 scanlines      */
#define PAULA_SAMPLE_BUF 32768               /* 32KB own sample RAM (chip)    */

/* 32-color palette, RGB16 (identical to main_pc.c so PC/Amiga match). */
static const int32_t sf_palette_rgb16[32][3] = {
    {    0,     0,     0}, {65536,     0,     0}, {    0, 65536,     0}, {65536, 65536,     0},
    {    0,     0, 65536}, {65536,     0, 65536}, {    0, 65536, 65536}, {65536, 65536, 65536},
    {32768,     0,     0}, {32768, 32768,     0}, {    0, 32768, 32768}, {32768,     0, 32768},
    {32768, 65536, 32768}, {65536, 32768,     0}, {32768, 65536, 65536}, {65536, 65536, 32768},
    {16384,     0,     0}, {16384, 16384,     0}, {    0, 16384, 16384}, {16384,     0, 16384},
    {16384, 32768, 16384}, {32768, 16384,     0}, {16384, 32768, 16384}, {32768, 32768, 16384},
    { 8192,     0,     0}, { 8192,  8192,     0}, {    0,   8192,   8192}, { 8192,     0,  8192},
    { 8192, 16384,  8192}, {16384,  8192,     0}, { 8192, 16384, 16384}, {16384, 16384,  8192}
};

static int32_t  sf_palette_rgb10[32];        /* packed (r<<20)|(g<<10)|b, 10-bit */
static int8_t  *paula_sample_buf;            /* 32KB Chip-RAM (AllocMem'd)     */

static void sf_vblank_handler(void);         /* 68k interrupt entry           */
extern void *SetVec(int vec, void *func);    /* clean-room AmigaOS call       */

static GameState g_state;                    /* core state (BSS, ~10KB)       */

void amiga_hw_init(void)
{
    int i;
    int planes_ok = 1;

    /* Best-effort Chip-RAM allocation for the 6 bitplanes (NULL-checked). */
    for (i = 0; i < 6; i++) {
        plane[i] = (uint32_t *)AllocMem(PLANE_BYTES, MEMF_CHIP | MEMF_CLEAR);
        if (plane[i] == 0) { planes_ok = 0; break; }
    }
    if (planes_ok) {
        /* Point the 68k bitplane DMA at our Chip-RAM (not the default ALN0-5). */
        for (i = 0; i < 6; i++)
            *(volatile uint32_t *)(HW_PLN0 + HW_PLN_STRIDE * (uint32_t)i) =
                (uint32_t)(uintptr_t)plane[i];
        *(volatile uint16_t *)HW_DMAPR = 0x00F0;   /* DMA: bitplanes only */
    } else {
        /* Chip-RAM exhausted: free what we got; render runs headless (no-op).
         * Core + audio still initialize so the game keeps running. */
        for (i = 0; i < 6; i++) {
            if (plane[i]) { FreeMem(plane[i], PLANE_BYTES); plane[i] = 0; }
        }
    }

    /* Paula-Sample-Puffer: eigenes 32KB Chip-RAM (Paula-DMA!).
     * HW-RISK (dokumentiert): Layout-Kollision mit dem 64KB-Sample-RAM der
     * frozen Engine prüfen, wenn beide gleichzeitig aktiv sind — Phase 2a. */
    paula_sample_buf = (int8_t *)AllocMem(PAULA_SAMPLE_BUF, MEMF_CHIP | MEMF_CLEAR);

    /* Copper palette: 32 colors, 10-bit channels, high bits left-shifted. */
    for (i = 0; i < 32; i++) {
        int32_t r = sf_palette_rgb16[i][0] >> 5;
        int32_t g = sf_palette_rgb16[i][1] >> 5;
        int32_t b = sf_palette_rgb16[i][2] >> 5;
        sf_palette_rgb10[i] = (r << 20) | (g << 10) | b;
        *(volatile uint32_t *)(HW_COPPER_PAL + 4u * (uint32_t)i) =
            (uint32_t)sf_palette_rgb10[i];
    }

    state_ptr = 0;
    vblank_count = 0;
    low_hp = 0;

    /* INTB (50Hz) vertical-blank interrupt: vector 25, enable bit 4. */
    SetVec(HW_INTB_VECTOR, (void *)sf_vblank_handler);
    *(volatile uint16_t *)HW_INTBENAR_IE |= HW_INTB_BIT;
    *(volatile uint8_t  *)HW_INTBENAR_IC  = HW_INTB_BIT;

    core_init(&g_state, SF_BIOME_CHROME_TEMPEST, 0x5EEDCAFEu);
    state_ptr = &g_state;
}

/* 68k interrupt contract: D0-D7, A0-A1 and SR are auto-saved by the CPU on
 * trigger. We additionally save D0-D7 (redundant, safe) + A2-A6 with the
 * standard prologue pattern and mirror-restore before RTS (RTS pops the
 * CPU's auto-save frame). */
static void sf_vblank_handler(void)
{
    asm volatile(
        "move.l %%d0,-(%%sp);move.l %%d1,-(%%sp);move.l %%d2,-(%%sp);"
        "move.l %%d3,-(%%sp);move.l %%d4,-(%%sp);move.l %%d5,-(%%sp);"
        "move.l %%d6,-(%%sp);move.l %%a2,-(%%sp);move.l %%a3,-(%%sp);"
        "move.l %%a4,-(%%sp);move.l %%a5,-(%%sp);move.l %%a6,-(%%sp)"
    );
    *(volatile uint8_t *)HW_INTBENAR_IC = HW_INTB_BIT;   /* clear INTB first */
    amiga_hw_vblank();
    asm volatile(
        "move.l (%%sp)+,%%a6;move.l (%%sp)+,%%a5;move.l (%%sp)+,%%a4;"
        "move.l (%%sp)+,%%a3;move.l (%%sp)+,%%a2;move.l (%%sp)+,%%d6;"
        "move.l (%%sp)+,%%d5;move.l (%%sp)+,%%d4;move.l (%%sp)+,%%d3;"
        "move.l (%%sp)+,%%d2;move.l (%%sp)+,%%d1;move.l (%%sp)+,%%d0;"
        "rts"
    );
}

void amiga_hw_vblank(void)
{
    vblank_count++;

    /* Low-HP-Flag ( < 25% der 100 HP ) fuer den Interlace-Jitter in put_px. */
    low_hp = (state_ptr && state_ptr->player.hp * 4 < 100) ? 1 : 0;
}

void amiga_hw_set_underwater(uint8_t on)
{
    /* CIA-A DRA bit 1: engages the analog 7kHz hardware low-pass filter
     * (the "underwater" muffled sound). Side effect: dims the Power-LED. */
    if (on) *(volatile uint8_t *)HW_CIA_A_DRA &= ~0x02u;
    else   *(volatile uint8_t *)HW_CIA_A_DRA |=  0x02u;
}

void amiga_hw_play_sample(const int8_t *data, uint16_t len, uint8_t voice, uint8_t loop)
{
    volatile uint8_t *a;
    uint16_t i;
    if (data == 0 || len == 0 || paula_sample_buf == 0) return;
    if (len > PAULA_SAMPLE_BUF) len = PAULA_SAMPLE_BUF;
    for (i = 0; i < len; i++) paula_sample_buf[i] = data[i];
    a = (volatile uint8_t *)(HW_PAULA_BASE + 8u * (uint32_t)(voice & 7));
    a[0x00] = 0; a[0x01] = 0;               /* AUDxLCH/LCL (Buffer @ Chip-Basis) */
    a[0x02] = 81;                           /* AUDxPER: 7159000/(2*44100)~81 */
    a[0x03] = 0;                            /* AUDxMOD: no modulation       */
    a[0x04] = 63;                           /* AUDxVOL: max                */
    a[0x05] = loop ? 0x00 : 0xFF;           /* AUDxPTM: loop@0 / no loop   */
    a[0x06] = 0x1F;                         /* AUDxST: play                */
}

uint8_t amiga_hw_hardware_present(void) { return 1; }

/* ----------------------------------------------------------------------------
 * Port-A paddle/joystick -> core input mask.
 * 16-bit paddle: 0..65535, center ~32768. Thresholds 12000/53000 give a
 * comfortable dead-zone. Paddle-Y < 12000 = up (JUMP). Fire = 8-bit bit 7.
 * ---------------------------------------------------------------------------- */
static uint16_t read_paddles(void)
{
    uint16_t m = 0;
    uint16_t px = *(volatile uint16_t *)HW_JOY_PADDLE_X;
    uint16_t py = *(volatile uint16_t *)HW_JOY_PADDLE_Y;
    uint8_t  fire = *(volatile uint8_t *)HW_JOY_FIRE;
    if (px < 12000)      m |= SF_INPUT_LEFT;
    else if (px > 53000) m |= SF_INPUT_RIGHT;
    if (py < 12000)      m |= SF_INPUT_JUMP;
    if (fire & 0x80)     m |= SF_INPUT_FIRE;
    return m;
}

/* ----------------------------------------------------------------------------
 * TFMX-Musik (Turrican 2) auf Paula Voice 0, 8-Bit-Stream:
 *   16574 Hz (PAL-50Hz nativ) * 331 Samples/VBlank = exakt 1 Tick.
 *   Der Player rendert 16-Bit (bit-genauer Python-Oracle-Mirror); wir
 *   quantisieren mit 4x-Gain auf Paula-8-Bit (TFMX-Peak ~7800 -> ~120/127).
 *   Stream-Prinzip: LCH=0, PTM=330 (Loop-Fenster), pro VBlank Buffer neu
 *   + exakt am VBlank neu triggern -> nahtloser Chunk-Stream.
 * ---------------------------------------------------------------------------- */
#define TFMX_STREAM_SAMPLES 331      /* 16574 / 50                                */
#define TFMX_PAULA_PER      216      /* 3579500 / 16574 = 215.96                  */
#include "amiga_music.h"  /* EMBEDdete TFMX-Tracks (INCBIN, kein stdio) */

#define TFMX_TRACKS T2_TRACK_COUNT

static TfmxPlayer s_tfmx;
static int        s_tfmx_loaded = 0;
static int        s_track;

/* Track-Liste: Zeiger auf die INCBIN-Daten in .rodata (amiga_music.c) */
static const struct {
    const uint8_t *tfx;
    uint32_t       tfx_len;
    const uint8_t *sam;
    uint32_t       sam_len;
    const char    *name;
} s_tracks[TFMX_TRACKS] = {
    { t2_tfx[0], t2_tfx_len[0], t2_sam[0], t2_sam_len[0], t2_names[0] },
    { t2_tfx[1], t2_tfx_len[1], t2_sam[1], t2_sam_len[1], t2_names[1] },
    { t2_tfx[2], t2_tfx_len[2], t2_sam[2], t2_sam_len[2], t2_names[2] },
    { t2_tfx[3], t2_tfx_len[3], t2_sam[3], t2_sam_len[3], t2_names[3] }
};

/* Tastatur direkt ueber CIA-A (kein DOS/Library-Abhaengigkeit):
 * $BFE001 bit0 = Taste gedrueckt, $BFE000 = Tastencode. */
static int cia_key_present(void)
{
    return ((int)(*(volatile uint8_t *)HW_CIA_A_DRA) & 0x01);
}
static int cia_key_get(void)
{
    return (int)(*(volatile uint8_t *)(HW_CIA_A_DRA - 1u));
}

/* Kein stdio auf der Ziel-Plattform: Log-Ausgabe ist ein No-Op.
 * Fuer Debugging in der Bartman-Extension spaeter hier
 * KPrintF()/debug_text() einhaengen (siehe src/amiga_crt.h-Kommentar). */
static void amiga_log(const char *msg)
{
    (void)msg;
}

static void tfmx_stream_start(int idx)
{
    if (idx < 0 || idx >= TFMX_TRACKS) return;
    if (tfmx_player_load_mem(&s_tfmx,
                             s_tracks[idx].tfx, s_tracks[idx].tfx_len,
                             (const int8_t *)s_tracks[idx].sam, s_tracks[idx].sam_len)) {
        tfmx_player_start(&s_tfmx, 0);
        s_tfmx_loaded = 1;
        s_track = idx;
        amiga_log(s_tracks[idx].name);   /* [M = naechster Track] */
    } else {
        s_tfmx_loaded = 0;
        amiga_log("Track nicht lesbar");
    }
}

static void tfmx_paula_stream(void)
{
    static int16_t mix16[TFMX_STREAM_SAMPLES];  /* static: 68k Stack ist knapp */
    volatile uint8_t *a = (volatile uint8_t *)(HW_PAULA_BASE + 0u);
    int i;

    if (!s_tfmx_loaded || paula_sample_buf == 0) return;

    /* render() treibt den Jiffy-Takt selbst (CIA-Modell) - kein eigener Call */
    tfmx_player_render(&s_tfmx, mix16, TFMX_STREAM_SAMPLES);

    /* 16 -> 8 Bit, 4x Gain, Half-Sample-Rounding, Clamp auf +/-127 */
    for (i = 0; i < TFMX_STREAM_SAMPLES; i++) {
        int32_t g = (int32_t)mix16[i] * 4;
        g += (g < 0) ? -128 : 128;
        paula_sample_buf[i] = (int8_t)(g >> 8);
    }

    a[0x00] = 0x00;                          /* AUD0LCH = 0          */
    a[0x01] = 0x00;                          /* AUD0LCL = 0          */
    a[0x02] = TFMX_PAULA_PER;                /* AUD0PER = 216        */
    a[0x03] = 0x00;                          /* AUD0MOD = 0          */
    a[0x04] = 48;                            /* AUD0VOL = 48/63      */
    a[0x05] = (uint8_t)(TFMX_STREAM_SAMPLES - 1); /* AUD0PTM = 330   */
    a[0x06] = 0x1F;                          /* AUD0ST = play        */
}

/* ----------------------------------------------------------------------------
 * Boot-/Title-Screen (2.5 Sekunden, Fire zum Ueberspringen):
 * 2-Ton-Titel (Orange ueber Rot, dunkelroter Schlagschatten), Diskette-Zeile
 * und blinkender "PRESS FIRE" - im Stil des PC-Hauptmenues, auf 320x256
 * mit 6 Bitplanes (10-Farben-OCS-Palette). Nur statische Teile 1x zeichnen,
 * dann nur die blinkende Zeile pro VBlank (Chip-RAM-schonend).
 * Paletten-Indizes: 0=Schwarz 1=Rot 8=Dunkelrot 13=Orange 25=Dunkelorange
 * ---------------------------------------------------------------------------- */
#include "retro_font.h"

#define BOOT_FRAMES 125                     /* 2.5 s @ 50 Hz                */
#define BOOT_TITLE_X 16                     /* 12 Zeichen * 3 * 8 px = 288  */
#define BOOT_TITLE_Y 64
#define BOOT_SUB_Y   108
#define BOOT_DISK_Y  140
#define BOOT_FIRE_Y  192

static uint32_t boot_tick_cnt;              /* 0 = noch nicht gestartet      */

static int boot_active(void) { return (int)(boot_tick_cnt < (uint32_t)BOOT_FRAMES); }

static void boot_char2(int x, int y, char ch, int top, int bot, int s)
{
    int row, col;
    if ((unsigned char)ch >= 128) ch = '?';
    for (row = 0; row < 8; row++) {
        uint8_t bits = RETRO_FONT[(unsigned char)ch][row];
        int c = (row < 4) ? top : bot;
        for (col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                int dx, dy;
                for (dy = 0; dy < s; dy++)
                    for (dx = 0; dx < s; dx++)
                        put_px(x + col * s + dx, y + row * s + dy, (uint8_t)c);
            }
        }
    }
}

static void boot_text(int x, int y, const char *t, int c)
{
    int i;
    for (i = 0; t[i]; i++) boot_char2(x + i * 8, y, t[i], c, c, 1);
}

static void boot_draw_static(void)
{
    int i;
    /* Copper-Baender (dunkelrot/dunkelorange, dezent) */
    draw_rect(0, 36, 320, 2, 8);
    draw_rect(0, 42, 320, 2, 25);
    draw_rect(0, 214, 320, 2, 8);
    /* Titel: Schlagschatten + 2 Toene (pro Zeichen leichtes Wanken) */
    for (i = 0; i < 12; i++) {
        int ox = BOOT_TITLE_X + i * 24;
        int oy = BOOT_TITLE_Y + ((i % 2 == 0) ? 0 : 2);
        boot_char2(ox + 2, oy + 2, "STRUDELFORCE"[i], 8, 8, 3);
        boot_char2(ox, oy, "STRUDELFORCE"[i], 13, 1, 3);
    }
    boot_text(16, BOOT_SUB_Y, "A TURRICAN TRIBUTE FOR THE AMIGA 500", 25);
    boot_text(16, BOOT_DISK_Y, "DISK 1 : STRUDELFORCE.ADF (SINGLE DISC)", 13);
    boot_text(16, 232, "(C) 2026 FRANK MITZKATIS & AI CODING AGENTS", 8);
}

/* Ein 50Hz-Boot-Frame; ruft main() bei jedem VBlank im Boot-Modus. */
void amiga_hw_boot_frame(void)
{
    boot_tick_cnt++;
    if (boot_tick_cnt == 1) {
        boot_draw_static();
        return;                              /* Planes sind MEMF_CLEAR=0 */
    }
    /* Blinkende Zeile: Rect loeschen, alle 25 Ticks 1x anzeigen */
    draw_rect(88, BOOT_FIRE_Y - 2, 150, 12, 0);
    if ((boot_tick_cnt / 25) % 2 == 1)
        boot_text(92, BOOT_FIRE_Y, "PRESS FIRE TO START", 13);
}

void amiga_hw_boot_skip(void) { boot_tick_cnt = (uint32_t)(BOOT_FRAMES + 1); }

int main(void)
{
    static VoxelRenderItem items[1024];      /* static: 68k stack is precious */
    uint32_t last_vb;
    uint16_t n;
    int i;
    int quit = 0;

    amiga_log("STRUDELFORCE (Amiga 500 OCS / 50Hz PAL) - TFMX Paula-Stream");

    amiga_hw_init();
    voxel_init();                             /* build sprite tables once   */
    last_vb = vblank_count;

    for (i = 0; i < TFMX_TRACKS; i++) {
        tfmx_stream_start(i);
        if (s_tfmx_loaded) break;
    }

    for (;;) {
        /* Busy-wait fuer den naechsten 50Hz VBlank (INTB zaehlt vblank_count). */
        while (vblank_count == last_vb) { }
        last_vb = vblank_count;

        /* Tastatur (non-blocking, CIA-A): M = naechster Track, Q/ESC = beenden */
        if (cia_key_present()) {
            int c = cia_key_get();
            if (c == 'm' || c == 'M') {
                tfmx_stream_start((s_track + 1) % TFMX_TRACKS);
            } else if (c == 'q' || c == 'Q' || c == 27) {
                quit = 1;
            }
        }
        if (quit) break;

        /* Boot-/Title-Screen: Core bleibt gefroren, Fire/Enter springt ueber */
        if (boot_active()) {
            if (read_paddles() & SF_INPUT_FIRE) {
                amiga_hw_boot_skip();
            } else {
                amiga_hw_boot_frame();
            }
            tfmx_paula_stream();             /* Titelmusik laeuft mit       */
            continue;                        /* kein Core-Tick im Boot      */
        }

        core_tick(state_ptr, read_paddles());
        tfmx_paula_stream();

        /* Great-Bath-Underwater + Low-HP: analoger 7kHz-Tiefpass (CIA-A) */
        amiga_hw_set_underwater(
            (uint8_t)(sf_biomedefs[state_ptr->biome].underwater ||
                      state_ptr->player.hp * 4 < 100));

        n = voxel_build_render_plan(state_ptr, items, 1024);
        amiga_hw_render(items, n, state_ptr);
    }

    /* Paula stoppen + TFMX raedigen */
    *(volatile uint8_t *)(HW_PAULA_BASE + 6u) = 0x00;   /* AUD0ST stop */
    if (s_tfmx_loaded) {
        tfmx_player_stop(&s_tfmx);
        tfmx_player_free(&s_tfmx);
    }
    amiga_log("STRUDELFORCE beendet");
    return 0;
}

#endif /* M68K */

/* ============================================================================
 * SOFTWARE STUB VARIANT (PC link) - identical API, no hardware.
 * ============================================================================ */
#ifndef M68K

void amiga_hw_init(void)
{
    int i;
    for (i = 0; i < (int)(sizeof(stub_fb) / sizeof(stub_fb[0])); i++)
        stub_fb[i] = 0;
    vblank_count = 0;
    state_ptr = 0;
}

void amiga_hw_vblank(void)
{
    vblank_count++;
}

void amiga_hw_set_underwater(uint8_t on)
{
    (void)on;   /* no hardware low-pass on PC */
}

void amiga_hw_play_sample(const int8_t *data, uint16_t len, uint8_t voice, uint8_t loop)
{
    (void)data; (void)len; (void)voice; (void)loop;   /* no-op on PC */
}

uint8_t amiga_hw_hardware_present(void) { return 0; }

#endif /* !M68K */
