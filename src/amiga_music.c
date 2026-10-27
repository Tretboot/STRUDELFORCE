/*
 * amiga_music.c - INCBIN-Embedding der 5 Turrican-2-Tracks (M68K-Build)
 * ------------------------------------------------------------------
 * .incbin-Pfade relativ zum CWD des Compilers (= Projekt-Root).
 * Makro-Muster entspricht dem der Bartman-VS-Code-Extension
 * (support/gcc8_c_support.h: INCBIN_SECTION), damit beide 68k-Toolchains
 * (m68k-amigaos-gcc, m68k-amiga-elf-gcc) identisches Verhalten zeigen.
 *
 * Ohne M68K-Definition ist diese Translation Unit leer (PC-Builds laden
 * die Tracks ueber stdio von der Platte).
 */
#ifdef M68K
#include "amiga_music.h"

/* name: Array-Referenz + Laenge; file: Pfad zum TFX/SAM (gestaggt) */
#define SF_INCBIN(name, file)                                                        \
    __asm__(".pushsection .rodata, \"a\"\n"                                          \
            ".balign 2\n"                                                            \
            ".global incbin_" #name "_start\n"                                       \
            ".type incbin_" #name "_start, @object\n"                                \
            "incbin_" #name "_start:\n"                                              \
            ".incbin \"" file "\"\n"                                                 \
            ".balign 1\n"                                                            \
            ".global incbin_" #name "_end\n"                                         \
            ".type incbin_" #name "_end, @object\n"                                  \
            ".size incbin_" #name "_start, incbin_" #name "_end - incbin_" #name "_start\n" \
            "incbin_" #name "_end:\n"                                                \
            ".byte 0\n"                                                              \
            ".popsection\n");                                                        \
    extern const unsigned char incbin_##name##_start[];                              \
    extern const unsigned char *incbin_##name##_end;
#define SF_INCBIN_LEN(name) \
    ((uint32_t)((const unsigned char *)incbin_##name##_end - 1u - (const unsigned char *)incbin_##name##_start))

/* Turrican 2: Load + World 1-4 (Chris Huelsbeck, TFMX Professional) */
SF_INCBIN(t2load_tfx, "amiga_dist/music/T2LOAD.TFX")
SF_INCBIN(t2load_sam, "amiga_dist/music/T2LOAD.SAM")
SF_INCBIN(t2w1_tfx,   "amiga_dist/music/T2W1.TFX")
SF_INCBIN(t2w1_sam,   "amiga_dist/music/T2W1.SAM")
SF_INCBIN(t2w2_tfx,   "amiga_dist/music/T2W2.TFX")
SF_INCBIN(t2w2_sam,   "amiga_dist/music/T2W2.SAM")
SF_INCBIN(t2w3_tfx,   "amiga_dist/music/T2W3.TFX")
SF_INCBIN(t2w3_sam,   "amiga_dist/music/T2W3.SAM")
SF_INCBIN(t2w4_tfx,   "amiga_dist/music/T2W4.TFX")
SF_INCBIN(t2w4_sam,   "amiga_dist/music/T2W4.SAM")

const uint8_t * const t2_tfx[T2_TRACK_COUNT] = {
    incbin_t2load_tfx_start,
    incbin_t2w1_tfx_start,
    incbin_t2w2_tfx_start,
    incbin_t2w3_tfx_start,
    incbin_t2w4_tfx_start
};
const uint32_t t2_tfx_len[T2_TRACK_COUNT] = {
    SF_INCBIN_LEN(t2load_tfx),
    SF_INCBIN_LEN(t2w1_tfx),
    SF_INCBIN_LEN(t2w2_tfx),
    SF_INCBIN_LEN(t2w3_tfx),
    SF_INCBIN_LEN(t2w4_tfx)
};
const uint8_t * const t2_sam[T2_TRACK_COUNT] = {
    incbin_t2load_sam_start,
    incbin_t2w1_sam_start,
    incbin_t2w2_sam_start,
    incbin_t2w3_sam_start,
    incbin_t2w4_sam_start
};
const uint32_t t2_sam_len[T2_TRACK_COUNT] = {
    SF_INCBIN_LEN(t2load_sam),
    SF_INCBIN_LEN(t2w1_sam),
    SF_INCBIN_LEN(t2w2_sam),
    SF_INCBIN_LEN(t2w3_sam),
    SF_INCBIN_LEN(t2w4_sam)
};
const char * const t2_names[T2_TRACK_COUNT] = {
    "Turrican 2 - Load Screen",
    "Turrican 2 - World 1 (Dessert)",
    "Turrican 2 - World 2 (Cave)",
    "Turrican 2 - World 3 (Flight)",
    "Turrican 2 - World 4 (Factory)"
};

#endif /* M68K */