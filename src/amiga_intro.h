/*
 * amiga_intro.h - STRUDELFORCE Amiga 500 Terminal Intro
 *
 * Simulierte Amiga-Copperbar-Hintergrundgrafik und ein Sinus-tanzender
 * Text-Scroller im ANSI/ASCII-Terminal-Modus (50Hz PAL).
 *
 * Lizenz: MIT (Clean Room)
 */

#ifndef AMIGA_INTRO_H
#define AMIGA_INTRO_H

#include <stdint.h>

/* Startet die Amiga-Intro-Sequenz im Terminal
 * interactive: 1 = wartet auf Tastendruck (oder max_frames), 0 = feste Framelänge
 * max_frames:  Maximale Frames (bei 50Hz entsprechen 150 Frames = 3 Sekunden). 0 = unbegrenzt bis Tastendruck.
 */
int amiga_intro_run(int interactive, int max_frames);

#endif /* AMIGA_INTRO_H */

