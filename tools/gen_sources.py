#!/usr/bin/env python3
"""gen_sources.py - regenerates every file whose extension the Kiwi sandbox does NOT persist
(.h, Makefile, .strudel, .vscode/*) and applies idempotent patches to legacy files.

Kiwi Lesson #1: only .c .py .md .txt .json .zip (+.png) survive between tool calls.
So headers/Makefile live here as text and are materialised right before packing.

  python3 tools/gen_sources.py <workspace_root>
"""
import os, sys, re

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
def w(rel, txt):
    p = os.path.join(ROOT, rel); os.makedirs(os.path.dirname(p) or ".", exist_ok=True)
    open(p, "w", newline="\n").write(txt.lstrip("\n")); print("gen", rel)

w("src/tfmx_player.h", r"""
/*
 * tfmx_player.h - STRUDELFORCE TFMX 1.5 / Pro / 7V player v2 (Chris Huelsbeck format)
 * Mirror oracle: tools/tfmx_oracle.py. Integer only, C89. NOT Jochen Hippel TFMX/COSO.
 * Containers: .tfx+.sam, mdat.X+smpl.X, TFHD, TFMXPAK.   Lizenz: MIT (Clean Room)
 */
#ifndef TFMX_PLAYER_H
#define TFMX_PLAYER_H
#include <stdint.h>
/* KEIN game_audio.h-Include hier! Der Player ist kern-neutral (Smith- und
 * sf-Targets linken ihn beide). Legacy tick bekommt GameAudio per
 * Forward-Declaration; tfmx_player.c includiert game_audio.h selbst. */
struct GameAudio;

#define TFMX_VOICES        8
#define TFMX_TRACKS        8
#define TFMX_MAX_SUBSONGS 32

typedef struct {
    uint8_t  mrun, keyup;
    uint32_t mptr, rptr;
    uint16_t mstep, rstep, mwait;
    int16_t  mloop;
    int32_t  dma_wait;
    uint8_t  note, prevnote, velo;
    int8_t   finetune;
    uint16_t dest, cur;
    uint16_t porta_rate, porta_per;
    uint8_t  porta_reset, porta_time;
    uint8_t  vib_reset, vib_time;
    int8_t   vib_width;
    int16_t  vib_off;
    int16_t  vol;
    uint8_t  env_rate, env_reset, env_time, env_end;
    uint8_t  ab_time, ab_reset;
    int16_t  ab_delta;
    uint32_t reg_start;
    uint16_t reg_len;
    uint32_t lat_start, lat_bytes;
    uint32_t pos_fp, inc_fp;
    uint8_t  dma;
} TfmxVoice;

typedef struct {
    uint8_t  pnum;
    uint32_t paddr, raddr;
    uint16_t pstep, rstep, pwait;
    int16_t  ploop;
    int8_t   xpose;
} TfmxTrack;

typedef struct {
    uint8_t  loaded, playing;
    char     title[244];
    const uint8_t *mdat;  uint32_t mdat_size;
    const int8_t  *smpl;  uint32_t smpl_size;
    uint8_t *owned_buf;
    uint32_t track_off, patt_off, macro_off;
    uint16_t num_steps;
    uint8_t  num_subsongs, current_subsong;
    uint16_t first, last, pos, speed, speed_cnt, step_tempo, bpm;
    int16_t  tloop, master, fade_dest, fade_step;
    uint8_t  fade_time, fade_reset;
    uint32_t rate, spj_fp;
    int32_t  acc_fp;
    uint32_t jiffy, oob_reads;
    TfmxVoice v[TFMX_VOICES];
    TfmxTrack t[TFMX_TRACKS];
} TfmxPlayer;

void tfmx_player_init(TfmxPlayer *p);
int  tfmx_player_load(TfmxPlayer *p, const char *tfx_path, const char *sam_path);
int  tfmx_player_load_mem(TfmxPlayer *p, const uint8_t *mdat, uint32_t mdat_size,
                          const int8_t *smpl, uint32_t smpl_size);
void tfmx_player_free(TfmxPlayer *p);
void tfmx_player_start(TfmxPlayer *p, uint8_t subsong);
void tfmx_player_stop(TfmxPlayer *p);
void tfmx_player_set_rate(TfmxPlayer *p, uint32_t rate);
void tfmx_player_jiffy(TfmxPlayer *p);                 /* Amiga HW: call from VBlank/CIA IRQ */
void tfmx_player_render(TfmxPlayer *p, int16_t *out, int n);
void tfmx_player_tick(TfmxPlayer *p, struct GameAudio *audio); /* legacy: hooks render into GameAudio */
#endif
""")

