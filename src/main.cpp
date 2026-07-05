#include <M5Cardputer.h>
#include "components/engine/engine.h"

static constexpr uint16_t SCREEN_W = 240;
static constexpr uint16_t SCREEN_H = 135;
static uint16_t framebuffer[SCREEN_W * SCREEN_H];

static Engine engine;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);          

    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) framebuffer[i] = 0; 

    DisplayDriver dd = create_builtin_display();
    InputDriver   id = create_builtin_input();
    SoundDriver   sd = create_builtin_sound();

    engine_init(&engine, dd, id, sd, framebuffer);
    engine.start(&engine);          
}

void loop() {
}
