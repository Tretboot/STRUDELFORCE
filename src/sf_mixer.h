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
