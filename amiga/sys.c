/*
 * sys.c - STRUDELFORCE Amiga: System-Takeover / -Release (OS-Seite)
 * ------------------------------------------------------------------
 * Getrennt von main.c, damit die AmigaOS-Header (UBYTE/ULONG/...) nicht mit
 * den Typen des Spielkerns (strudelforce.h) kollidieren. Muster nach dem
 * Bartman-Template (Amiga C/C++ Compile, Debug & Profile): Forbid, LoadView(0),
 * Interrupts/DMA sichern, alles abschalten - und beim Beenden restaurieren.
 */
#include "support/gcc8_c_support.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <exec/execbase.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>

struct ExecBase *SysBase;
volatile struct Custom *custom;
struct GfxBase *GfxBase;
struct DosLibrary *DOSBase;

typedef struct {
    int is_aga;
    int is_ecs;
    int cpu_type;
    unsigned long chip_kb;
    unsigned long fast_kb;
    char summary[40];
} AmiHwInfo;

AmiHwInfo g_ami_hw;

void sys_detect_hw(void)
{
    unsigned long chip_bytes, fast_bytes;
    int chip_kb;
    const char *chipset_str = "OCS";
    const char *cpu_str = "68000";
    const char *ram_str = "512KB CHIP";

    SysBase = *((struct ExecBase **)4UL);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 0);

    /* Chipset */
    g_ami_hw.is_aga = 0;
    g_ami_hw.is_ecs = 0;
    if (GfxBase) {
        UBYTE rev = GfxBase->ChipRevBits0;
        if (rev & (GFXF_AA_ALICE | GFXF_AA_LISA | GFXF_AA_MLISA)) {
            g_ami_hw.is_aga = 1;
            chipset_str = "AGA";
        } else if (rev & (GFXF_HR_AGNUS | GFXF_HR_DENISE)) {
            g_ami_hw.is_ecs = 1;
            chipset_str = "ECS";
        }
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = 0;
    }

    /* CPU */
    if (SysBase->AttnFlags & AFF_68060) {
        g_ami_hw.cpu_type = 68060;
        cpu_str = "68060";
    } else if (SysBase->AttnFlags & AFF_68040) {
        g_ami_hw.cpu_type = 68040;
        cpu_str = "68040";
    } else if (SysBase->AttnFlags & AFF_68030) {
        g_ami_hw.cpu_type = 68030;
        cpu_str = "68030";
    } else if (SysBase->AttnFlags & AFF_68020) {
        g_ami_hw.cpu_type = 68020;
        cpu_str = "68020";
    } else if (SysBase->AttnFlags & AFF_68010) {
        g_ami_hw.cpu_type = 68010;
        cpu_str = "68010";
    } else {
        g_ami_hw.cpu_type = 68000;
        cpu_str = "68000";
    }

    /* Memory */
    chip_bytes = AvailMem(MEMF_CHIP);
    fast_bytes = AvailMem(MEMF_FAST);
    chip_kb = (int)(chip_bytes / 1024UL);
    g_ami_hw.chip_kb = chip_kb;
    g_ami_hw.fast_kb = (int)(fast_bytes / 1024UL);

    if (chip_kb >= 1700) {
        ram_str = "2MB CHIP";
    } else if (chip_kb >= 850) {
        ram_str = "1MB CHIP";
    } else {
        ram_str = "512KB CHIP";
    }

    /* Summary string (<= 30 chars): e.g. "SYS: 512KB CHIP | 68000 | OCS" */
    {
        char *p = g_ami_hw.summary;
        const char *s = "SYS: ";
        while (*s) *p++ = *s++;
        s = ram_str;
        while (*s) *p++ = *s++;
        *p++ = ' '; *p++ = '|'; *p++ = ' ';
        s = cpu_str;
        while (*s) *p++ = *s++;
        *p++ = ' '; *p++ = '|'; *p++ = ' ';
        s = chipset_str;
        while (*s) *p++ = *s++;
        *p = '\0';
    }
}

static UWORD SystemInts;
static UWORD SystemDMA;
static UWORD SystemADKCON;
static volatile APTR VBR = 0;
static APTR SystemIrq;
static struct View *ActiView;

static APTR GetVBR(void)
{
    APTR vbr = 0;
    UWORD getvbr[] = { 0x4e7a, 0x0801, 0x4e73 }; /* MOVEC.L VBR,D0 ; RTE */
    if (SysBase->AttnFlags & AFF_68010)
        vbr = (APTR)Supervisor((ULONG (*)())getvbr);
    return vbr;
}

