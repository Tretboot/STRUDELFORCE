/*
 * amiga_music.c - INCBIN-Embedding der 4 kuratierten TFMX-Tracks (M68K-Build v0.3.1)
 * ------------------------------------------------------------------
 * .incbin-Pfade relativ zum CWD des Compilers (= Projekt-Root).
 * Tracks:
 *   1. Turrican 3: World 1 Factory Action (Chris Huelsbeck)
 *   2. BiFi Adventure (Chris Huelsbeck)
 *   3. Cybersty
 *   4. Mad TV: Ongame 6
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

/* Kuratierte Tracks (gestaged nach amiga_dist/music/) */
SF_INCBIN(t3w1_tfx,  "amiga_dist/music/T3W1.TFX")
SF_INCBIN(t3w1_sam,  "amiga_dist/music/T3W1.SAM")
SF_INCBIN(bifi_tfx,  "amiga_dist/music/BIFI.TFX")
SF_INCBIN(bifi_sam,  "amiga_dist/music/BIFI.SAM")
SF_INCBIN(cyber_tfx, "amiga_dist/music/CYBER.TFX")
SF_INCBIN(cyber_sam, "amiga_dist/music/CYBER.SAM")
SF_INCBIN(madtv_tfx, "amiga_dist/music/MADTV.TFX")
SF_INCBIN(madtv_sam, "amiga_dist/music/MADTV.SAM")

const uint8_t * const t2_tfx[T2_TRACK_COUNT] = {
    incbin_t3w1_tfx_start,
    incbin_bifi_tfx_start,
    incbin_cyber_tfx_start,
    incbin_madtv_tfx_start
};
const uint32_t t2_tfx_len[T2_TRACK_COUNT] = {
    SF_INCBIN_LEN(t3w1_tfx),
    SF_INCBIN_LEN(bifi_tfx),
    SF_INCBIN_LEN(cyber_tfx),
    SF_INCBIN_LEN(madtv_tfx)
};
const uint8_t * const t2_sam[T2_TRACK_COUNT] = {
    incbin_t3w1_sam_start,
    incbin_bifi_sam_start,
    incbin_cyber_sam_start,
    incbin_madtv_sam_start
};
const uint32_t t2_sam_len[T2_TRACK_COUNT] = {
    SF_INCBIN_LEN(t3w1_sam),
    SF_INCBIN_LEN(bifi_sam),
    SF_INCBIN_LEN(cyber_sam),
    SF_INCBIN_LEN(madtv_sam)
};
const char * const t2_names[T2_TRACK_COUNT] = {
    "Turrican 3: World 1 Factory",
    "BiFi Adventure",
    "Cybersty",
    "Mad TV: Ongame 6"
};

#endif /* M68K */
