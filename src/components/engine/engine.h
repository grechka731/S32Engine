#pragma once
#include <cstdint>

#include "components/drivers/M5StackCardputerV01/display.h"
#include "components/drivers/M5StackCardputerV01/input.h"
#include "components/drivers/M5StackCardputerV01/sound.h"
#include "components/graphics/graphics.h"
#include "components/other/struct.h"

struct Engine {
    //drivers
    DisplayDriver dd;
    InputDriver   id;
    SoundDriver   sd;

    //grx
    graphicsInfo ggf;

    Color* framebuffer;  
    bool      is_running;

    void (*init)(Engine* self, DisplayDriver dd, InputDriver id, SoundDriver sd,
                 uint16_t* framebuffer);
    void (*update)(Engine* self, int dt);
    void (*shutdown)(Engine* self);
    void (*start)(Engine* self);
};

void engine_init(Engine* self, DisplayDriver dd, InputDriver id, SoundDriver sd,
                 uint16_t* framebuffer);
