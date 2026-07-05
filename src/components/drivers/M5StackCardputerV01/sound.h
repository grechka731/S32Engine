#pragma once
#include <cstdint>

// ИСПРАВЛЕНО: раньше структуры SoundDriver вообще не было — теперь объявлена.
struct SoundDriver {
    void (*init)(SoundDriver* self);
    void (*play_wav_on_channel)(SoundDriver* self, uint8_t channel,
                                const uint8_t* wav_data, uint32_t wav_size);
    void (*stop_channel)(SoundDriver* self, uint8_t channel);
};

SoundDriver create_builtin_sound();