w("src/mod_player.h", r"""
/* mod_player.h - ProTracker MOD player (C89, integer). Mirror: tools/mod_oracle.py */
#ifndef MOD_PLAYER_H
#define MOD_PLAYER_H
#include <stdint.h>
#define MOD_MAX_CH 8
typedef struct { const int8_t *data; uint32_t len, rep, replen; int8_t fine; uint8_t vol; } ModSample;
typedef struct {
    uint8_t  sidx, active, cmd, prm, porta_spd, vib_pos, vib_prm, loop_row, loop_cnt, offset_mem;
    int16_t  period, dest, vol, note_per, per_out;
    uint32_t pos_fp, inc_fp;
} ModChan;
typedef struct {
    uint8_t  loaded, playing;
    char     title[21];
    uint8_t  nch, songlen, restart, npat;
    const uint8_t *orders, *patdata, *buf;
    uint32_t size;
    ModSample smp[32];
    uint8_t  pos, row, speed, bpm, pdelay, jump;
    uint16_t tick;
    int16_t  next_pos, next_row;
    uint32_t rate, ticks, loops;
    int32_t  acc;
    ModChan  ch[MOD_MAX_CH];
} ModPlayer;
int  mod_player_load_mem(ModPlayer *m, const uint8_t *buf, uint32_t size);  /* buf must outlive player */
void mod_player_start(ModPlayer *m, uint32_t rate);
void mod_player_tick(ModPlayer *m);                    /* one 50Hz-ish tick (CIA bpm) */
void mod_player_render(ModPlayer *m, int16_t *out, int n);
#endif
""")

w("src/sf_mixer.h", r"""
/* sf_mixer.h - mix TFMX + MOD + Strudel synth (+SID later) with click-free gain ramps */
#ifndef SF_MIXER_H
#define SF_MIXER_H
#include <stdint.h>
#define SF_MIX_MAX_SOURCES 6
#define SF_MIX_BLOCK 256
typedef void (*SfRenderFn)(void *ctx, int16_t *out, int n);
typedef struct { SfRenderFn fn; void *ctx; int16_t gain, target; uint8_t muted, duck; char name[12]; } SfSource;
typedef struct {
    SfSource src[SF_MIX_MAX_SOURCES];
    int      count;
    int16_t  master;      /* 256 = unity */
    int16_t  duck_level;  /* 0..256, applied to sources with duck=1 (e.g. music under SFX) */
    uint32_t clips;
    int16_t  scratch[SF_MIX_BLOCK];
} SfMixer;
void sf_mixer_init(SfMixer *mx);
int  sf_mixer_add(SfMixer *mx, SfRenderFn fn, void *ctx, int16_t gain, const char *name);
void sf_mixer_set_gain(SfMixer *mx, int idx, int16_t gain);
void sf_mixer_render(SfMixer *mx, int16_t *out, int n);
void sf_mixer_ext_render(void *ctx, int16_t *out, int n);  /* plug into GameAudio.ext_render */
#endif
""")

