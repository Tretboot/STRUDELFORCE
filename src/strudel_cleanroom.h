/*
 * strudel_cleanroom.h - Clean-Room C99 Implementierung der TidalCycles/Strudel Mini-Notation
 * Lizenz: MIT (Keine GPL/AGPL Abhängigkeiten, 100% eigener Code auf Basis rationaler Zyklus-Mathematik)
 *
 * Unterstützte Syntax:
 *   - Sequenzen & Subdivisions: "c2 [eb2 g2] c3 [bb2 g2 f2 eb2]"
 *   - Pausen: "~" oder "-"
 *   - Speed-Multiplikatoren: "c2*4" (schneller)
 *   - Alternation pro Zyklus: "<c2 eb2 f2 g2>"
 *   - Euklidische Rhythmen (Bjorklund): "c2(3,8)"
 */

#ifndef STRUDEL_CLEANROOM_H
#define STRUDEL_CLEANROOM_H

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define STRUDEL_MAX_NODES 128
#define STRUDEL_MAX_CHILDREN 16
#define STRUDEL_MAX_EVENTS 64

/* Rationale Zeit (Bruchrechnung wie in Haskell Data.Ratio) */
typedef struct {
    int32_t num;
    int32_t den;
} StrudelFrac;

typedef enum {
    S_NODE_ATOM,      /* Einzelne Note (MIDI-Nummer oder -1 für Pause '~') */
    S_NODE_SEQ,       /* [a b c] - Teilt das Zeitfenster gleichmäßig auf */
    S_NODE_ALT,       /* <a b c> - Wechselt pro Cycle-Nummer */
    S_NODE_EUCLID     /* a(k,n)  - Verteilt k Impulse auf n Schritte */
} StrudelNodeType;

typedef struct {
    StrudelNodeType type;
    int16_t midi_note;     /* -1 = Pause (~), sonst MIDI Note (z.B. 36 = C2) */
    uint8_t instrument_id; /* Verweis auf TFMX Macro oder Sample */
    uint8_t repeat_mult;   /* *N Multiplikator (Standard: 1) */
    uint8_t euclid_k;      /* Impulse für (k,n) */
    uint8_t euclid_n;      /* Schritte für (k,n) */
    uint8_t num_children;
    uint8_t children[STRUDEL_MAX_CHILDREN];
} StrudelNode;

typedef struct {
    StrudelNode nodes[STRUDEL_MAX_NODES];
    uint16_t node_count;
    uint8_t root_idx;
    char raw_source[512];  /* Für den Live-Code-Editor im ESC-Menü */
} StrudelPattern;

typedef struct {
    StrudelFrac start;
    StrudelFrac end;
    int16_t midi_note;
    uint8_t instrument_id;
} StrudelEvent;

/* Hilfsfunktion: Wandelt Notennamen wie "c2", "eb3", "f#4", "~" in MIDI-Noten um */
static int16_t strudel_parse_note_token(const char *tok) {
    int base = -1;
    int idx = 1;
    int octave = 3;
    if (!tok || tok[0] == '~' || tok[0] == '-') return -1;
    switch (tolower((unsigned char)tok[0])) {
        case 'c': base = 0; break;
        case 'd': base = 2; break;
        case 'e': base = 4; break;
        case 'f': base = 5; break;
        case 'g': base = 7; break;
        case 'a': base = 9; break;
        case 'b': base = 11; break;
        default: return -1;
    }
    if (tok[idx] == '#' || tok[idx] == 's') { base += 1; idx++; }
    else if (tok[idx] == 'b') { base -= 1; idx++; }
    if (isdigit((unsigned char)tok[idx])) {
        octave = tok[idx] - '0';
    }
    return (int16_t)((octave + 1) * 12 + base);
}

static uint8_t strudel_alloc_node(StrudelPattern *pat, StrudelNodeType type) {
    uint8_t idx;
    if (pat->node_count >= STRUDEL_MAX_NODES) return 0;
    idx = (uint8_t)pat->node_count++;
    memset(&pat->nodes[idx], 0, sizeof(StrudelNode));
    pat->nodes[idx].type = type;
    pat->nodes[idx].midi_note = -1;
    pat->nodes[idx].repeat_mult = 1;
    return idx;
}

