#pragma once

#include <cstdint>
#include "components/other/struct.h"

void  grx_clear   (const graphicsInfo* g, Color color);
void  grx_setPixel(const graphicsInfo* g, Color color, Point p);
void  grx_setPixel(const graphicsInfo* g, Pixel pixel);        
Color grx_getPixel(const graphicsInfo* g, Point p);
void  grx_fillRect(const graphicsInfo* g, Color color, Point p0, Point p1);
void  grx_sprite  (const graphicsInfo* g, Sprite sprite, Point leftTop);
