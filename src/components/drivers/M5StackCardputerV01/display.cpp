#include "components/drivers/M5StackCardputerV01/display.h"
#include <M5Cardputer.h>

static void cardputer_display_init(DisplayDriver* self) {
    // M5Cardputer.begin() вызывается в main ДО этого
    self->width  = M5Cardputer.Display.width();   // 240
    self->height = M5Cardputer.Display.height();  // 135
}

static void cardputer_display_flush(DisplayDriver* self, const uint16_t* frame_buffer) {
    M5Cardputer.Display.pushImage(0, 0, self->width, self->height, frame_buffer);
}

DisplayDriver create_builtin_display() {
    DisplayDriver dd;
    dd.width  = 0;
    dd.height = 0;
    dd.init   = cardputer_display_init;
    dd.flush  = cardputer_display_flush;
    return dd;
}
