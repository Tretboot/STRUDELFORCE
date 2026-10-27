/*
 * amiga_crt.h - STRUDELFORCE M68K-Build: minimale C-Runtime-Shims
 * ------------------------------------------------------------------
 * Die Amiga-Builds (sowohl m68k-amigaos-gcc als auch die self-contained
 * m68k-amiga-elf-gcc der Bartman-VS-Code-Extension mit -nostdlib) duerfen
 * nicht auf die C-Standardlibrary angewiesen sein. Hier stehen die wenigen
 * Funktionen, die die 5 Amiga-Module (AMIGA_SRCS) benoetigen, als statische
 * Implementierungen + Token-Ersatz.
 *
 * Nur aktiv mit -DM68K; die PC-/Windows-Builds nutzen die echte libc.
 * C89-konform, kein malloc, deterministisch.
 */
#ifndef AMIGA_CRT_H
#define AMIGA_CRT_H

#ifdef M68K

static void *sf_memset(void *dst, int c, unsigned long n)
{
    unsigned char *d = (unsigned char *)dst;
    while (n-- > 0u) *d++ = (unsigned char)c;
    return dst;
}

static int sf_memcmp(const void *a, const void *b, unsigned long n)
{
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    while (n-- > 0u) {
        if (*x != *y) return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}

#define memset(d, c, n)  sf_memset((d), (c), (unsigned long)(n))
#define memcmp(a, b, n)  sf_memcmp((a), (b), (unsigned long)(n))

#endif /* M68K */
#endif /* AMIGA_CRT_H */