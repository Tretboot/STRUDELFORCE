#!/usr/bin/env python3
"""build_soundengine_core.py - Assembles src/soundengine_core.c from verified reference modules"""
import os, sys, re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

def read(rel):
    with open(os.path.join(ROOT, rel), "r", encoding="utf-8", errors="replace") as f:
        return f.read()

def strip_includes_and_headers(text):
    # Remove #include, #define _CRT_SECURE_NO_WARNINGS
    lines = []
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#include") or s.startswith("#define _CRT_SECURE_NO_WARNINGS"):
            continue
        lines.append(line)
    return "\n".join(lines)

def convert_cpp_comments(text):
    res = []
    i = 0
    n = len(text)
    in_block = False
    in_str = False
    while i < n:
        if in_block:
            if i + 1 < n and text[i:i+2] == "*/":
                in_block = False
                res.append("*/")
                i += 2
            else:
                res.append(text[i])
                i += 1
        elif in_str:
            if text[i] == '\\' and i + 1 < n:
                res.append(text[i:i+2])
                i += 2
            elif text[i] == '"':
                in_str = False
                res.append('"')
                i += 1
            else:
                res.append(text[i])
                i += 1
        else:
            if i + 1 < n and text[i:i+2] == "/*":
                in_block = True
                res.append("/*")
                i += 2
            elif i + 1 < n and text[i:i+2] == "//":
                eol = text.find("\n", i)
                if eol == -1:
                    comment = text[i+2:]
                    i = n
                else:
                    comment = text[i+2:eol]
                    i = eol
                comment_clean = comment.replace("*/", "* /")
                res.append("/*" + comment_clean + " */")
            elif text[i] == '"':
                in_str = True
                res.append('"')
                i += 1
            else:
                res.append(text[i])
                i += 1
    return "".join(res)

