{
        int16_t bass_note;
        GameVoice *bass = &a->voices[0];
        GameVoice *arp = &a->voices[1];
        if (a->bass_override_active) {
            bass_note = a->bass_override_note;
        } else {
            bass_note = BASSLINE[(a->tick / 5) % 8];
        }
        if (bass_note < 0) {
            bass->active = 0;
            arp->active = 0;
        } else {
            if (!bass->active) {
                voice_set(a, 0, a->bass_macro, bass_note, 32);
            } else {
                bass->macro_id = a->bass_macro;
                bass->base_note = bass_note;
                if (bass->volume < 32) bass->volume = 32;
            }
            if (a->arp_active) {
                if (!arp->active) {
                    voice_set(a, 1, 1, bass_note + 12, 24);
                } else {
                    arp->base_note = bass_note + 12;
                    if (arp->volume < 24) arp->volume = 24;
                }
            } else {
                arp->active = 0;
            }
        }
    }