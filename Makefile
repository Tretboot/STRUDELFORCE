# ============================================================================
# STRUDELFORCE Consolidated Makefile (Post-Defragmentation)
#
# Monolithische Modul-Struktur (unter 15 C-Dateien):
#   - src/engine_core.c      : 2.5D Voxel Render, Amiga OCS/AGA HW, Retro Intro
#   - src/soundengine_core.c : TFMX, MOD, Cyber-Paula, Strudel, Game SFX, Mixer
#   - src/game_logic.c       : 16.16 Fixed-Point Physik, 3 Leben, Waffen, Kampagne
#   - src/gui_main.c         : Win32 GDI 2.5D GUI (strudelforce_v0.1)
#   - src/game_main.c        : 50Hz ANSI/ASCII Terminal Game (strudelforce_game)
#   - src/main_pc.c          : Raylib 2.5D Frontend
#   - src/test_suite.c       : Konsolidierter Headless Integrationstest
# ============================================================================
CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2 -Isrc
C89FLAGS = -std=c89 -pedantic -Wall -Wextra -Wno-long-long -Isrc -fsyntax-only
LDFLAGS ?= -lm
PY      ?= python3

M68K_CC     ?= m68k-amigaos-gcc
M68K_CFLAGS ?= -m68000 -O2 -fomit-frame-pointer -Wall -Isrc -DM68K -DAMIGA
M68K_LDFLAGS?= -s

GUI_SRCS  = src/gui_main.c src/game_logic.c src/soundengine_core.c src/engine_core.c
GAME_SRCS = src/game_main.c src/game_logic.c src/soundengine_core.c src/engine_core.c
TEST_SRCS = src/test_suite.c src/game_logic.c src/soundengine_core.c src/engine_core.c

.PHONY: all game gui test c89_check clean help verify

all: game gui test

help:
	@echo "make all          - Baut Spiel, GUI und Testsuite"
	@echo "make game         - 50Hz Terminal Frontend (strudelforce_game)"
	@echo "make gui          - Win32 2.5D Voxel GUI (strudelforce)"
	@echo "make test         - Fuehrt die konsolidierte Testsuite aus"
	@echo "make c89_check    - Prueft strict C89 Syntax der Kernmodule"
	@echo "make verify       - Fuehrt alle Python-Oracles und Selftests aus"

gui: $(GUI_SRCS)
	$(CC) $(CFLAGS) $(GUI_SRCS) -o strudelforce -lgdi32 -lwinmm -luser32 $(LDFLAGS)

game: $(GAME_SRCS)
	$(CC) $(CFLAGS) $(GAME_SRCS) -o strudelforce_game -lwinmm $(LDFLAGS)

test: $(TEST_SRCS)
	$(CC) $(CFLAGS) $(TEST_SRCS) -o test_suite $(LDFLAGS)
	./test_suite

c89_check:
	$(CC) $(C89FLAGS) src/game_logic.c src/soundengine_core.c src/engine_core.c

verify:
	$(PY) tools/selftest.py
	$(PY) verify_core_sim.py

clean:
	rm -f *.o *.obj strudelforce_game strudelforce_v0.1 test_suite strudelforce_game.exe strudelforce_v0.1.exe test_suite.exe
