/*
 * sid_header.c - PSID/RSID header parser (groundwork for .sid support)
 * Playback needs a 6502 CPU core + SID (6581/8580) emulation -> see docs/ROADMAP_AUDIO.md.
 * This module only validates & describes a file so the jukebox/loader can list it.
 * Lizenz: MIT (Clean Room)
 */
#include "sid_header.h"
#include <string.h>

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

int sid_header_parse(SidHeader *h, const uint8_t *buf, uint32_t size) {
    memset(h, 0, sizeof(*h));
    if (!buf || size < 0x76) return 0;
    if (memcmp(buf, "PSID", 4) && memcmp(buf, "RSID", 4)) return 0;
    h->rsid = buf[0] == 'R';
    h->version = be16(buf + 4);
    h->data_offset = be16(buf + 6);
    h->load_addr = be16(buf + 8);
    h->init_addr = be16(buf + 10);
    h->play_addr = be16(buf + 12);
    h->songs = be16(buf + 14);
    h->start_song = be16(buf + 16);
    h->speed = ((uint32_t)be16(buf + 18) << 16) | be16(buf + 20);
    memcpy(h->name, buf + 0x16, 32); h->name[32] = '\0';
    memcpy(h->author, buf + 0x36, 32); h->author[32] = '\0';
    memcpy(h->released, buf + 0x56, 32); h->released[32] = '\0';
    if (h->data_offset > size) return 0;
    if (h->load_addr == 0 && h->data_offset + 2 <= size)          /* load address in data */
        h->load_addr = (uint16_t)(buf[h->data_offset] | (buf[h->data_offset + 1] << 8));
    if (h->version >= 2 && size >= 0x7C) h->flags = be16(buf + 0x76);
    return 1;
}
