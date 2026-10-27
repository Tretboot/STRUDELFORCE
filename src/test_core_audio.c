/*
 * Selbsttest für Agenten: Prüft den Clean-Room Strudel-Parser und die 50Hz TFMX-Engine
 */
#include <stdio.h>
#include "strudel_cleanroom.h"
#include "tfmx_macro_synth.h"

int main(void) {
    StrudelPattern pat;
    const char *live_code = "c2 [eb2 g2] c3*2 <bb2 g2(3,8)>";
    strudel_compile(&pat, live_code, 0);

    printf("=== STRUDELFORCE Clean-Room Audio Core Self-Test ===\n");
    printf("Live-Code Pattern: %s\n", pat.raw_source);
    printf("Generierte AST-Knoten: %u\n\n", pat.node_count);

    for (uint32_t cycle = 0; cycle < 2; cycle++) {
        printf("Cycle %u: ", cycle);
        for (uint32_t step = 0; step < 16; step++) {
            int16_t note = strudel_query_step(&pat, cycle, step, 16);
            if (note >= 0) printf("[%d] ", note);
            else if (note == -1) printf(" ~  ");
            else printf(" .  ");
        }
        printf("\n");
    }

    CyberPaulaEngine paula;
    cyber_paula_init_waves(&paula);
    printf("\nTFMX 50Hz Macro Engine bereit. Sample-RAM initialisiert (%d Bytes).\n", SAMPLE_RAM_SIZE);
    return 0;
}
