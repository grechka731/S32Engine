<img width="537" height="399" alt="avocard" src="https://github.com/user-attachments/assets/c1553bbd-e334-4b78-aa43-8a583718e31e" />
# S32Engine

![platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![board](https://img.shields.io/badge/board-M5Cardputer-orange)
![framework](https://img.shields.io/badge/framework-Arduino-00979D)
![license](https://img.shields.io/badge/license-GPL--3.0-green)

A small framebuffer game engine for the M5Cardputer (M5StampS3 / ESP32-S3).

You draw into a single pixel buffer and the engine runs the main loop, polls input and pushes that buffer to the screen once per frame. No scenes, no ECS, no resource managers. Just enough to get pixels moving on the Cardputer display and build a small game on top.

[Русская версия](README.ru.md)

## Features

- Double-free-friendly software framebuffer (RGB565), no direct-to-screen flicker
- Fixed-step main loop at roughly 60 FPS with per-frame delta time
- Minimal graphics API: clear, pixel, rectangle, sprite with color-key transparency
- Driver layer behind function pointers (display, input, sound), so the core is not tied to one board
- Single build target via PlatformIO, one library dependency

## Hardware

The target is the M5Cardputer, built around the M5StampS3 (ESP32-S3FN8).

- MCU: ESP32-S3FN8
- Flash: 8 MB
- SRAM: about 512 KB
- PSRAM: none
- Display: 240x135, RGB565
- Keyboard: built in, no dedicated arrow keys
- Audio: built in speaker

There is no PSRAM, so the framebuffer lives in regular SRAM. At 240x135 and two bytes per pixel that is roughly 63 KB, which fits, but memory is tight overall and worth keeping in mind.

## Architecture

Everything is built around the `Engine` struct, which holds:

- three drivers: `DisplayDriver`, `InputDriver`, `SoundDriver`
- `graphicsInfo`, describing what and where graphics draws (buffer plus its dimensions)
- a pointer to the framebuffer
- the `is_running` flag
- function pointers for the engine lifecycle: `init`, `update`, `shutdown`, `start`

Startup happens in `main.cpp`:

```cpp
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
    InputDriver  id = create_builtin_input();
    SoundDriver  sd = create_builtin_sound();

    engine_init(&engine, dd, id, sd, framebuffer);
    engine.start(&engine);
}

void loop() {
}
```

Order matters here: `M5Cardputer.begin()` first, then the drivers, then `engine_init`, and only then `engine.start()`. The Arduino `loop()` is empty on purpose. The engine runs its own loop inside `start()` and does not return until `is_running` is set to false.

## Project layout

```
S32Engine/
  platformio.ini              build config (board, framework, dependencies)
  CMakeLists.txt
  sdkconfig.esp32s3box
  LICENSE                     GPL-3.0
  README.md
  include/
  lib/
  test/
  src/
    main.cpp                  entry point, wires up the engine
    CMakeLists.txt
    components/
      engine/
        engine.h              Engine struct and engine_init
        engine.cpp            main loop, init/update/shutdown/start
      graphics/
        graphics.h            grx_ declarations
        graphics.cpp          buffer drawing
      other/
        struct.h              Color, Point, Pixel, Sprite, graphicsInfo
      drivers/
        M5StackCardputerV01/
          display.h / display.cpp   pushes the buffer to the screen
          input.h   / input.cpp     reads the keyboard
          sound.h   / sound.cpp      speaker
```

Includes are resolved from the `src` root, for example `#include "components/graphics/graphics.h"`. This relies on the `-I src` flag in `platformio.ini`. Remove it and the includes break.

## Main loop

The core of the engine is `engine_start`. It is blocking and runs while `is_running` is true:

```cpp
static void engine_start(Engine* self) {
    self->is_running = true;
    int64_t prev = esp_timer_get_time();

    while (self->is_running) {
        grx_clear(&self->ggf, 0x0000);          // clear the frame

        int64_t now = esp_timer_get_time();
        int dt = (int)((now - prev) / 1000);    // delta time in ms
        prev = now;

        self->id.update(&self->id);             // poll the keyboard
        self->update(self, dt);                 // your game logic
        self->dd.flush(&self->dd, self->framebuffer);  // present the frame

        vTaskDelay(pdMS_TO_TICKS(16));          // about 60 frames per second
    }
}
```

Each frame:

1. Clear. The whole buffer is filled with black (0x0000).
2. Timing. `dt` is computed as milliseconds since the previous frame, so movement can be tied to time instead of frame count.
3. Input. The driver refreshes the keyboard state.
4. Update. `self->update` is called, which is where game logic goes.
5. Flush. The finished buffer is sent to the display in one call.
6. Yield. `vTaskDelay(16)` hands time back to FreeRTOS and holds the pace near 60 FPS.

`engine_init` fills the struct, binds the lifecycle functions, initializes all three drivers and sets up `graphicsInfo` (buffer, width and height come from the display):

```cpp
void engine_init(Engine* self, DisplayDriver dd, InputDriver id,
                 SoundDriver sd, uint16_t* framebuffer) {
    self->dd = dd;
    self->id = id;
    self->sd = sd;
    self->framebuffer = framebuffer;
    self->is_running = false;

    self->init     = engine_init;
    self->update   = engine_update;
    self->shutdown = engine_shutdown;
    self->start    = engine_start;

    self->dd.init(&self->dd);
    self->id.init(&self->id);
    self->sd.init(&self->sd);

    self->ggf.buffer = self->framebuffer;
    self->ggf.height = self->dd.height;
    self->ggf.width  = self->dd.width;
}
```

`engine_shutdown` sets `is_running` to false, and the loop exits on the next iteration.

## Graphics API

All drawing lives in `graphics.cpp` and works through `graphicsInfo` (buffer plus dimensions). The base types are in `other/struct.h`:

```cpp
typedef uint16_t Color;                          // RGB565 color
#define GRX_COLOR_TRANSPARENT ((Color)0xF81F)    // transparent (magenta)

struct Point   { uint16_t x, y; };
struct Pixel   { Color color; Point point; };
struct Sprite  { const Color* pixels; uint16_t width, height; };
struct graphicsInfo { Color* buffer; uint16_t width, height; };
```

Available functions:

- `grx_clear(g, color)` fills the whole buffer with one color
- `grx_setPixel(g, color, p)` sets a pixel with bounds checking
- `grx_setPixel(g, pixel)` same, using a `Pixel` struct
- `grx_getPixel(g, p)` reads a pixel color
- `grx_fillRect(g, color, p0, p1)` filled rectangle defined by two corners
- `grx_sprite(g, sprite, leftTop)` draws a sprite with color-key transparency

A few details worth knowing:

- Every function is bounds checked, so nothing writes outside the buffer.
- `grx_fillRect` figures out the top-left corner itself and clips to the screen, so the order of the two points does not matter.
- `grx_sprite` skips pixels of color 0xF81F, which acts as transparency, so sprites can be drawn over a background without a rectangular backing.
- Pixels are addressed as `buffer[y * width + x]`, a plain linear buffer.

## Drivers

Every driver is a struct of function pointers, created by a `create_builtin_*()` factory.

### Display (DisplayDriver)

```cpp
struct DisplayDriver {
    uint16_t width;
    uint16_t height;
    void (*init)(DisplayDriver* self);
    void (*flush)(DisplayDriver* self, const uint16_t* frame_buffer);
};
```

`init` reads the real screen size from `M5Cardputer.Display` (240x135), and `flush` pushes the whole buffer to the screen with a single `pushImage` call. `M5Cardputer.begin()` must be called in `main` before the driver is initialized.

### Input (InputDriver)

```cpp
struct InputState {
    bool up, down, left, right;
    bool action;    // Enter / Space
    bool quit;      // ` key
    char last_key;  // last typed character (0 = none)
};

struct InputDriver {
    InputState state;
    void (*init)(InputDriver* self);
    void (*update)(InputDriver* self);
};
```

Each frame `update` calls `M5Cardputer.update()`, reads the pressed keys and builds a fresh `InputState`. Besides the movement and action flags it keeps `last_key`, the last typed character, which is handy for text input and menus.

### Sound (SoundDriver)

```cpp
struct SoundDriver {
    void (*init)(SoundDriver* self);
    void (*play_wav_on_channel)(SoundDriver* self, uint8_t channel,
                                const uint8_t* wav_data, uint32_t wav_size);
    void (*stop_channel)(SoundDriver* self, uint8_t channel);
};
```

A thin wrapper over `M5Cardputer.Speaker`: init, play a WAV from memory on a channel, and stop a channel.

## Controls

The Cardputer has no dedicated arrow keys, so the cluster in the lower right of the keyboard is used instead:

- `;` up
- `.` down
- `,` left
- `/` right
- Enter or Space action
- `` ` `` quit

The keys `;` `.` `,` `/` sit in a cross shape, which makes them comfortable to play with.

## Building

The project builds with PlatformIO.

```ini
[env:cardputer]
platform = espressif32
board = m5stack-stamps3
framework = arduino
monitor_speed = 115200
build_flags =
    -O2
    -I src
lib_deps =
    m5stack/M5Cardputer
```

Commands:

```bash
pio run -e cardputer               # build
pio run -e cardputer -t upload      # build and flash
pio device monitor -b 115200        # serial monitor
```

Build notes:

- The board is `m5stack-stamps3`, not `esp32s3box`.
- The framework is `arduino`. M5Cardputer and LovyanGFX will not build under espidf.
- The `-I src` flag is required, otherwise includes like `components/...` are not found.
- There is a single dependency, `m5stack/M5Cardputer`, which PlatformIO pulls in automatically.

## Writing your first update

All of your logic goes in the engine update function. Right now `engine_update` is an empty stub:

```cpp
static void engine_update(Engine* self, int dt) {
    (void)self;
    (void)dt;
}
```

To draw something, work with `self->ggf` through the `grx_` functions and read input from `self->id.state`. For example, a movable square:

```cpp
static int x = 100, y = 60;

static void engine_update(Engine* self, int dt) {
    const InputState& in = self->id.state;

    if (in.left)  x -= 2;
    if (in.right) x += 2;
    if (in.up)    y -= 2;
    if (in.down)  y += 2;

    if (in.quit) self->shutdown(self);

    Point p0 { (uint16_t)x,        (uint16_t)y };
    Point p1 { (uint16_t)(x + 20), (uint16_t)(y + 20) };
    grx_fillRect(&self->ggf, 0xFFFF, p0, p1);
}
```

You do not need to clear the screen yourself, the engine already clears the buffer at the start of each frame.

## Status

Done:

- Framebuffer and main loop at 60 FPS with delta time
- Graphics: clear, pixels, pixel read, rectangles, sprites with transparency
- Display driver (buffer output through M5Cardputer.Display)
- Keyboard driver (arrows, action, quit, last key)
- Sound driver (WAV per channel through M5Cardputer.Speaker)
- Working PlatformIO build

Not there yet:

- `engine_update` is empty, this is a skeleton for a game, not a finished game
- No asset or sprite loading, everything is defined in code
- Sound is at a very basic level (play or stop a WAV)
- Graphics has no lines, circles or text yet, only what is listed above

## Roadmap

- Real game logic in `update` instead of the stub
- Sprite and asset loading (from SD card or flash)
- More graphics primitives: lines, circles, text
- A more complete sound layer

## License

GPL-3.0. The full text is in the LICENSE file at the repository root.
