/*
 * strudel_sketch.h - STRUDELFORCE Strudel Sketch Engine v2 (Clean Room, C99)
 * Lizenz: MIT
 *
 * Mehrstimmige Strudel/TidalCycles-Sketch-Engine fuer die
 * Switch-Angel-Referenz-Tracks (assets/strudel_tracks/, v2-Acceptance-Test).
 *
 * Unterstuetzte Oberflaeche (v2):
 *   - $NAME: / _$NAME: (stumm) / anonyme Stimmen, setCpm(x), all(postgain)
 *   - const-Bindings (Strings, Zahlen, slider()), function-Definitionen
 *     werden erkannt und uebersprungen, register('acidenv'/'o') werden
 *     als Builtins expandiert, samples('quelle') wird registriert
 *   - n("...") / s("...") + String-Methoden (.seg .add .velocity .slow .rev)
 *   - Pattern-Notation: Sequenz, <Alternation>*N, (k,n) Euklid, *N Speed,
 *     !N Repeat, @Velocity, Sample:Vel, Sample/2, ~ - Pausen, c2/eb3 Namen
 *   - Modifier-Ketten: trans/scale/add/orbit/gain/room/delay(±fb/sync)/pan/
 *     diode/fm(+wave/h)/detune/clip/unison/lpf/lpenv/lpq/lps/lpd/rlpf/hpf/
 *     att/dec/rel/begin/penv/pdec/duck(+depth/attack)/bank/fit/scrub/ribbon/
 *     wt/postgain/beat(len)/slow/ply/sometimesBy/almostNever/...
 *   - Reaktive Werte: rand, sine.fast(N), tri.range(a,b).fast(N), slider(),
 *     (tri # mul 4 # slow 16) in mondo
 *   - mondo`...` Template-DSL ($ Stimme, # Modifiers, # in Klammern)
 *   - blockArrange([[const], mask], [[letter, mod],...]) als Built-in:
 *     128-Slot-Arrangement-Gate pro Zeile (F on, T +12, D delay, B speed(-1),
 *     S stretch) — deterministische Approximation der Tidal-semantik
 *
 * Strikte Regeln: C99, null malloc (statische Pools), deterministisch
 * (alle rand-Werte aus LCG(cycle, step, voice, salt)).
 * Die v1-Engine (strudel_cleanroom.h) bleibt fuer die ESC-Konsole erhalten.
 */
#ifndef STRUDEL_SKETCH_H
#define STRUDEL_SKETCH_H

#include <stdint.h>

#define STU_MAX_VOICES         24
#define STU_MAX_MODS           24
#define STU_MAX_NODES          512
#define STU_MAX_CHILDREN       64
#define STU_MAX_BEAT_NODES     16
#define STU_MAX_CONSTS         16
#define STU_MAX_SAMPLE_NAMES   24
#define STU_NAME_LEN           24
#define STU_BEATS_MAX          64
#define STU_SRC_LEN            16384

/* Atom-Arten */
enum { STU_AT_REST = 0, STU_AT_NOTE, STU_AT_SAMPLE, STU_AT_VALUE };

/* Knoten-Arten */
enum {
    STU_N_ATOM = 0, STU_N_SEQ, STU_N_ALT, STU_N_EUCLID,
    STU_N_TRANS,    STU_N_SCALE, STU_N_ADDPAT, STU_N_BEAT,
    STU_N_SEG,      STU_N_FLIP
};

typedef struct {
    int type;
    /* ATOM */
    int atom;            /* STU_AT_* */
    float semis;         /* relative Halbtoene (z.B. -14, 4.5) */
    int16_t midi;        /* fuer Notennamen c2 (absolut, 0 = nicht gesetzt) */
    int16_t velocity;    /* 0..127, 0 = Standard */
    char sample[16];     /* "" oder z.B. "sbd" */
    int repeat;          /* !N (Standard 1) */
    int speed;           /* *N (Standard 1) */
    int fit;             /* /N (Standard 1) */
    /* EUCLID */
    int eucl_k, eucl_n;
    /* Wrapper */
    int trans_off;       /* STU_N_TRANS: +-Halbtoene */
    int scale_id;        /* STU_N_SCALE */
    int scale_root;      /* STU_N_SCALE: Wurzel-Halbton (0=C .. 11=B) */
    int child_start;     /* Index in StrudelSketch.child_pool */
    int nchildren;
    /* STU_N_BEAT: Positionen in StrudelSketch.beat_pool */
    int beat_idx;
    int nbeats;
    /* STU_N_SEG */
    int seg_n;
} StuNode;

/* Reaktiver Wert */
enum { STU_V_NUM = 0, STU_V_RAND, STU_V_LFO };
typedef struct {
    int kind;
    float num;
    int wave;          /* LFO: 0 = Sine, 1 = Triangle */
    float fast;        /* Zyklen pro Zyklus (1) */
    float slow;        /* 1 Zyklus pro N Zyklen (1) */
    float range_a;     /* .range(a,b) (0) */
    float range_b;     /* (1) */
    float mul;         /* # mul N (1) */
    float add;         /* .add(x) (0) */
} StuVal;

