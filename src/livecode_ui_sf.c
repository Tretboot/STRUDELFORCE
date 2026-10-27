#include "sf_interfaces.h"
/*
 * STRUDELFORCE — AGENT 4 (UI, Parser & Biome-Generator)
 * livecode_ui.c — ESC-Live-Coding-Menü:
 *   - 512-Byte Source-Editor mit Cursor (Insert/Delete/Home/End/Left/Right)
 *   - CTRL+ENTER: Compile + Swap-Pending (main_pc.c übergibt an Cyber-Paula)
 *   - F2: Save nach user_music/unsaved.strudel
 *   - F3: kein Action hier (main_pc.c ruft livecode_list_user_music)
 *
 * C99, 68k-kompatibel: KEIN float, KEIN malloc, KEINE VLAs.
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>

#define LIVECODE_SOURCE_LEN 512    /* char source[512] in LiveCodeUI */

static void livecode_set_error(LiveCodeUI *ui, const char *msg)
{
    int i = 0;
    while (msg[i] != 0 && i < 63) { ui->error[i] = msg[i]; i++; }
    ui->error[i] = 0;
}

static int has_atoms(const LiveCodeUI *ui)
{
    return (ui->compiled.node_count > 1) ? 1 : 0;
}

static void livecode_ensure_parent_dir(const char *path)
{
    const char *slash = 0;
    const char *p;
    char dir[128];
    for (p = path; *p != 0; p++) { if (*p == '/') slash = p; }
    if (slash != 0 && (slash - path) > 0 && (size_t)(slash - path) < sizeof(dir)) {
        memcpy(dir, path, (size_t)(slash - path));
        dir[slash - path] = 0;
        (void)mkdir(dir, 0775);    /* EEXIST/EACCES bewusst ignoriert */
    }
}

void livecode_init(LiveCodeUI *ui, const char *initial_source)
{
    if (ui == 0) return;
    memset(ui, 0, sizeof(LiveCodeUI));
    ui->open = 1;
    ui->tab  = 0;
    if (initial_source != 0) {
        strncpy(ui->source, initial_source, LIVECODE_SOURCE_LEN - 1);
    }
    ui->source[LIVECODE_SOURCE_LEN - 1] = 0;
    ui->cursor = (int16_t)strlen(ui->source);
    strudel_compile(&ui->compiled, ui->source, 0);
    ui->compile_ok = (ui->compiled.node_count > 0 && has_atoms(ui)) ? 1 : 0;
}

void livecode_on_input(LiveCodeUI *ui, uint16_t input_mask, uint16_t ascii_or_0)
{
    if (ui == 0) return;
    if (input_mask & SF_INPUT_ESCAPE) { ui->open = 0; return; }
    if (!ui->open) return;

    /* CTRL+ENTER: Pattern kompilieren und Swap an Paula anfordern. */
    if ((input_mask & 0x8000u) || (ascii_or_0 == 13 && (input_mask & 0x0100u))) {
        livecode_compile_swap(ui);
        return;
    }
    if (ascii_or_0 == 0xF2u) { livecode_save(ui, "user_music/unsaved.strudel"); return; }
    if (ascii_or_0 == 0xF3u) { return; } /* F3: main_pc.c zeigt BYO-Dialog */

    if (ascii_or_0 >= 32 && ascii_or_0 < 127) {
        size_t len = strlen(ui->source);
        size_t pos = (size_t)ui->cursor;
        if (pos > len) pos = len;
        if (len < (size_t)(LIVECODE_SOURCE_LEN - 1)) {
            memmove(ui->source + pos + 1, ui->source + pos, len - pos + 1);
            ui->source[pos] = (char)ascii_or_0;
            ui->cursor = (int16_t)pos + 1;
        }
        return;
    }
    if (ascii_or_0 == 8u) { /* BACKSPACE */
        if (ui->cursor > 0) {
            size_t len = strlen(ui->source);
            size_t pos = (size_t)ui->cursor - 1;
            memmove(ui->source + pos, ui->source + pos + 1, len - pos);
            ui->cursor--;
        }
        return;
    }
    if (ascii_or_0 == 1u) { ui->cursor = 0; return; }        /* HOME */
    if (ascii_or_0 == 4u) { ui->cursor = (int16_t)strlen(ui->source); return; } /* END */

    if (ascii_or_0 == 0) {
        if (input_mask & SF_INPUT_LEFT)  { if (ui->cursor > 0) { ui->cursor--; } return; }
        if (input_mask & SF_INPUT_RIGHT) {
            int len = (int)strlen(ui->source);
            if (ui->cursor < len) { ui->cursor++; }
        }
    }
}