w("src/sid_header.h", r"""
/* sid_header.h - PSID/RSID header parser (playback: see docs/ROADMAP_AUDIO.md) */
#ifndef SID_HEADER_H
#define SID_HEADER_H
#include <stdint.h>
typedef struct {
    uint8_t rsid; uint16_t version, data_offset, load_addr, init_addr, play_addr, songs, start_song, flags;
    uint32_t speed; char name[33], author[33], released[33];
} SidHeader;
int sid_header_parse(SidHeader *h, const uint8_t *buf, uint32_t size);
#endif
""")

# ---------------------------------------------------------------- game_audio patch (idempotent)
ga_h = os.path.join(ROOT, "src/game_audio.h"); ga_c = os.path.join(ROOT, "src/game_audio.c")
if os.path.exists(ga_h):
    h = open(ga_h, newline="").read()
    if "ext_render" not in h:
        h, n = re.subn(r"(uint32_t\s+ext_sample_size;)(\s*\r?\n\s*\}\s*GameAudio;)",
                       r"\1\n    void (*ext_render)(void *ctx, int16_t *out, int n); /* v2: TFMX/MOD/mixer hook */\n    void *ext_ctx;\2", h, 1)
        assert n == 1, "game_audio.h patch anchor not found"
        h = h.replace("void game_audio_render(GameAudio *a, int16_t *out, int n); /* 16574 Hz */",
                      "void game_audio_render(GameAudio *a, int16_t *out, int n); /* 16574 Hz, honours ext_render */\n"
                      "void game_audio_render_synth(GameAudio *a, int16_t *out, int n); /* built-in synth only */")
        open(ga_h, "w", newline="").write(h); print("patch src/game_audio.h")
if os.path.exists(ga_c):
    c = open(ga_c, newline="").read()
    if "game_audio_render_synth" not in c:
        c, n = re.subn(r"(a->ext_sample_size\s*=\s*0;)", r"\1\n    a->ext_render = NULL;\n    a->ext_ctx = NULL;", c, 1)
        assert n == 1, "init anchor"
        c, n = re.subn(r"void game_audio_render\(GameAudio \*a, int16_t \*out, int n\)\s*\{",
                       "void game_audio_render_synth(GameAudio *a, int16_t *out, int n) {", c, 1)
        assert n == 1, "render anchor"
        c += ("\n/* v2 wrapper: external source (TFMX player / SfMixer) replaces the synth when attached.\n"
              "   To hear both, add game_audio_render_synth as an SfMixer source. */\n"
              "void game_audio_render(GameAudio *a, int16_t *out, int n) {\n"
              "    if (a->ext_render) { a->ext_render(a->ext_ctx, out, n); return; }\n"
              "    game_audio_render_synth(a, out, n);\n}\n")
        open(ga_c, "w", newline="").write(c); print("patch src/game_audio.c")