/* Rekursiver Descent-Parser für Strudel Mini-Notation */
static const char* strudel_parse_expr(StrudelPattern *pat, const char *p, uint8_t parent_idx, char close_char) {
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == close_char) {
            if (*p == close_char) p++;
            break;
        }
        if (*p == '[' || *p == '<') {
            char expected_close = (*p == '[') ? ']' : '>';
            StrudelNodeType ntype = (*p == '[') ? S_NODE_SEQ : S_NODE_ALT;
            uint8_t child_idx = strudel_alloc_node(pat, ntype);
            if (pat->nodes[parent_idx].num_children < STRUDEL_MAX_CHILDREN) {
                pat->nodes[parent_idx].children[pat->nodes[parent_idx].num_children++] = child_idx;
            }
            p = strudel_parse_expr(pat, p + 1, child_idx, expected_close);
            if (*p == '*') {
                p++;
                pat->nodes[child_idx].repeat_mult = (uint8_t)atoi(p);
                while (isdigit((unsigned char)*p)) p++;
            }
        } else {
            char tok[32];
            int ti = 0;
            uint8_t atom_idx;
            char *star;
            char *paren;
            int k = 1, n = 1;
            while (*p && !isspace((unsigned char)*p) && *p != ']' && *p != '>' && *p != '[' && *p != '<') {
                if (ti < 31) tok[ti++] = *p;
                p++;
            }
            tok[ti] = '\0';

            atom_idx = strudel_alloc_node(pat, S_NODE_ATOM);
            star = strchr(tok, '*');
            paren = strchr(tok, '(');
            if (star) {
                *star = '\0';
                pat->nodes[atom_idx].repeat_mult = (uint8_t)atoi(star + 1);
            }
            if (paren) {
                *paren = '\0';
                int k = 1, n = 1;
                if (sscanf(paren + 1, "%d,%d", &k, &n) == 2 && n > 0) {
                    pat->nodes[atom_idx].type = S_NODE_EUCLID;
                    pat->nodes[atom_idx].euclid_k = (uint8_t)k;
                    pat->nodes[atom_idx].euclid_n = (uint8_t)n;
                }
            }
            pat->nodes[atom_idx].midi_note = strudel_parse_note_token(tok);
            if (pat->nodes[parent_idx].num_children < STRUDEL_MAX_CHILDREN) {
                pat->nodes[parent_idx].children[pat->nodes[parent_idx].num_children++] = atom_idx;
            }
        }
    }
    return p;
}

static void strudel_compile(StrudelPattern *pat, const char *code, uint8_t default_inst) {
    uint16_t i;
    memset(pat, 0, sizeof(StrudelPattern));
    strncpy(pat->raw_source, code, sizeof(pat->raw_source) - 1);
    pat->root_idx = strudel_alloc_node(pat, S_NODE_SEQ);
    strudel_parse_expr(pat, code, pat->root_idx, '\0');
    for (i = 0; i < pat->node_count; i++) {
        pat->nodes[i].instrument_id = default_inst;
    }
}

/* Evaluiert den AST für einen gegebenen Zyklus (cycle_num) und gibt Events im [0..steps_per_cycle] Raster zurück */
static int16_t strudel_query_step(const StrudelPattern *pat, uint32_t cycle_num, uint32_t step_in_cycle, uint32_t total_steps) {
    uint8_t curr = pat->root_idx;
    uint32_t local_step = step_in_cycle;
    uint32_t span = total_steps;
    int depth;

    for (depth = 0; depth < 8; depth++) {
        const StrudelNode *n = &pat->nodes[curr];
        if (n->repeat_mult > 1 && span >= n->repeat_mult) {
            span /= n->repeat_mult;
            local_step %= span;
        }
        if (n->type == S_NODE_ATOM) {
            return (local_step == 0) ? n->midi_note : -2; /* -2 = sustain, -1 = rest */
        } else if (n->type == S_NODE_EUCLID) {
            uint32_t sub_span;
            uint32_t e_idx;
            int hit;
            if (n->euclid_n == 0) return -1;
            sub_span = span / n->euclid_n;
            if (sub_span == 0) sub_span = 1;
            if (local_step % sub_span != 0) return -2;
            e_idx = (local_step / sub_span) % n->euclid_n;
            hit = ((e_idx * n->euclid_k) % n->euclid_n) < n->euclid_k;
            return hit ? n->midi_note : -1;
        } else if (n->type == S_NODE_ALT) {
            if (n->num_children == 0) return -1;
            curr = n->children[cycle_num % n->num_children];
        } else if (n->type == S_NODE_SEQ) {
            uint32_t child_span;
            uint8_t c_idx;
            if (n->num_children == 0) return -1;
            child_span = span / n->num_children;
            if (child_span == 0) child_span = 1;
            c_idx = (uint8_t)(local_step / child_span);
            if (c_idx >= n->num_children) c_idx = n->num_children - 1;
            local_step %= child_span;
            span = child_span;
            curr = n->children[c_idx];
        }
    }
    return -1;
}

#endif /* STRUDEL_CLEANROOM_H */
