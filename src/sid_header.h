/* sid_header.h - PSID/RSID header parser (playback: see docs/ROADMAP_AUDIO.md) */
#ifndef SID_HEADER_H
#define SID_HEADER_H
#include <stdint.h>
typedef struct {
    uint8_t rsid; uint16_t version, data_offset, load_addr, init_addr, play_addr, songs, start_song, flags;
    uint32_t speed; char name[33], author[33], released[33];
} SidHeader;
int sid_header_parse(SidHeader *h, const uint8_t *buf, uint32_t size);
#endif