# ---------------------------------------------------------------- Makefile
w("Makefile", r"""
# ============================================================================
# STRUDELFORCE Makefile v2 (PC, tests, oracles, Amiga m68k ADF)
# Generated by tools/gen_sources.py - edit there (Kiwi sandbox drops Makefiles!)
# ============================================================================
CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2 -Isrc
C89FLAGS = -std=c89 -pedantic -Wall -Wextra -Wno-long-long -Isrc -fsyntax-only
LDFLAGS ?= -lm
PY      ?= python3

M68K_CC     ?= m68k-amigaos-gcc
M68K_CFLAGS ?= -m68000 -O2 -fomit-frame-pointer -Wall -Isrc -DM68K -DAMIGA
M68K_LDFLAGS?= -s
ADF_TOOL    ?= xdftool
AMIGA_EMU   ?= vamos
AMIGA_DIST  ?= amiga_dist
AMIGA_MUSIC ?= $(AMIGA_DIST)/music
AMIGA_MUSIC_SRC ?= TFMX Professional/Turrican 2
# ADF-Musik (Turrican 2, kurze ADFS-Namen): Load + World 1-4
M68K_ASSETS ?= $(AMIGA_MUSIC)/T2LOAD.TFX $(AMIGA_MUSIC)/T2LOAD.SAM \
               $(AMIGA_MUSIC)/T2W1.TFX $(AMIGA_MUSIC)/T2W1.SAM \
               $(AMIGA_MUSIC)/T2W2.TFX $(AMIGA_MUSIC)/T2W2.SAM \
               $(AMIGA_MUSIC)/T2W3.TFX $(AMIGA_MUSIC)/T2W3.SAM \
               $(AMIGA_MUSIC)/T2W4.TFX $(AMIGA_MUSIC)/T2W4.SAM

AUDIO_SRCS = src/tfmx_player.c src/mod_player.c src/sf_mixer.c src/sid_header.c src/game_audio.c src/core_sim.c

.PHONY: all game gui pc_game test test_demo test_tfmx test_phase1 test_audio_mix c89_check \
        headless_test win_audio_selftest intro strudel_export verify verify_tfmx verify_mod verify_levels \
        c_vs_oracle levels_png level_header \
        amiga_lint amiga_bin amiga_adf stage_amiga_music vamos_test clean help

RAYLIB_DIR ?= raylib

all: game gui test_audio_mix

help:
	@echo "make verify        - ALL python oracles (works in Kiwi without gcc)"
	@echo "make c89_check     - strict C89 syntax check of audio core (local gcc)"
	@echo "make test_audio_mix- build C mixer harness (TFMX+MOD -> WAV)"
	@echo "make c_vs_oracle   - bit-compare C TFMX/MOD output against python oracles"
	@echo "make test_phase1   - 30-Check Integrationstest (sf-Kern: core_sim_sf.c)"
	@echo "make headless_test - Audio-Parser-Selftest (test_core_audio.c)"
	@echo "make win_audio_selftest - Win-Audio-Transport v2 Selftest (Ring/Thread)"
	@echo "make intro         - Amiga Copper-Intro (./strudelforce_game --intro)"
	@echo "make strudel_export- TFMX Module -> Strudel Mini-Notation"
	@echo "make pc_game       - Raylib 2.5D Voxel-Frontend (braucht RAYLIB_DIR)"
	@echo "make amiga_lint / amiga_bin / amiga_adf / vamos_test"

GUI_SRCS  = src/gui_main.c src/core_sim.c src/game_audio.c src/tfmx_player.c src/livecode_ui.c src/win_audio.c
GAME_SRCS = src/game_main.c src/core_sim.c src/game_audio.c src/tfmx_player.c src/amiga_intro.c src/livecode_ui.c src/win_audio.c

gui: $(GUI_SRCS)
	$(CC) $(CFLAGS) $(GUI_SRCS) -o strudelforce_gui -lgdi32 -lwinmm -luser32 $(LDFLAGS)
game: $(GAME_SRCS)
	$(CC) $(CFLAGS) $(GAME_SRCS) -o strudelforce_game -lwinmm $(LDFLAGS)

test_audio_mix: src/test_audio_mix.c $(AUDIO_SRCS)
	$(CC) $(CFLAGS) src/test_audio_mix.c $(AUDIO_SRCS) -o test_audio_mix $(LDFLAGS)

c89_check:
	$(CC) $(C89FLAGS) src/tfmx_player.c src/mod_player.c src/sf_mixer.c src/sid_header.c

test: verify test_audio_mix c_vs_oracle test_phase1
test_demo: game
	./strudelforce_game --demo 2500 --audio demo_audio.wav
test_tfmx: test_audio_mix
	./test_audio_mix --tfmx "TFMX Professional/Apidya/Apidya - Ongame 1 - Meadow.tfx" --secs 30 --wav apidya_c.wav
# sf-Kern (Schwarm-Zweig): core_sim_sf.c + livecode_ui_sf.c implementieren sf_interfaces.h.
# WICHTIG: core_sim_sf.c und core_sim.c definieren beide core_init/core_tick ->
# niemals in einem einzigen Binary linken!
test_phase1:
	$(CC) $(CFLAGS) src/test_phase1.c src/core_sim_sf.c src/cyber_paula.c src/voxel_render.c src/livecode_ui_sf.c src/biomes.c src/amiga_hw.c -o strudelforce_test $(LDFLAGS)
	./strudelforce_test

headless_test:
	$(CC) $(CFLAGS) src/test_core_audio.c -o strudelforce_headless $(LDFLAGS)
	./strudelforce_headless

win_audio_selftest:
	$(CC) $(CFLAGS) src/test_win_audio.c src/win_audio.c -o win_audio_selftest -lwinmm $(LDFLAGS)
	./win_audio_selftest

intro: game
	./strudelforce_game --intro

strudel_export:
	$(PY) tools/tfmx_importer.py --batch "TFMX Professional" --out "exported_strudel"

# Raylib 2.5D Voxel-Frontend (optional, z.B. RAYLIB_DIR=C:/raylib make pc_game)
pc_game: src/main_pc.c src/core_sim_sf.c src/cyber_paula.c src/voxel_render.c src/livecode_ui_sf.c src/biomes.c src/amiga_hw.c
	@test -d "$(RAYLIB_DIR)/include" || (echo "Raylib fehlt: RAYLIB_DIR auf den Raylib-Ordner setzen"; exit 1)
	$(CC) $(CFLAGS) -I$(RAYLIB_DIR)/include $^ -o strudelforce_pc -L$(RAYLIB_DIR)/lib -lraylib $(LDFLAGS)

verify: verify_tfmx verify_mod verify_levels
verify_tfmx:
	$(PY) tools/tfmx_oracle.py --scan "TFMX Professional" 4
verify_mod:
	$(PY) tools/selftest.py mod
verify_levels:
	$(PY) tools/level_tool.py validate
c_vs_oracle: test_audio_mix
	$(PY) tools/c_vs_oracle.py
level_header:
	$(PY) tools/level_tool.py header src/level_data.h
levels_png:
	$(PY) tools/level_tool.py png docs/img

# Amiga-Target (m68k): Die M68K-Sektion von amiga_hw.c enthaelt den einzigen
# Entry-Point main() (50Hz VBlank-Loop, Voxel-Render, TFMX-Paula-Musik aus
# music/). Kein zweites main hier links laden!
AMIGA_SRCS = src/amiga_hw.c src/core_sim_sf.c src/voxel_render.c src/biomes.c src/tfmx_player.c
amiga_lint:
	$(M68K_CC) $(M68K_CFLAGS) -std=gnu89 -fsyntax-only src/tfmx_player.c src/core_sim_sf.c src/amiga_hw.c
stage_amiga_music:
	@mkdir -p $(AMIGA_MUSIC) $(AMIGA_DIST)/s
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - Load.tfx" $(AMIGA_MUSIC)/T2LOAD.TFX
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - Load.sam" $(AMIGA_MUSIC)/T2LOAD.SAM
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - World 1 - Dessert.tfx" $(AMIGA_MUSIC)/T2W1.TFX
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - World 1 - Dessert.sam" $(AMIGA_MUSIC)/T2W1.SAM
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - World 2 - Cave.tfx" $(AMIGA_MUSIC)/T2W2.TFX
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - World 2 - Cave.sam" $(AMIGA_MUSIC)/T2W2.SAM
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - World 3 - Flight.tfx" $(AMIGA_MUSIC)/T2W3.TFX
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - World 3 - Flight.sam" $(AMIGA_MUSIC)/T2W3.SAM
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - World 4 - Factory.tfx" $(AMIGA_MUSIC)/T2W4.TFX
	@cp "$(AMIGA_MUSIC_SRC)/Turrican 2 - World 4 - Factory.sam" $(AMIGA_MUSIC)/T2W4.SAM
	@printf '; STRUDELFORCE - startup-sequence (A500)\n; Data-Disk: run  df0:strudelforce.amiga\n; Boot-Disk:  startet das Spiel direkt.\necho STRUDELFORCE 2.5D Voxel Shooter (A500)\nrun "strudelforce.amiga"\n' > $(AMIGA_DIST)/s/startup-sequence
	@echo "Amiga-Musik staged: Load + World 1-4 (Turrican 2) + s/startup-sequence"
amiga_bin: $(AMIGA_SRCS)
	@command -v $(M68K_CC) >/dev/null 2>&1 || (echo "m68k-amigaos-gcc fehlt (bebbo/amiga-gcc)"; exit 1)
	$(M68K_CC) $(M68K_CFLAGS) $(AMIGA_SRCS) -o strudelforce.amiga $(M68K_LDFLAGS)
amiga_adf: amiga_bin stage_amiga_music
	@command -v $(ADF_TOOL) >/dev/null 2>&1 || (echo "xdftool fehlt: pip install amitools"; exit 1)
	@cp strudelforce.amiga $(AMIGA_DIST)/strudelforce.amiga
	$(ADF_TOOL) -f strudelforce.adf pack $(AMIGA_DIST) ffs
	$(ADF_TOOL) strudelforce.adf list
vamos_test: amiga_bin
	$(AMIGA_EMU) ./strudelforce.amiga

# ---------------------------------------------------------------------------
# Amiga-Build via VS Code Extension BartmanAbyss.amiga-debug (Haupt-Dev-Flow)
# Self-contained m68k-amiga-elf-gcc 15.2 (LTO, -nostdlib) + elf2hunk.
# Das "compile"-Task der Extension ruft:  make -j4 program=<out>
# Voraussetzung: stage_amiga_music (INCBIN-Tracks) + Kickstart-1.3-ROM-Pfad
# in .vscode/settings.json (amiga.rom-paths.A500).
# ---------------------------------------------------------------------------
M68KELF_CC ?= m68k-amiga-elf-gcc
M68KELF_CFLAGS = -g -m68000 -Ofast -nostdlib -Wextra -Wno-unused-function \
                 -Wno-volatile-register-var -fomit-frame-pointer -fno-tree-loop-distribution \
                 -flto -fwhole-program -fno-exceptions -ffunction-sections -fdata-sections \
                 -Isrc -DM68K -DAMIGA
M68KELF_LDFLAGS = -Wl,--emit-relocs,--gc-sections,-Ttext=0
program ?= out/strudelforce.amiga

.PHONY: program
program: stage_amiga_music
	@command -v $(M68KELF_CC) >/dev/null 2>&1 || (echo "m68k-amiga-elf-gcc fehlt (Bartman-Extension installiert + PATH?)"; exit 1)
	@mkdir -p obj_amiga out
	$(M68KELF_CC) $(M68KELF_CFLAGS) -c src/amiga_hw.c -o obj_amiga/amiga_hw.o
	$(M68KELF_CC) $(M68KELF_CFLAGS) -c src/core_sim_sf.c -o obj_amiga/core_sim_sf.o
	$(M68KELF_CC) $(M68KELF_CFLAGS) -c src/voxel_render.c -o obj_amiga/voxel_render.o
	$(M68KELF_CC) $(M68KELF_CFLAGS) -c src/biomes.c -o obj_amiga/biomes.o
	$(M68KELF_CC) $(M68KELF_CFLAGS) -c src/tfmx_player.c -o obj_amiga/tfmx_player.o
	$(M68KELF_CC) $(M68KELF_CFLAGS) -c src/amiga_music.c -o obj_amiga/amiga_music.o
	$(M68KELF_CC) $(M68KELF_CFLAGS) $(M68KELF_LDFLAGS) obj_amiga/amiga_hw.o obj_amiga/core_sim_sf.o obj_amiga/voxel_render.o obj_amiga/biomes.o obj_amiga/tfmx_player.o obj_amiga/amiga_music.o -o $(program:.exe=.elf)
	elf2hunk $(program:.exe=.elf) $(program)

clean:
	rm -f strudelforce_game strudelforce_gui strudelforce_test test_audio_mix win_audio_selftest *.exe *.obj src/*.obj
	rm -f strudelforce.amiga strudelforce.adf *.wav *.raw s_startup.tmp
	rm -rf amiga_dist obj_amiga out
""")

