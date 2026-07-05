#include "components/drivers/M5StackCardputerV01/sound.h"
#include <M5Cardputer.h>

// ИСПРАВЛЕНО: был self-include (#include "sound.h" сам в себя) и лишняя 's' в конце.

static void cardputer_sound_init(SoundDriver* self) {
    M5Cardputer.Speaker.begin();
}

static void cardputer_sound_play_wav(SoundDriver* self, uint8_t channel,
                                     const uint8_t* wav_data, uint32_t wav_size) {
    M5Cardputer.Speaker.playWav(wav_data, wav_size, 1, channel);
}

static void cardputer_sound_stop_channel(SoundDriver* self, uint8_t channel) {
    M5Cardputer.Speaker.stop(channel);
}

SoundDriver create_builtin_sound() {
    SoundDriver sd;
    sd.init                = cardputer_sound_init;
    sd.play_wav_on_channel = cardputer_sound_play_wav;
    sd.stop_channel        = cardputer_sound_stop_channel;
    return sd;
}
