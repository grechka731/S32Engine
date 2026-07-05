#include "graphics.h"

void grx_clear(const graphicsInfo* g, Color color) {
    const int total = g->width * g->height;
    for (int i = 0; i < total; i++)
        g->buffer[i] = color;
}

void grx_setPixel(const graphicsInfo* g, Color color, Point p) {
    if (p.x < 0 || p.y < 0 || p.x >= g->width || p.y >= g->height)
        return;
    g->buffer[(uint32_t)p.y * g->width + p.x] = color;
}

void grx_setPixel(const graphicsInfo* g, Pixel pixel) {
    grx_setPixel(g, pixel.color, pixel.point);
}

Color grx_getPixel(const graphicsInfo* g, Point p) {
    if (p.x < 0 || p.y < 0 || p.x >= g->width || p.y >= g->height)
        return GRX_COLOR_TRANSPARENT;
    return g->buffer[(uint32_t)p.y * g->width + p.x];
}

void grx_fillRect(const graphicsInfo* g, Color color, Point p0, Point p1) {
    int x0 = p0.x < p1.x ? p0.x : p1.x;
    int y0 = p0.y < p1.y ? p0.y : p1.y;
    int x1 = p0.x > p1.x ? p0.x : p1.x;
    int y1 = p0.y > p1.y ? p0.y : p1.y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= g->width)  x1 = g->width  - 1;
    if (y1 >= g->height) y1 = g->height - 1;
    for (int y = y0; y <= y1; y++) {
        Color* row = g->buffer + (uint32_t)y * g->width;
        for (int x = x0; x <= x1; x++)
            row[x] = color;
    }
}

void grx_sprite(const graphicsInfo* g, Sprite sprite, Point leftTop) {
    for (int sy = 0; sy < sprite.height; sy++) {
        int screenY = leftTop.y + sy;
        if (screenY < 0 || screenY >= g->height) continue;
        const Color* srcRow = sprite.pixels + (uint32_t)sy * sprite.width;
        Color*       dstRow = g->buffer     + (uint32_t)screenY * g->width;
        for (int sx = 0; sx < sprite.width; sx++) {
            int screenX = leftTop.x + sx;
            if (screenX < 0 || screenX >= g->width) continue;
            Color c = srcRow[sx];
            if (c != GRX_COLOR_TRANSPARENT)
                dstRow[screenX] = c;
        }
    }
}