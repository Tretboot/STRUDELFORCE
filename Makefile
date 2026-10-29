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
# Extension-Aufruf (make -j4 program=<out>) baut den Amiga-Port statt der PC-Targets:
ifdef program
.DEFAULT_GOAL := program
endif

M68K_CC     ?= m68k-amigaos-gcc
M68K_CFLAGS ?= -m68000 -O2 -fomit-frame-pointer -Wall -Isrc -DM68K -DAMIGA
M68K_LDFLAGS?= -s

AMIGA_DIST  ?= amiga_dist
AMIGA_MUSIC ?= $(AMIGA_DIST)/music
ADF_TOOL    ?= $(PY) -m amitools.tools.xdftool
AMIGA_SRCS  = src/amiga_hw.c src/core_sim_sf.c src/voxel_render.c src/biomes.c src/tfmx_player.c src/amiga_music.c

GUI_SRCS  = src/gui_main.c src/game_logic.c src/level_engine.c src/soundengine_core.c src/engine_core.c
GAME_SRCS = src/game_main.c src/game_logic.c src/level_engine.c src/soundengine_core.c src/engine_core.c
TEST_SRCS = src/test_suite.c src/game_logic.c src/level_engine.c src/soundengine_core.c src/engine_core.c

.PHONY: all game gui test c89_check clean help verify stage_amiga_music amiga_bin amiga_adf

all: game gui test

help:
	@echo "make all          - Baut Spiel, GUI und Testsuite"
	@echo "make game         - 50Hz Terminal Frontend (strudelforce_game)"
	@echo "make gui          - Win32 2.5D Voxel GUI (strudelforce)"
	@echo "make test         - Fuehrt die konsolidierte Testsuite aus"
	@echo "make c89_check    - Prueft strict C89 Syntax der Kernmodule"
	@echo "make verify       - Fuehrt alle Python-Oracles und Selftests aus"
	@echo "make stage_amiga_music - Staged Amiga TFMX Musik nach amiga_dist/music"
	@echo "make amiga_bin    - Baut 68k Amiga Binary (strudelforce.amiga)"
	@echo "make amiga_adf    - Baut bootfaehiges 880 KB Amiga Floppy Image (strudelforce.adf)"

ifeq ($(OS),Windows_NT)
WINMM ?= -lwinmm
else
WINMM ?=
endif

gui: $(GUI_SRCS)
	$(CC) $(CFLAGS) $(GUI_SRCS) -o strudelforce -lgdi32 -lwinmm -luser32 $(LDFLAGS)

game: $(GAME_SRCS)
	$(CC) $(CFLAGS) $(GAME_SRCS) -o strudelforce_game $(WINMM) $(LDFLAGS)

test: $(TEST_SRCS)
	$(CC) $(CFLAGS) $(TEST_SRCS) -o test_suite $(LDFLAGS)
	./test_suite

c89_check:
	$(CC) $(C89FLAGS) src/game_logic.c src/level_engine.c src/soundengine_core.c src/engine_core.c

verify:
	$(PY) tools/selftest.py
	$(PY) verify_core_sim.py

stage_amiga_music:
	$(PY) tools/stage_amiga_music.py

# Amiga-Port = eigenstaendiges Projekt amiga/ (m68k-amiga-elf-gcc der Bartman/Architect-Extension,
# -nostdlib, Smith-Kern src/game_logic.c). Die alte AMIGA_SRCS/Schwarm-Kern-Variante baute nach dem
# Merge nicht mehr (Typ-Konflikt GameState) und ist durch dieses Target ersetzt.
# `program` wird vom compile-Task der VS-Code-Extension gerufen (make -j4 program=...).
.PHONY: program
program:
	$(MAKE) -C amiga

amiga_bin: program
	$(PY) -c "import shutil; shutil.copyfile('amiga/out/a.exe','strudelforce.amiga')"

amiga_adf:
	$(PY) tools/build_amiga_adf.py

clean:
	rm -f *.o *.obj strudelforce strudelforce.exe strudelforce_game strudelforce_game.exe test_suite test_suite.exe strudelforce.amiga strudelforce.adf
