# S32Engine

![platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![board](https://img.shields.io/badge/board-M5Cardputer-orange)
![framework](https://img.shields.io/badge/framework-Arduino-00979D)
![license](https://img.shields.io/badge/license-GPL--3.0-green)

Маленький framebuffer-движок для M5Cardputer (M5StampS3 / ESP32-S3).

Ты рисуешь в единый буфер пикселей, а движок крутит главный цикл, опрашивает ввод и раз в кадр выкидывает этот буфер на экран. Никаких сцен, ECS и менеджеров ресурсов. Только то, что нужно чтобы пиксели задвигались на экране Cardputer и можно было собрать поверх небольшую игру.

[English version](README.md)

## Возможности

- Программный фреймбуфер (RGB565), без прямого вывода в экран и без мерцания
- Фиксированный главный цикл около 60 FPS с покадровым расчётом dt
- Минимальный графический API: очистка, пиксель, прямоугольник, спрайт с прозрачностью по цвету
- Слой драйверов на указателях на функции (дисплей, ввод, звук), ядро не привязано к одной плате
- Один таргет сборки через PlatformIO, одна зависимость

## Железо

Цель это M5Cardputer на базе M5StampS3 (ESP32-S3FN8).

- Микроконтроллер: ESP32-S3FN8
- Flash: 8 МБ
- SRAM: около 512 КБ
- PSRAM: нет
- Экран: 240x135, RGB565
- Клавиатура: встроенная, без отдельных стрелок
- Звук: встроенный динамик

PSRAM нет, поэтому фреймбуфер живёт в обычной SRAM. 240x135 по 2 байта на пиксель это примерно 63 КБ. Влезает, но памяти в целом немного, это стоит держать в голове.

## Архитектура

Всё строится вокруг структуры `Engine`, в которой лежат:

- три драйвера: `DisplayDriver`, `InputDriver`, `SoundDriver`
- `graphicsInfo`, описывающий во что и куда рисует графика (буфер плюс его размеры)
- указатель на фреймбуфер
- флаг `is_running`
- указатели на функции жизненного цикла: `init`, `update`, `shutdown`, `start`

Старт происходит в `main.cpp`:

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

Порядок важен: сначала `M5Cardputer.begin()`, потом драйверы, потом `engine_init` и только затем `engine.start()`. Arduino-шный `loop()` пустой намеренно: движок крутит свой цикл внутри `start()` и не возвращается, пока `is_running` не станет false.

## Структура проекта

```
S32Engine/
  platformio.ini              конфиг сборки (плата, фреймворк, зависимости)
  CMakeLists.txt
  sdkconfig.esp32s3box
  LICENSE                     GPL-3.0
  README.md
  include/
  lib/
  test/
  src/
    main.cpp                  точка входа, заводит движок
    CMakeLists.txt
    components/
      engine/
        engine.h              структура Engine и engine_init
        engine.cpp            главный цикл, init/update/shutdown/start
      graphics/
        graphics.h            объявления grx_
        graphics.cpp          рисование по буферу
      other/
        struct.h              Color, Point, Pixel, Sprite, graphicsInfo
      drivers/
        M5StackCardputerV01/
          display.h / display.cpp   вывод буфера на экран
          input.h   / input.cpp     чтение клавиатуры
          sound.h   / sound.cpp      динамик
```

Включения разрешаются от корня `src`, например `#include "components/graphics/graphics.h"`. Это держится флагом `-I src` в `platformio.ini`. Уберёшь его, инклюды отвалятся.

## Главный цикл

Ядро движка это `engine_start`. Функция блокирующая и работает, пока `is_running` равно true:

```cpp
static void engine_start(Engine* self) {
    self->is_running = true;
    int64_t prev = esp_timer_get_time();

    while (self->is_running) {
        grx_clear(&self->ggf, 0x0000);          // очистить кадр

        int64_t now = esp_timer_get_time();
        int dt = (int)((now - prev) / 1000);    // дельта времени в мс
        prev = now;

        self->id.update(&self->id);             // опросить клавиатуру
        self->update(self, dt);                 // твоя игровая логика
        self->dd.flush(&self->dd, self->framebuffer);  // показать кадр

        vTaskDelay(pdMS_TO_TICKS(16));          // около 60 кадров в секунду
    }
}
```

Каждый кадр:

1. Очистка. Весь буфер заливается чёрным (0x0000).
2. Время. Считается dt, сколько миллисекунд прошло с прошлого кадра, чтобы движение можно было привязать ко времени, а не к номеру кадра.
3. Ввод. Драйвер обновляет состояние клавиатуры.
4. Апдейт. Вызывается `self->update`, куда ты кладёшь логику игры.
5. Flush. Готовый буфер целиком отправляется на дисплей.
6. Пауза. `vTaskDelay(16)` уступает время FreeRTOS и держит темп около 60 FPS.

`engine_init` заполняет структуру, привязывает функции жизненного цикла, инициализирует все три драйвера и настраивает `graphicsInfo` (буфер, ширина и высота берутся из дисплея):

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

`engine_shutdown` ставит `is_running` в false, и цикл на следующей итерации выходит.

## Графический API

Вся отрисовка живёт в `graphics.cpp` и работает через `graphicsInfo` (буфер плюс размеры). Базовые типы в `other/struct.h`:

```cpp
typedef uint16_t Color;                          // цвет RGB565
#define GRX_COLOR_TRANSPARENT ((Color)0xF81F)    // прозрачный (магента)

struct Point   { uint16_t x, y; };
struct Pixel   { Color color; Point point; };
struct Sprite  { const Color* pixels; uint16_t width, height; };
struct graphicsInfo { Color* buffer; uint16_t width, height; };
```

Функции:

- `grx_clear(g, color)` залить весь буфер одним цветом
- `grx_setPixel(g, color, p)` поставить пиксель с проверкой границ
- `grx_setPixel(g, pixel)` то же через структуру Pixel
- `grx_getPixel(g, p)` прочитать цвет пикселя
- `grx_fillRect(g, color, p0, p1)` закрашенный прямоугольник по двум углам
- `grx_sprite(g, sprite, leftTop)` нарисовать спрайт с учётом прозрачности

Полезные детали:

- Все функции проверяют границы, за пределы буфера ничего не вылезет.
- `grx_fillRect` сам определяет левый верхний угол и обрезает прямоугольник по экрану, порядок точек не важен.
- `grx_sprite` пропускает пиксели цвета 0xF81F, это и есть прозрачность. Спрайты можно рисовать поверх фона без прямоугольной подложки.
- Пиксель адресуется как `buffer[y * width + x]`, обычный линейный буфер.

## Драйверы

Каждый драйвер это структура с указателями на функции, создаётся фабрикой `create_builtin_*()`.

### Дисплей (DisplayDriver)

```cpp
struct DisplayDriver {
    uint16_t width;
    uint16_t height;
    void (*init)(DisplayDriver* self);
    void (*flush)(DisplayDriver* self, const uint16_t* frame_buffer);
};
```

`init` берёт реальный размер экрана у `M5Cardputer.Display` (240x135), а `flush` выливает весь буфер на экран одним вызовом `pushImage`. `M5Cardputer.begin()` должен быть вызван в `main` до инициализации драйвера.

### Ввод (InputDriver)

```cpp
struct InputState {
    bool up, down, left, right;
    bool action;    // Enter / Space
    bool quit;      // клавиша `
    char last_key;  // последний нажатый символ (0 = ничего)
};

struct InputDriver {
    InputState state;
    void (*init)(InputDriver* self);
    void (*update)(InputDriver* self);
};
```

Каждый кадр `update` дёргает `M5Cardputer.update()`, читает нажатые клавиши и собирает свежий `InputState`. Помимо флагов стрелок и действия сохраняется `last_key`, последний введённый символ, удобно для текста и меню.

### Звук (SoundDriver)

```cpp
struct SoundDriver {
    void (*init)(SoundDriver* self);
    void (*play_wav_on_channel)(SoundDriver* self, uint8_t channel,
                                const uint8_t* wav_data, uint32_t wav_size);
    void (*stop_channel)(SoundDriver* self, uint8_t channel);
};
```

Тонкая обёртка над `M5Cardputer.Speaker`: инициализация, проигрывание WAV из памяти по каналу и остановка канала.

## Управление

У Cardputer нет отдельных стрелок, поэтому вместо них используется кластер клавиш в правом нижнем углу:

- `;` вверх
- `.` вниз
- `,` влево
- `/` вправо
- Enter или Space действие
- `` ` `` выход

Клавиши `;` `.` `,` `/` расположены крестиком, играть вполне удобно.

## Сборка

Проект собирается через PlatformIO.

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

Команды:

```bash
pio run -e cardputer               # собрать
pio run -e cardputer -t upload      # собрать и прошить
pio device monitor -b 115200        # монитор порта
```

Моменты по сборке:

- Плата `m5stack-stamps3`, не `esp32s3box`.
- Фреймворк `arduino`. На espidf M5Cardputer и LovyanGFX не соберутся.
- Флаг `-I src` обязателен, иначе инклюды вида `components/...` не найдутся.
- Зависимость одна, `m5stack/M5Cardputer`, PlatformIO подтянет её сам.

## Первый update

Вся логика игры живёт в функции апдейта. Сейчас `engine_update` это пустая заглушка:

```cpp
static void engine_update(Engine* self, int dt) {
    (void)self;
    (void)dt;
}
```

Чтобы что-то нарисовать, работай с `self->ggf` через функции `grx_`, а ввод бери из `self->id.state`. Например, двигающийся квадратик:

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

Очищать экран самому не нужно, движок чистит буфер в начале каждого кадра.

## Статус

Готово:

- Фреймбуфер и главный цикл на 60 FPS с dt
- Графика: заливка, пиксели, чтение пикселя, прямоугольники, спрайты с прозрачностью
- Драйвер дисплея (вывод буфера через M5Cardputer.Display)
- Драйвер клавиатуры (стрелки, действие, выход, последний символ)
- Драйвер звука (WAV по каналам через M5Cardputer.Speaker)
- Рабочая сборка PlatformIO

Пока нет:

- `engine_update` пустой, это каркас под игру, а не готовая игрушка
- Нет загрузки ассетов и спрайтов, всё задаётся в коде
- Звук на базовом уровне (проиграть или остановить WAV)
- В графике нет линий, окружностей и текста

## Планы

- Реальная игровая логика в `update` вместо заглушки
- Загрузка спрайтов и ассетов (с SD-карты или из флеша)
- Больше примитивов: линии, окружности, текст
- Более полный звуковой слой

## Лицензия

GPL-3.0. Полный текст в файле LICENSE в корне репозитория.
