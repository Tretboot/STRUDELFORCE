# STRUDELFORCE Amiga-Port - Build mit m68k-amiga-elf-gcc (Bartman "Amiga C/C++ Compile, Debug & Profile")
# Aufruf: aus amiga/ heraus `make` (Extension-Task "compile" oder per Hand mit der opt\bin im PATH).
# Ergebnis: out/a.exe (Amiga-Hunk) -> ../strudelforce.amiga (fuer ADF-Pipeline).
# Kern: ../src/game_logic.c (Smith-Kern, identisch zum PC) + ../src/level_engine.c.
# Libc-frei (-nostdlib -ffreestanding): siehe AGENTS.md, Dual-Toolchain-Regel.

ifdef OS
	WINDOWS = 1
	SHELL = cmd.exe
endif

VPATH = support ../src

c_sources := main.c sys.c rt.c gcc8_c_support.c game_logic.c level_engine.c
c_objects := $(addprefix obj/,$(patsubst %.c,%.o,$(c_sources)))
s_sources := support/gcc8_a_support.s
s_objects := $(addprefix obj/,$(patsubst %.s,%.o,$(notdir $(s_sources))))
objects := $(c_objects) $(s_objects)

override program = out/a
OUT = $(program)
CC = m68k-amiga-elf-gcc

ifdef WINDOWS
	SDKDIR = $(abspath $(dir $(shell where $(CC)))..\m68k-amiga-elf\sys-include)
else
	SDKDIR = $(abspath $(dir $(shell which $(CC)))../m68k-amiga-elf/sys-include)
endif

CCFLAGS = -g -MP -MMD -m68000 -O2 -std=gnu99 -nostdlib -ffreestanding -Wextra -Wno-unused-function -Wno-volatile-register-var -fomit-frame-pointer -fno-tree-loop-distribution -fno-exceptions -I. -I../src -DM68K -DAMIGA
ASFLAGS = -Wa,-g,--register-prefix-optional,-I$(SDKDIR),-D
LDFLAGS = -Wl,--emit-relocs,-Ttext=0,-Map=$(OUT).map

all: $(OUT).exe

$(OUT).exe: $(OUT).elf
	$(info Elf2Hunk $(program).exe)
	@elf2hunk $(OUT).elf $(OUT).exe -s

$(OUT).elf: $(objects) | out
	$(info Linking $(program).elf)
	@$(CC) $(CCFLAGS) $(LDFLAGS) $(objects) -o $@

clean:
	$(info Cleaning...)
ifdef WINDOWS
	@del /q obj\* out\*
else
	@$(RM) obj/* out/*
endif

-include $(objects:.o=.d)

$(c_objects) : obj/%.o : %.c | obj
	$(info Compiling $<)
	@$(CC) $(CCFLAGS) -c -o $@ $<

$(s_objects): obj/%.o : %.s | obj
	$(info Assembling $<)
	@$(CC) $(CCFLAGS) $(ASFLAGS) -c -o $@ $<

obj:
ifdef WINDOWS
	@if not exist obj mkdir obj
else
	@mkdir -p obj
endif

out:
ifdef WINDOWS
	@if not exist out mkdir out
else
	@mkdir -p out
endif
