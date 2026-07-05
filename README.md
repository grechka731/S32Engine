<img width="537" height="399" alt="avocard" src="https://github.com/user-attachments/assets/c1553bbd-e334-4b78-aa43-8a583718e31e" />

# S32Engine

> Небольшой самописный движок для **M5Cardputer**. Ты рисуешь картинку в свой
> буфер пикселей, а движок сам крутит игровой цикл и раз в кадр выливает этот
> буфер на экран. Никаких сцен, ECS и менеджеров ресурсов — только то, что
> реально нужно, чтобы что-то показать на экранчике Cardputer'а.

Это пет-проект под конкретную железку, а не универсальный фреймворк. Ядро
сознательно держится маленьким: пара структур, главный цикл и тонкие драйверы
поверх библиотеки M5Cardputer. Хочешь больше — дописываешь под себя.

---

## Содержание

- [Что это такое](#что-это-такое)
- [Железо](#железо)
- [Как это устроено](#как-это-устроено)
- [Структура проекта](#структура-проекта)
- [Игровой цикл](#игровой-цикл)
- [Графика](#графика)
- [Драйверы](#драйверы)
- [Управление](#управление)
- [Сборка и прошивка](#сборка-и-прошивка)
- [С чего начать свою игру](#с-чего-начать-свою-игру)
- [Что уже работает и что нет](#что-уже-работает-и-что-нет)
- [Планы](#планы)
- [Лицензия](#лицензия)

---

## Что это такое

S32Engine — это framebuffer-движок. Идея простая:

1. Есть один массив `uint16_t` на весь экран — это и есть кадр (формат цвета RGB565).
2. Ты рисуешь в этот массив чем угодно: пиксели, прямоугольники, спрайты.
3. Движок раз в кадр отдаёт массив дисплею целиком.

Вся отрисовка идёт в память, а не напрямую в экран — поэтому нет мерцания и
можно спокойно перерисовывать всё с нуля каждый кадр.

Архитектура построена на **структурах с указателями на функции**. То есть
дисплей, ввод и звук — это не жёстко зашитые вызовы M5Cardputer, а интерфейсы.
Сейчас реализация одна (под Cardputer), но при желании ядро можно перенести на
другую железку, просто подсунув свои драйверы и не трогая движок.

---

## Железо

Цель — **M5Cardputer**. Внутри у него **M5StampS3** на чипе **ESP32-S3FN8**:

| Параметр | Значение |
|-----------------|-----------------------------------|
| Чип | ESP32-S3FN8 |
| Flash | 8 МБ |
| SRAM | ~512 КБ |
| PSRAM | нет |
| Экран | 240×135, цвет RGB565 |
| Клавиатура | встроенная, без отдельных стрелок |
| Звук | встроенный динамик |

Отдельно отмечу про PSRAM: его нет, так что весь фреймбуфер живёт в обычной
SRAM. 240×135×2 байта ≈ 63 КБ — влезает, но памяти в целом немного, это стоит
держать в голове.

---

## Как это устроено

Всё завязано вокруг структуры `Engine`. В ней лежат:

- три драйвера — `DisplayDriver`, `InputDriver`, `SoundDriver`;
- `graphicsInfo` — то, во что и куда рисует графика (буфер + его размеры);
- указатель на сам фреймбуфер;
- флаг `is_running`;
- указатели на функции движка: `init`, `update`, `shutdown`, `start`.

Стартует всё из `main.cpp`:

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

Порядок важный: сначала `M5Cardputer.begin()`, потом создание драйверов, потом
`engine_init` и только затем `engine.start()`. `loop()` из Arduino намеренно
пустой — движок крутит свой цикл внутри `start()` и наружу не возвращается,
пока не выставят `is_running = false`.

---

## Структура проекта

```
S32Engine/
├── platformio.ini              конфиг сборки (плата, фреймворк, зависимости)
├── CMakeLists.txt
├── sdkconfig.esp32s3box
├── LICENSE                     GPL-3.0
├── README.md
├── include/
├── lib/
├── test/
└── src/
    ├── main.cpp                точка входа, тут заводится движок
    ├── CMakeLists.txt
    └── components/
        ├── engine/
        │   ├── engine.h        структура Engine + engine_init
        │   └── engine.cpp      главный цикл, init/update/shutdown/start
        ├── graphics/
        │   ├── graphics.h      объявления grx_*
        │   └── graphics.cpp    рисование по буферу
        ├── other/
        │   └── struct.h        Color, Point, Pixel, Sprite, graphicsInfo
        └── drivers/
            └── M5StackCardputerV01/
                ├── display.h / display.cpp   вывод буфера на экран
                ├── input.h   / input.cpp     чтение клавиатуры
                └── sound.h   / sound.cpp      динамик
```

Включения идут от корня `src`, например `#include "components/graphics/graphics.h"`.
Это держится флагом `-I src` в `platformio.ini` — если его убрать, инклюды
отвалятся.

---

## Игровой цикл

Сердце движка — функция `engine_start`. Она блокирующая и работает, пока
`is_running == true`:

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

        vTaskDelay(pdMS_TO_TICKS(16));          // ~60 кадров в секунду
    }
}
```

Что происходит каждый кадр:

1. **Очистка** — весь буфер заливается чёрным (`0x0000`).
2. **Замер времени** — считается `dt` (сколько миллисекунд прошло с прошлого кадра), чтобы движение можно было привязать ко времени, а не к номеру кадра.
3. **Ввод** — драйвер обновляет состояние клавиатуры.
4. **Апдейт** — вызывается `self->update`, куда ты кладёшь логику игры.
5. **Flush** — готовый буфер целиком отправляется на дисплей.
6. **Пауза** `vTaskDelay(16)` — уступает время FreeRTOS и держит темп около 60 FPS.

`engine_init` заполняет структуру, привязывает функции движка, инициализирует
все три драйвера и настраивает `graphicsInfo` (буфер + ширина/высота берутся
из дисплея):

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

`engine_shutdown` просто ставит `is_running = false`, и цикл на следующей
итерации выходит.

---

## Графика

Вся отрисовка живёт в `graphics.cpp` и работает через `graphicsInfo` (буфер +
его размеры). Базовые типы описаны в `other/struct.h`:

```cpp
typedef uint16_t Color;                          // цвет RGB565
#define GRX_COLOR_TRANSPARENT ((Color)0xF81F)    // прозрачный (ядовитая магента)

struct Point   { uint16_t x, y; };
struct Pixel   { Color color; Point point; };
struct Sprite  { const Color* pixels; uint16_t width, height; };
struct graphicsInfo { Color* buffer; uint16_t width, height; };
```

Набор функций:

| Функция | Что делает |
|--------------------------------------------------|--------------------------------------------------|
| `grx_clear(g, color)` | залить весь буфер одним цветом |
| `grx_setPixel(g, color, p)` | поставить пиксель (с проверкой границ) |
| `grx_setPixel(g, pixel)` | то же, но через структуру `Pixel` |
| `grx_getPixel(g, p)` | прочитать цвет пикселя |
| `grx_fillRect(g, color, p0, p1)` | закрашенный прямоугольник по двум углам |
| `grx_sprite(g, sprite, leftTop)` | нарисовать спрайт с учётом прозрачности |

Несколько важных деталей:

- Все функции **проверяют границы** — за пределы буфера ничего не вылезет.
- `grx_fillRect` сам разбирается, какой угол левый-верхний, и обрезает
  прямоугольник по экрану, так что порядок точек не важен.
- `grx_sprite` пропускает пиксели цвета `0xF81F` — это и есть «прозрачность».
  То есть спрайты можно рисовать поверх фона без прямоугольной подложки.
- Пиксель адресуется как `buffer[y * width + x]` — обычный линейный буфер.

---

## Драйверы

Все драйверы — это структуры с указателями на функции. Каждый создаётся своей
фабрикой `create_builtin_*()`.

### Дисплей (`DisplayDriver`)

```cpp
struct DisplayDriver {
    uint16_t width;
    uint16_t height;
    void (*init)(DisplayDriver* self);
    void (*flush)(DisplayDriver* self, const uint16_t* frame_buffer);
};
```

`init` берёт реальные размеры экрана у `M5Cardputer.Display` (240×135), а
`flush` выливает весь буфер на экран одним вызовом `pushImage`. Обрати
внимание: `M5Cardputer.begin()` должен быть вызван в `main` **до** инициализации
драйвера.

### Ввод (`InputDriver`)

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

Каждый кадр `update` дёргает `M5Cardputer.update()`, читает нажатые клавиши и
собирает свежий `InputState`. Помимо разложенных по флагам стрелок/действий
сохраняется и `last_key` — последний введённый символ, удобно для текста и меню.

### Звук (`SoundDriver`)

```cpp
struct SoundDriver {
    void (*init)(SoundDriver* self);
    void (*play_wav_on_channel)(SoundDriver* self, uint8_t channel,
                                const uint8_t* wav_data, uint32_t wav_size);
    void (*stop_channel)(SoundDriver* self, uint8_t channel);
};
```

Тонкая обёртка над `M5Cardputer.Speaker`: инициализация, проигрывание WAV из
памяти по каналу и остановка канала.

---

## Управление

У Cardputer нет отдельных стрелок, поэтому под них отданы соседние клавиши в
правом нижнем углу клавиатуры:

| Клавиша | Действие |
|--------------|--------------|
| `;` | вверх |
| `.` | вниз |
| `,` | влево |
| `/` | вправо |
| `Enter` / `Space` | действие |
| `` ` `` | выход |

Раскладка «стрелок» `; . , /` расположена крестиком, так что играть вполне
удобно.

---

## Сборка и прошивка

Проект собирается через **PlatformIO**. Конфиг:

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

На что обратить внимание:

- Плата — именно `m5stack-stamps3`. Не `esp32s3box`.
- Фреймворк — `arduino`. На `espidf` M5Cardputer/LovyanGFX не соберутся.
- Флаг `-I src` обязателен, иначе инклюды вида `components/...` не найдутся.
- Зависимость всего одна — `m5stack/M5Cardputer`, PlatformIO подтянет её сам.

---

## С чего начать свою игру

Вся твоя логика живёт в функции апдейта движка. Сейчас `engine_update` —
пустая заглушка:

```cpp
static void engine_update(Engine* self, int dt) {
    (void)self;
    (void)dt;
}
```

Чтобы что-то нарисовать, работай с `self->ggf` через функции `grx_*`, а ввод
бери из `self->id.state`. Например, движущийся квадратик:

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

Очистку экрана делать не надо — движок уже чистит буфер в начале каждого кадра.

---

## Что уже работает и что нет

**Готово:**

- Фреймбуфер и главный цикл на ~60 FPS с подсчётом `dt`.
- Графика: заливка, пиксели, чтение пикселя, прямоугольники, спрайты с прозрачностью.
- Драйвер дисплея (вывод буфера через M5Cardputer.Display).
- Драйвер клавиатуры (стрелки, действие, выход, последний символ).
- Драйвер звука (WAV по каналам через M5Cardputer.Speaker).
- Настроенная сборка под PlatformIO.

**Пока нет / заглушки:**

- `engine_update` пустой — это каркас под игру, а не готовая игрушка.
- Нет загрузки ассетов/спрайтов, всё пока задаётся в коде.
- Звук на самом базовом уровне (проиграть/остановить WAV).
- В графике нет линий, окружностей, текста — только то, что перечислено выше.

---

## Планы

- Реальная игровая логика в `update` вместо заглушки.
- Загрузка спрайтов и ассетов (с SD-карты или из флеша).
- Больше примитивов в графике: линии, окружности, вывод текста.
- Доработать звук.

---

## Лицензия

GPL-3.0. Полный текст — в файле `LICENSE` в корне репозитория.
