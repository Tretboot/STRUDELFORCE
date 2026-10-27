/*
 * test_audio_mix.c - LOCAL ONLY (needs gcc): renders TFMX and/or MOD through sf_mixer
 *
 *   test_audio_mix --tfmx file.tfx [--sub N] --mod song.mod --secs 20 --wav out.wav
 *   test_audio_mix --tfmx file.tfx --secs 5 --raw tfmx_c.raw   (compare with tools/c_vs_oracle.py)
 *
 * Exit code 0 = rendered, 2 = load failure. Prints peak / clip counters for CI.
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tfmx_player.h"
#include "mod_player.h"
#include "sf_mixer.h"

static TfmxPlayer g_tfmx;      /* static: keep big structs off the stack */
static ModPlayer  g_mod;
static SfMixer    g_mix;

static void tfmx_src(void *c, int16_t *o, int n) { tfmx_player_render((TfmxPlayer *)c, o, n); }
static void mod_src(void *c, int16_t *o, int n)  { mod_player_render((ModPlayer *)c, o, n); }

static uint8_t *slurp(const char *path, uint32_t *size) {
    FILE *f = fopen(path, "rb"); long n; uint8_t *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n + 1);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f); *size = (uint32_t)n; return b;
}

static void wav_header(FILE *f, uint32_t frames, uint32_t rate) {
    uint32_t v; uint16_t s;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 2; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    s = 1; fwrite(&s, 2, 1, f); fwrite(&s, 2, 1, f);
    fwrite(&rate, 4, 1, f); v = rate * 2; fwrite(&v, 4, 1, f);
    s = 2; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 2; fwrite(&v, 4, 1, f);
}

int main(int argc, char **argv) {
    const char *tfx = NULL, *mod = NULL, *wav = NULL, *raw = NULL;
    int sub = 0, secs = 10, i, peak = 0;
    uint32_t rate = 16574, total, done = 0;
    FILE *out = NULL;
    int16_t buf[1024];
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--tfmx") && i + 1 < argc) tfx = argv[++i];
        else if (!strcmp(argv[i], "--mod") && i + 1 < argc) mod = argv[++i];
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else if (!strcmp(argv[i], "--raw") && i + 1 < argc) raw = argv[++i];
        else if (!strcmp(argv[i], "--sub") && i + 1 < argc) sub = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = atoi(argv[++i]);
    }
    sf_mixer_init(&g_mix);
    if (tfx) {
        tfmx_player_init(&g_tfmx);
        if (!tfmx_player_load(&g_tfmx, tfx, NULL)) { fprintf(stderr, "TFMX load failed: %s\n", tfx); return 2; }
        tfmx_player_set_rate(&g_tfmx, rate);
        tfmx_player_start(&g_tfmx, (uint8_t)sub);
        sf_mixer_add(&g_mix, tfmx_src, &g_tfmx, mod ? 180 : 256, "tfmx");
        printf("TFMX '%s' steps=%u subs=%u\n", g_tfmx.title, g_tfmx.num_steps, g_tfmx.num_subsongs);
    }
    if (mod) {
        uint32_t sz; uint8_t *b = slurp(mod, &sz);
        if (!b || !mod_player_load_mem(&g_mod, b, sz)) { fprintf(stderr, "MOD load failed: %s\n", mod); return 2; }
        mod_player_start(&g_mod, rate);
        sf_mixer_add(&g_mix, mod_src, &g_mod, tfx ? 180 : 256, "mod");
        printf("MOD '%s' ch=%u len=%u\n", g_mod.title, g_mod.nch, g_mod.songlen);
    }
    if (!g_mix.count) { fprintf(stderr, "usage: --tfmx f.tfx and/or --mod f.mod [--wav o.wav|--raw o.raw]\n"); return 1; }
    total = rate * (uint32_t)secs;
    if (wav) { out = fopen(wav, "wb"); if (out) wav_header(out, total, rate); }
    else if (raw) out = fopen(raw, "wb");
    while (done < total) {
        int n = (int)(total - done > 1024 ? 1024 : total - done);
        sf_mixer_render(&g_mix, buf, n);
        for (i = 0; i < n; i++) { int a = buf[i] < 0 ? -buf[i] : buf[i]; if (a > peak) peak = a; }
        if (out) fwrite(buf, sizeof(int16_t), (size_t)n, out);
        done += (uint32_t)n;
    }
    if (out) fclose(out);
    printf("RESULT frames=%u peak=%d clips=%lu tfmx_oob=%lu\n", total, peak,
           (unsigned long)g_mix.clips, (unsigned long)(tfx ? g_tfmx.oob_reads : 0));
    return 0;
}
