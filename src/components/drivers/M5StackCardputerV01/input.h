#pragma once
#include <cstdint>

// ОТСУТСТВОВАВШИЙ драйвер: InputDriver упоминался в engine, но не был описан.
struct InputState {
    bool up;
    bool down;
    bool left;
    bool right;
    bool action;    // Enter / Space
    bool quit;      // клавиша `
    char last_key;  // последний символ (0 = ничего)
};

struct InputDriver {
    InputState state;
    void (*init)(InputDriver* self);
    void (*update)(InputDriver* self);
};

InputDriver create_builtin_input();
