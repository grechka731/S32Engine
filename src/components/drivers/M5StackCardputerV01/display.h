#pragma once
#include <cstdint>

// ИСПРАВЛЕНО: структура объявлена ДО функций (у тебя было наоборот).
// ИСПРАВЛЕНО: добавлен указатель init (был только flush).
struct DisplayDriver {
    uint16_t width;
    uint16_t height;
    void (*init)(DisplayDriver* self);
    void (*flush)(DisplayDriver* self, const uint16_t* frame_buffer);
};

DisplayDriver create_builtin_display();