# ---------------------------------------------------------------- songs (one text, every game)
w("user_music/dream_world.strudel", 'note("<c3 e3 g3 c4> [g3 e3] <a3 f3> [g3 ~ g3 b3]")\n')
w("user_music/great_bath_tribute.strudel", 'note("<c3 eb3 g3 f3> [g3 c4 eb4 g4]*2 <ab2 bb2 c3 d3>")\n')
w("user_music/hive_drive.strudel", 'note("c2 [c2 eb2] [f2 g2] <bb2 [g2 f2]>")\n')

w(".vscode/tasks.json", r"""
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "compile",
      "type": "shell",
      "windows": {
        "command": "${command:amiga.bin-path}\\gnumake.exe",
        "options": {
          "cwd": "${workspaceRoot}",
          "env": {
            "PATH": "${env:PATH};${command:amiga.bin-path}\\opt\\bin;${command:amiga.bin-path}"
          }
        }
      },
      "command": "make",
      "options": {
        "cwd": "${workspaceRoot}",
        "env": {
          "PATH": "${env:PATH}:${command:amiga.bin-path}/opt/bin:${command:amiga.bin-path}"
        }
      },
      "args": [
        "-j4",
        "program=${config:amiga.program}"
      ],
      "problemMatcher": [
        {
          "base": "$gcc",
          "fileLocation": "absolute"
        }
      ],
      "presentation": {
        "echo": true,
        "reveal": "silent",
        "focus": false,
        "panel": "shared",
        "showReuseMessage": false,
        "clear": true
      },
      "group": {
        "kind": "build",
        "isDefault": true
      }
    },
    {"label": "SF: verify (oracles)", "type": "shell", "command": "make verify", "group": "test"},
    {"label": "SF: c89_check", "type": "shell", "command": "make c89_check", "problemMatcher": ["$gcc"]},
    {"label": "SF: c_vs_oracle", "type": "shell", "command": "make c_vs_oracle", "problemMatcher": ["$gcc"]},
    {"label": "SF: amiga_adf", "type": "shell", "command": "make amiga_adf", "problemMatcher": ["$gcc"]},
    {"label": "SF: kiwi pack", "type": "shell", "command": "python3 tools/kiwi_handoff.py pack ."}
  ]
}
""")
w(".vscode/launch.json", r"""
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "amiga",
      "request": "launch",
      "preLaunchTask": "compile",
      "name": "Amiga 500 (STRUDELFORCE)",
      "config": "A500",
      "program": "${workspaceFolder}/${config:amiga.program}",
      "kickstart": "${config:amiga.rom-paths.A500}",
      "internalConsoleOptions": "openOnSessionStart"
    }
  ]
}
""")
w(".vscode/settings.json", r"""
{
  "amiga.program": "out/strudelforce.amiga",
  "amiga.rom-paths.A500": "PFAD_ZUM_KICKSTART_1_3_ROM_EINTRAGEN"
}
""")
w(".vscode/amiga.json", r"""
{
  "includePath": ["${workspaceFolder}", "${workspaceFolder}/src"],
  "defines": ["M68K", "AMIGA"]
}
""")
w(".vscode/extensions.json", r"""
{
  "recommendations": [
    "BartmanAbyss.amiga-debug",
    "ms-vscode.cpptools"
  ]
}
""")
print("GEN_DONE")
