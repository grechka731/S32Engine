#include "components/drivers/M5StackCardputerV01/input.h"
#include <M5Cardputer.h>

static void cardputer_input_init(InputDriver* self) {
    self->state = InputState{};
}

static void cardputer_input_update(InputDriver* self) {
    M5Cardputer.update();              // обязательно обновить клавиатуру
    InputState s = InputState{};

    if (M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState k = M5Cardputer.Keyboard.keysState();
        for (char c : k.word) {
            s.last_key = c;
            switch (c) {
                case ';': s.up    = true; break;  // стрелки Cardputer: ; . , /
                case '.': s.down  = true; break;
                case ',': s.left  = true; break;
                case '/': s.right = true; break;
                case '`': s.quit  = true; break;
            }
        }
        if (k.enter) s.action = true;
        if (k.space) s.action = true;
    }

    self->state = s;
}

InputDriver create_builtin_input() {
    InputDriver id;
    id.init   = cardputer_input_init;
    id.update = cardputer_input_update;
    id.state  = InputState{};
    return id;
}