int livecode_compile_swap(LiveCodeUI *ui)
{
    char trimmed[512];
    const char *src;
    const char *end;
    size_t len;

    if (ui == 0) return 0;

    src = ui->source;
    while (*src == ' ' || *src == '\t' || *src == '\n' || *src == '\r') src++;
    end = src + strlen(src);
    while (end > src && (end[-1] == ' ' || end[-1] == '\t' ||
           end[-1] == '\n' || end[-1] == '\r')) end--;
    len = (size_t)(end - src);
    if (len > sizeof(trimmed) - 1) len = sizeof(trimmed) - 1;
    memcpy(trimmed, src, len);
    trimmed[len] = 0;

    if (len == 0) {
        livecode_set_error(ui, "Empty source");
        ui->compile_ok = 0;
        ui->swap_pending = 0;
        return 0;
    }

    strudel_compile(&ui->compiled, trimmed, 0);
    if (ui->compiled.node_count <= 1) {
        livecode_set_error(ui, "No notes in pattern");
        ui->compile_ok = 0;
        ui->swap_pending = 0;
        return 0;
    }

    ui->compile_ok = 1;
    ui->swap_pending = 1;   /* -> main_pc.c: cyber_paula_set_pending_pattern() */
    ui->error[0] = 0;
    return 1;
}

int livecode_save(const LiveCodeUI *ui, const char *path)
{
    FILE *f;
    if (ui == 0 || path == 0) return 0;
    livecode_ensure_parent_dir(path);
    f = fopen(path, "w");
    if (f == 0) return 0;
    fprintf(f, "%s\n", ui->source);
    fclose(f);
    return 1;
}

int livecode_load(LiveCodeUI *ui, const char *path)
{
    FILE *f;
    size_t len;
    if (ui == 0 || path == 0) return 0;
    f = fopen(path, "r");
    if (f == 0) return 0;
    ui->source[0] = 0;
    if (fgets(ui->source, LIVECODE_SOURCE_LEN - 1, f) != 0) {
        len = strlen(ui->source);
        if (len > 0 && ui->source[len - 1] == '\n') ui->source[len - 1] = 0;
    }
    fclose(f);
    strudel_compile(&ui->compiled, ui->source, 0);
    ui->compile_ok = (ui->compiled.node_count > 1) ? 1 : 0;
    ui->cursor = (int16_t)strlen(ui->source);
    return 1;
}

int livecode_list_user_music(char (*names)[64], int max, int *count)
{
    DIR *d;
    struct dirent *e;
    int n = 0;
    if (count == 0) return 0;
    *count = 0;
    (void)mkdir("user_music", 0775);     /* Best-effort */
    d = opendir("user_music");
    if (d == 0) return 0;
    while (n < max && (e = readdir(d)) != 0) {
        const char *nm = e->d_name;
        size_t len = strlen(nm);
        int matches = 0;
        if (len > 8 && memcmp(nm + len - 8, ".strudel", 8) == 0) matches = 1;
        else if (len > 4 && memcmp(nm + len - 4, ".mod", 4) == 0) matches = 1;
        else if (len > 5 && memcmp(nm + len - 5, ".tfmx", 5) == 0) matches = 1;
        if (matches) {
            if (names != 0) {
                size_t c = (len < 63) ? len : 63;
                memcpy(names[n], nm, c);
                names[n][c] = 0;
            }
            n++;
        }
    }
    closedir(d);
    *count = n;
    return 1;
}

const char *livecode_get_source(const LiveCodeUI *ui)
{
    if (ui == 0) return "";
    return ui->source;
}