/* Modifier-Arten */
enum {
    STU_M_SYNTH = 1, STU_M_TRANS, STU_M_SCALE, STU_M_ADD, STU_M_ORBIT, STU_M_GAIN,
    STU_M_ROOM, STU_M_DELAY, STU_M_DELAYFB, STU_M_DELAYSYNC, STU_M_PAN, STU_M_DIODE,
    STU_M_FM, STU_M_FMW, STU_M_FMH, STU_M_DET, STU_M_CLIP, STU_M_UNISON,
    STU_M_LPF, STU_M_LPE, STU_M_LPQ, STU_M_LPS, STU_M_LPD, STU_M_RLPF, STU_M_HPF,
    STU_M_ATT, STU_M_DEC, STU_M_REL, STU_M_BEGIN, STU_M_PENV, STU_M_PDEC,
    STU_M_DUCK, STU_M_DUCKDEPTH, STU_M_DUCKATT, STU_M_BANK, STU_M_FIT,
    STU_M_SCRUB, STU_M_RIBBON, STU_M_WT, STU_M_POLY, STU_M_SOMETIMES,
    STU_M_ALMOSTNEVER, STU_M_WAVES, STU_M_POSTGAIN
};

typedef struct {
    int kind;
    float a, b;
    StuVal val;
    int has_val;
    char str[48];      /* Synth-Name / Bank / Duck-Ziel / ... */
    int node;            /* Pattern-Argument (Wurzelknoten), -1 = keines */
} StuMod;

typedef struct {
    char name[STU_NAME_LEN];
    int muted;
    int root;               /* Pattern-Wurzelknoten */
    int synth_is_sample;
    char synth[48];         /* "sawtooth", "supersaw,wt_digital:3", ... */
    int grid;               /* Schritte pro Zyklus (16 std, 32 slow/beat, ...) */
    int loop;               /* Schritte pro Loop (16 std, 128 blockArrange) */
    int vel_mode;           /* .velocity(): Pattern-Werte sind Lautstaerken */
    StuMod mods[STU_MAX_MODS];
    int nmods;
    int gate[128];          /* blockArrange: 0=aus | 1=an | 2=T | 4=D | 8=B | 16=S */
    int poly_n;             /* ply() */
    int slow_mult;          /* .slow(N): Grid-Vervielfachung (1) */
} StuVoice;

/* const-Binding (Skalare UND komplette Voice-Ausdraecke fuer blockArrange) */
typedef struct {
    char name[STU_NAME_LEN];
    int is_voicedef;
    StuVal val;
    int root;
    int synth_is_sample;
    char synth[48];
    char str[32];        /* fuer String-Consts (z.B. const sc = 'g:minor') */
    StuMod mods[STU_MAX_MODS];
    int nmods;
} StuConst;

typedef struct {
    int ok;
    char error[96];
    int nwarn;
    char warn_last[96];
    int cycles_per_measure;   /* setCpm (34 bei 136/4) */
    int nvoices;
    StuVoice voices[STU_MAX_VOICES];
    StuNode nodes[STU_MAX_NODES];
    int nnodes;
    int child_pool[STU_MAX_NODES * STU_MAX_CHILDREN];
    int child_count;
    float beat_pool[STU_MAX_BEAT_NODES * STU_BEATS_MAX];
    int nbeat_nodes;
    float postgain;
    char sample_source[128];  /* samples('...') */
    int has_acidenv;          /* register('acidenv') gefunden */
    int n_consts;
    StuConst consts[STU_MAX_CONSTS];
    char sample_names[STU_MAX_SAMPLE_NAMES][16];
    int n_samples;
} StrudelSketch;

/* Pro-16tel-Event-Ausgabe (1 Stimme pro Eintrag, max. STU_MAX_VOICES) */
typedef struct {
    int active;
    int voice;
    int16_t note;        /* MIDI (0 = keins, nur Sample) */
    int16_t velocity;    /* 0..127 */
    char sample[16];
    char synth[48];
    int repeat;
    float gain, room, delay, delayfb, pan, detune, clip, unison,
          lpf, lpe, lpq, lps, lpd, rlpf, hpf, att, dec, rel, begin,
          penv, pdec, duckdepth, duckatt, fm, fmh, diode, scrub,
          ribbon, delaysync, fit_f;
    int orbit, bank, duck_mask, poly, fit;
    int gate_letter;     /* Maskenbuchstaben (bitmask) am aktuellen Slot */
} StuEvent;

typedef struct {
    StuEvent ev[STU_MAX_VOICES];
} StuStepEvents;

/* Kompiliert den Sketch (0 = ok, -1 = Fehler in sk->error). Kein malloc. */
int  strudel_sketch_compile(StrudelSketch *sk, const char *src);
/* Ereignis-Gitter abfragen: cycle = Zyklus-Nr., step = 0..15 (16tel) */
void strudel_sketch_step(const StrudelSketch *sk, uint32_t cycle, uint32_t step,
                         StuStepEvents *out);
int  strudel_sketch_grid(const StrudelSketch *sk, int vidx);
void strudel_sketch_print(const StrudelSketch *sk);

#endif /* STRUDEL_SKETCH_H */
