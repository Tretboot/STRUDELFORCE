/*
 * sf_mixer.c - STRUDELFORCE multi-source mixer (TFMX + MOD + Strudel synth [+ SID later])
 * C89, integer only, no malloc. Each source renders mono s16 into a scratch block,
 * mixer sums with per-source gain (0..256 = 0..1.0) and optional ducking.
 * Lizenz: MIT (Clean Room)
 */
#include "sf_mixer.h"
#include <string.h>

void sf_mixer_init(SfMixer *mx) { memset(mx, 0, sizeof(*mx)); mx->master = 256; }

int sf_mixer_add(SfMixer *mx, SfRenderFn fn, void *ctx, int16_t gain, const char *name) {
    SfSource *s;
    if (mx->count >= SF_MIX_MAX_SOURCES || !fn) return -1;
    s = &mx->src[mx->count];
    s->fn = fn; s->ctx = ctx; s->gain = gain; s->target = gain; s->muted = 0; s->duck = 0;
    strncpy(s->name, name ? name : "src", sizeof(s->name) - 1); s->name[sizeof(s->name) - 1] = '\0';
    return mx->count++;
}

void sf_mixer_set_gain(SfMixer *mx, int idx, int16_t gain) {
    if (idx >= 0 && idx < mx->count) mx->src[idx].target = gain;   /* ramps, no clicks */
}

void sf_mixer_render(SfMixer *mx, int16_t *out, int n) {
    int done = 0, i, k;
    while (done < n) {
        int blk = n - done;
        int32_t sum[SF_MIX_BLOCK];
        if (blk > SF_MIX_BLOCK) blk = SF_MIX_BLOCK;
        memset(sum, 0, sizeof(sum));
        for (k = 0; k < mx->count; k++) {
            SfSource *s = &mx->src[k];
            int16_t g0, g1;
            s->fn(s->ctx, mx->scratch, blk);       /* always render: keeps sources in time */
            g0 = s->gain;
            if (s->gain < s->target) { s->gain = (int16_t)(s->gain + 8); if (s->gain > s->target) s->gain = s->target; }
            if (s->gain > s->target) { s->gain = (int16_t)(s->gain - 8); if (s->gain < s->target) s->gain = s->target; }
            g1 = s->gain;
            if (s->muted) continue;
            for (i = 0; i < blk; i++) {
                int32_t g = g0 + (((int32_t)(g1 - g0) * i) / blk);
                if (s->duck && mx->duck_level) g = (g * (256 - mx->duck_level)) >> 8;
                sum[i] += ((int32_t)mx->scratch[i] * g) >> 8;
            }
        }
        for (i = 0; i < blk; i++) {
            int32_t v = (sum[i] * mx->master) >> 8;
            if (v > 32767) { v = 32767; mx->clips++; } else if (v < -32768) { v = -32768; mx->clips++; }
            out[done + i] = (int16_t)v;
        }
        done += blk;
    }
}

/* Adapter for GameAudio.ext_render (game_audio.c v2 hook) */
void sf_mixer_ext_render(void *ctx, int16_t *out, int n) { sf_mixer_render((SfMixer *)ctx, out, n); }