def main():
    sec1_src = read("src/soundengine_core.c")
    # Extract Section 1 (Strudel Cleanroom AST)
    s1_start = sec1_src.find("/* ====\n * SECTION 1: STRUDEL CLEANROOM")
    if s1_start < 0:
        s1_start = sec1_src.find("SECTION 1: STRUDEL CLEANROOM")
        s1_start = sec1_src.rfind("/*", 0, s1_start)
    s1_end = sec1_src.find("SECTION 2: CHRIS HUELSBECK")
    s1_end = sec1_src.rfind("/*", 0, s1_end)
    section1 = sec1_src[s1_start:s1_end].strip()

    # Section 2: TFMX Player from archive/src_pre_defrag/tfmx_player.c
    tfmx_raw = read("archive/src_pre_defrag/tfmx_player.c")
    tfmx_body = strip_includes_and_headers(tfmx_raw)

    # Section 3: MOD Player from archive/src_pre_defrag/mod_player.c
    mod_raw = read("archive/src_pre_defrag/mod_player.c")
    mod_raw = mod_raw.replace("#define PAULA_CLOCK 3546895UL", "#ifndef PAULA_CLOCK\n#define PAULA_CLOCK 3546895UL\n#endif")
    mod_body = strip_includes_and_headers(mod_raw)

    # Section 4: SfMixer from archive/src_pre_defrag/sf_mixer.c
    mix_raw = read("archive/src_pre_defrag/sf_mixer.c")
    mix_body = strip_includes_and_headers(mix_raw)

    # Section 5 & 6: Cyber Paula & Reactive Matrix & SFX
    # We take game_audio.c and adapt names
    ga_raw = read("archive/src_pre_defrag/game_audio.c")
    ga_body = strip_includes_and_headers(ga_raw)

    # Section 7: Soundgodizer from archive/src_pre_defrag/soundgodizer.c
    god_raw = read("archive/src_pre_defrag/soundgodizer.c")
    god_body = strip_includes_and_headers(god_raw)

    # Section 8: Win32 Audio Transport from archive/src_pre_defrag/win_audio.c
    win_raw = read("archive/src_pre_defrag/win_audio.c")
    win_body = strip_includes_and_headers(win_raw)

    out = []
    out.append("""/*
 * soundengine_core.c - STRUDELFORCE Universal Sound Engine Hub
 *
 * Vereint alle Audio-Subsysteme in einem monolithischen Modul:
 *   1. Clean-Room Strudel Mini-Notation Compiler & Query AST
 *   2. Chris Huelsbeck TFMX 1.5 / Pro / 7V Player (Bit-identisch zum TFMX-Oracle)
 *   3. ProTracker MOD Replay Engine (4, 6, 8 Kanaele, 31 Samples, Bit-identisch zum MOD-Oracle)
 *   4. Multi-Source Audio Mixer (SfMixer mit Gain-Rampen & Ducking)
 *   5. Cyber-Paula 8-Stimmen Synthesizer & Reaktive Gameplay-Matrix
 *   6. Game-SFX System (Feuer, Sprung, Treffer, Explosion, Pickup, Truhe)
 *   7. Soundgodizer PC HQ-Overdrive & Haas Stereo Widener (optional)
 *   8. Win32 Low-Latency Ring-Transport (Audio-Thread & WaveOut)
 *
 * Lizenz: MIT (Clean Room)
 */

#define _CRT_SECURE_NO_WARNINGS
#include "strudelforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif
""")

    out.append(convert_cpp_comments(section1))
    out.append("\n\n/* ============================================================================\n"
               " * SECTION 2: CHRIS HUELSBECK TFMX REPLAY ENGINE (v2)\n"
               " * ============================================================================ */\n")
    out.append(convert_cpp_comments(tfmx_body))

    out.append("\n\n/* ============================================================================\n"
               " * SECTION 3: PROTRACKER MOD REPLAY ENGINE\n"
               " * ============================================================================ */\n")
    # Add mod_player_init and mod_player_stop
    mod_inits = """
void mod_player_init(ModPlayer *m) {
    if (m) memset(m, 0, sizeof(*m));
}

void mod_player_stop(ModPlayer *m) {
    if (m) m->playing = 0;
}
"""
    out.append(mod_inits)
    out.append(convert_cpp_comments(mod_body))

    out.append("\n\n/* ============================================================================\n"
               " * SECTION 4: MULTI-SOURCE MIXER (SfMixer)\n"
               " * ============================================================================ */\n")
    out.append(convert_cpp_comments(mix_body))

    out.append("\n\n/* ============================================================================\n"
               " * SECTION 5: CYBER-PAULA REPLAY & GAMEPLAY REACTIVE MATRIX\n"
               " * ============================================================================ */\n")
    # Adapt game_audio function names so both soundengine_* and game_audio_* work
    ga_converted = convert_cpp_comments(ga_body)
    # Fix voice_set to initialize loop bounds and inc_fp immediately
    old_vs = """static void voice_set(GameAudio *a, int v, uint8_t macro, int16_t note, int16_t vol) {
    GameVoice *vc = &a->voices[v];
    vc->active = 1;
    vc->macro_id = macro;
    vc->base_note = note;
    vc->tick_50hz = 0;
    vc->pos_fp = 0;
    vc->volume = vol;
}"""
    new_vs = """static void voice_set(GameAudio *a, int v, uint8_t macro, int16_t note, int16_t vol) {
    GameVoice *vc = &a->voices[v];
    const MacDef *m = &MACROS[macro % 5];
    double freq;
    vc->active = 1;
    vc->macro_id = macro;
    vc->base_note = note;
    vc->tick_50hz = 0;
    vc->pos_fp = 0;
    vc->volume = vol;
    vc->loop_start = m->off;
    vc->loop_len = m->len ? m->len : 128;
    freq = 440.0 * pow(2.0, (double)(note - 69) / 12.0);
    vc->inc_fp = (uint32_t)((freq * (double)vc->loop_len) / (double)GAME_AUDIO_RATE * 65536.0);
}"""
    ga_converted = ga_converted.replace(old_vs, new_vs)
    ga_converted = ga_converted.replace("if (!vc->active || vc->volume <= 0) continue;",
                                        "if (!vc->active || vc->volume <= 0 || vc->loop_len == 0) continue;")
    # Replace game_audio_init with soundengine_init, game_audio_update with soundengine_update, etc.
    ga_converted = ga_converted.replace("void game_audio_init(GameAudio *a)", "void soundengine_init(GameAudio *a)")
    ga_converted = ga_converted.replace("void game_audio_update(GameAudio *a, const GameState *s)", "void soundengine_update(GameAudio *a, const GameState *s)")
    ga_converted = ga_converted.replace("void game_audio_render_synth(GameAudio *a, int16_t *out, int n)", "void soundengine_render_synth(GameAudio *a, int16_t *out, int n)")
    ga_converted = ga_converted.replace("void game_audio_render(GameAudio *a, int16_t *out, int n)", "void soundengine_render(GameAudio *a, int16_t *out, int n)")
    # Inside soundengine_render, replace game_audio_render_synth call with soundengine_render_synth
    ga_converted = ga_converted.replace("game_audio_render_synth(a, out, n);", "soundengine_render_synth(a, out, n);")
    out.append(ga_converted)

    out.append("\n\n/* ============================================================================\n"
               " * SECTION 6: GAME-SFX SYSTEM (Feuer, Jump, Hit, Explosion, Crystal, Pickup, Chest)\n"
               " * ============================================================================ */\n")
    sfx_code = """
void sound_sfx_fire(GameAudio *a, uint8_t weapon) {
    if (weapon == WEAPON_LASER) {
        voice_set(a, 4, 2, 60, 50);
    } else if (weapon == WEAPON_BOUNCE) {
        voice_set(a, 4, 1, 64, 45);
    } else {
        voice_set(a, 4, 0, 57, 48);
    }
}

void sound_sfx_jump(GameAudio *a) {
    voice_set(a, 5, 1, 45, 52);
}

void sound_sfx_hit(GameAudio *a, uint8_t is_tank) {
    voice_set(a, 6, 2, is_tank ? 28 : 34, is_tank ? 58 : 46);
}

void sound_sfx_explosion(GameAudio *a) {
    voice_set(a, 6, 3, 24, 60);
}

void sound_sfx_crystal(GameAudio *a) {
    voice_set(a, 7, 1, 72, 48);
}

void sound_sfx_pickup(GameAudio *a) {
    voice_set(a, 7, 0, 67, 56);
}

void sound_sfx_chest(GameAudio *a) {
    voice_set(a, 7, 2, 63, 62);
}
"""
    out.append(sfx_code)

    out.append("\n\n/* ============================================================================\n"
               " * SECTION 7: SOUNDGODIZER POST-PROCESSING (PC HQ Overdrive & Haas Widener)\n"
               " * ============================================================================ */\n")
    out.append(convert_cpp_comments(god_body))

    out.append("\n\n/* ============================================================================\n"
               " * SECTION 8: WIN32 AUDIO TRANSPORT (Low-Latency Ring & Dedicated Thread)\n"
               " * ============================================================================ */\n")
    out.append(convert_cpp_comments(win_body))

    full_text = "\n".join(out)

    target_path = os.path.join(ROOT, "src", "soundengine_core.c")
    with open(target_path, "w", encoding="utf-8") as f:
        f.write(full_text)
    print(f"Generated {target_path} ({len(full_text.splitlines())} lines)")

if __name__ == "__main__":
    main()
