/*
 * rt.c - STRUDELFORCE Amiga: minimale Runtime-Helfer fuer -nostdlib
 * ------------------------------------------------------------------
 * libgcc fehlt bei -nostdlib. fix16_mul() (game_logic.c) nutzt eine
 * 32x32->64-Bit-Multiplikation, der 68000 hat nur 16x16->32 (MULU.W).
 * __muldi3 wird deshalb hier aus 16-Bit-Teilprodukten gebaut - ohne
 * selbst wieder 64-Bit-Multiplikationen zu verwenden. Bit-genau zur
 * C-Referenz (Wraparound mod 2^64), damit der Determinismus-Test haelt.
 */
typedef unsigned int  u32;
typedef unsigned long long u64;

static u64 mul32x32(u32 a, u32 b)
{
    u32 a0 = a & 0xFFFFu, a1 = a >> 16;
    u32 b0 = b & 0xFFFFu, b1 = b >> 16;
    u32 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    u32 mid = (p00 >> 16) + (p01 & 0xFFFFu) + (p10 & 0xFFFFu);
    u32 lo  = (p00 & 0xFFFFu) | (mid << 16);
    u32 hi  = p11 + (p01 >> 16) + (p10 >> 16) + (mid >> 16);
    return ((u64)hi << 32) | lo;
}

u64 __muldi3(u64 a, u64 b)
{
    u32 al = (u32)a, ah = (u32)(a >> 32);
    u32 bl = (u32)b, bh = (u32)(b >> 32);
    u64 r = mul32x32(al, bl);
    u32 cross = ah * bl + al * bh;          /* nur untere 32 Bit relevant */
    return r + ((u64)cross << 32);
}

