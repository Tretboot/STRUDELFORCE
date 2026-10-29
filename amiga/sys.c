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
    return AllocMem(bytes, MEMF_CHIP | MEMF_CLEAR);
}

void sys_boot_banner(void)
{
    DOSBase = (struct DosLibrary *)OpenLibrary((CONST_STRPTR)"dos.library", 0);
    if (DOSBase) {
        const char banner[] =
            "\n\x1b[33m==================================================\n"
            "  StrudelOS v0.3.1 - Amiga 500 Bare-Metal Edition\n"
            "  (C) 2026 Frank Mitzkatis & AI Team - Turrican Tribute\n"
            "==================================================\n\x1b[32m"
            "  [*] System: 512KB Chip-RAM + OCS/ECS Blitter\n"
            "  [*] Audio:  Paula 4-Channel DMA Hardware Sound\n"
            "  [*] Core:   16.16 Fixed-Point (50Hz PAL)\n"
            "  [*] Starte Strudelforce auf Custom-Chipset...\n\n\x1b[0m";
        Write(Output(), (APTR)banner, sizeof(banner) - 1);
        Delay(35);
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