/* VBlank-Beginn: vpos 312 hpos 1 (PAL). Flankenerkennung wie im Template. */
void sys_waitvbl(void)
{
    debug_start_idle();
    while (1) {
        volatile ULONG vpos = *(volatile ULONG *)0xDFF004;
        vpos &= 0x1ff00;
        if (vpos != (311 << 8)) break;
    }
    while (1) {
        volatile ULONG vpos = *(volatile ULONG *)0xDFF004;
        vpos &= 0x1ff00;
        if (vpos == (311 << 8)) break;
    }
    debug_stop_idle();
}

void *sys_alloc_chip(unsigned long bytes)
{
    SysBase = *((struct ExecBase **)4UL);
    return AllocMem(bytes, MEMF_CHIP | MEMF_CLEAR);
}

void sys_free_chip(void *ptr, unsigned long bytes)
{
    if (ptr) {
        SysBase = *((struct ExecBase **)4UL);
        FreeMem(ptr, bytes);
    }
}

void sys_boot_banner(void)
{
    sys_detect_hw();
    SysBase = *((struct ExecBase **)4UL);
    custom = (struct Custom *)0xdff000;
    DOSBase = (struct DosLibrary *)OpenLibrary((CONST_STRPTR)"dos.library", 0);
    if (DOSBase) {
        BPTR out = Output();
        if (out) {
            char banner[512];
            char *p = banner;
            const char b1[] =
                "\n\x1b[33m==================================================\n"
                "  StrudelOS v0.3.1 - Amiga 500/1200 Bare-Metal\n"
                "  (C) 2026 Frank Mitzkatis & AI Team - Turrican Tribute\n"
                "==================================================\n\x1b[32m"
                "  [*] ";
            const char b2[] =
                "\n  [*] Audio:  Paula 4-Channel DMA Hardware Sound\n"
                "  [*] Core:   16.16 Fixed-Point (50Hz PAL)\n"
                "  [*] Starte Strudelforce auf Custom-Chipset...\n\n\x1b[0m";
            const char *s = b1;
            while (*s) *p++ = *s++;
            s = g_ami_hw.summary;
            while (*s) *p++ = *s++;
            s = b2;
            while (*s) *p++ = *s++;
            *p = '\0';
            Write(out, (APTR)banner, (LONG)(p - banner));
            Delay(25);
        }
        CloseLibrary((struct Library *)DOSBase);
        DOSBase = 0;
    }
}

int sys_take(void)
{
    SysBase = *((struct ExecBase **)4UL);
    custom = (struct Custom *)0xdff000;
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR) "graphics.library", 0);
    if (!GfxBase) return 0;

    Forbid();
    SystemADKCON = custom->adkconr;
    SystemInts = custom->intenar;
    SystemDMA = custom->dmaconr;
    ActiView = GfxBase->ActiView;

    LoadView(0);
    WaitTOF();
    WaitTOF();
    sys_waitvbl();
    sys_waitvbl();

    OwnBlitter();
    WaitBlit();
    Disable();

    custom->intena = 0x7fff;
    custom->intreq = 0x7fff;
    custom->dmacon = 0x7fff;
    {
        int a;
        for (a = 0; a < 32; a++) custom->color[a] = 0;
    }
    sys_waitvbl();
    sys_waitvbl();

    VBR = GetVBR();
    SystemIrq = *(volatile APTR *)(((UBYTE *)VBR) + 0x6c);
    return 1;
}

void sys_free(void)
{
    sys_waitvbl();
    WaitBlit();
    custom->intena = 0x7fff;
    custom->intreq = 0x7fff;
    custom->dmacon = 0x7fff;

    *(volatile APTR *)(((UBYTE *)VBR) + 0x6c) = SystemIrq;

    custom->cop1lc = (ULONG)GfxBase->copinit;
    custom->cop2lc = (ULONG)GfxBase->LOFlist;
    custom->copjmp1 = 0x7fff;

    custom->intena = SystemInts | 0x8000;
    custom->dmacon = SystemDMA | 0x8000;
    custom->adkcon = SystemADKCON | 0x8000;

    WaitBlit();
    DisownBlitter();
    Enable();

    LoadView(ActiView);
    WaitTOF();
    WaitTOF();
    Permit();
    CloseLibrary((struct Library *)GfxBase);
}

