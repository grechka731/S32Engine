#pragma once

#include <cstdint>

typedef uint16_t Color;                        
#define GRX_COLOR_TRANSPARENT ((Color)0xF81F)

struct Point
{
    uint16_t x;
    uint16_t y;
};

struct Pixel
{
    Color color;
    Point point;
};


struct Sprite {
    const Color* pixels;                       
    uint16_t width;
    uint16_t height;
};

struct graphicsInfo {
    Color*   buffer;
    uint16_t width;
    uint16_t height;
};