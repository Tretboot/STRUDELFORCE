/* test_embedded_music.c - Selftest der in die EXE gepackten Tracks (PC-only)
 *
 * Verifiziert, dass die Embedded-Arrays (src/embedded_music.h, generiert von
 * tools/gen_embedded_music.py) laedbar sind und Ton erzeugen:
 *   1. Apidya Meadow .tfx + .sam  -> tfmx_player_load_mem  -> 44000 Samples rendern
 *   2. Wing Commander Combat      -> mod_player_load_mem   -> 44000 Samples rendern
 *   3. Wing Commander Victory     -> mod_player_load_mem   -> 44000 Samples rendern
 *
 * Exit-Code 0 = alle Embedded-Tracks OK.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tfmx_player.h"
#include "mod_player.h"
#include "embedded_music.h"

#define RATE 16574
#define N (4 * RATE) /* ~0.24 s pro Track */

static int check_peak(const int16_t *buf, int n) {
    int i, peak = 0;
    for (i = 0; i < n; i++) {
        int a = buf[i] < 0 ? -buf[i] : buf[i];
        if (a > peak) peak = a;
    }
    return peak;
}

int main(void) {
    int16_t buf[N];
    int fails = 0;
    TfmxPlayer tfx;
    ModPlayer mod;

    memset(buf, 0, sizeof(buf));
    tfmx_player_init(&tfx);
    if (!tfmx_player_load_mem(&tfx, EMB_TFMX_APIDYA_MEADOW, EMB_TFMX_APIDYA_MEADOW_LEN,
                              (const int8_t *)EMB_SAM_APIDYA_MEADOW, EMB_SAM_APIDYA_MEADOW_LEN)) {
        printf("[FAIL] Embedded TFMX %s laedt nicht\n", EMB_TFMX_APIDYA_MEADOW_NAME);
        fails++;
    } else {
        tfmx_player_set_rate(&tfx, RATE);
        tfmx_player_start(&tfx, 0);
        tfmx_player_render(&tfx, buf, N);
        {
            int peak = check_peak(buf, N);
            printf("[TFMX] %s: samples=%d peak=%d %s\n", EMB_TFMX_APIDYA_MEADOW_NAME,
                   (int)N, peak, peak > 100 ? "[PASS]" : "[FAIL]");
            if (peak <= 100) fails++;
        }
    }

    {
        const struct { const char *name; const uint8_t *data; uint32_t len; } mods[2] = {
            { EMB_MOD_WC_REGULARCOMBAT_NAME, EMB_MOD_WC_REGULARCOMBAT, EMB_MOD_WC_REGULARCOMBAT_LEN },
            { EMB_MOD_WC_OVERALLVICTORY_NAME, EMB_MOD_WC_OVERALLVICTORY, EMB_MOD_WC_OVERALLVICTORY_LEN },
        };
        int i;
        for (i = 0; i < 2; i++) {
            memset(&mod, 0, sizeof(mod));
            memset(buf, 0, sizeof(buf));
            if (!mod_player_load_mem(&mod, mods[i].data, mods[i].len)) {
                printf("[FAIL] Embedded MOD %s laedt nicht\n", mods[i].name);
                fails++;
                continue;
            }
            mod_player_start(&mod, RATE);
            mod_player_render(&mod, buf, N);
            {
                int peak = check_peak(buf, N);
                printf("[MOD] %s: samples=%d peak=%d %s\n", mods[i].name,
                       (int)N, peak, peak > 100 ? "[PASS]" : "[FAIL]");
                if (peak <= 100) fails++;
            }
        }
    }

    if (fails == 0) printf("EMBEDDED_MUSIC OK (3/3 Tracks)\n");
    else printf("EMBEDDED_MUSIC FAIL: %d Fehler\n", fails);
    return fails ? 1 : 0;
}
