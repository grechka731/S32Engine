#include "components/engine/engine.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void engine_update(Engine* self, int dt) {
    (void)self;
    (void)dt;
}

static void engine_shutdown(Engine* self) {
    self->is_running = false;
}

static void engine_start(Engine* self) {
    self->is_running = true;
    int64_t prev = esp_timer_get_time();  

    while (self->is_running) {
        grx_clear(&self->ggf, 0x0000);

        int64_t now = esp_timer_get_time();
        int dt = (int)((now - prev) / 1000);
        prev = now;

        self->id.update(&self->id);                    
        self->update(self, dt);                        
        self->dd.flush(&self->dd, self->framebuffer);  

        vTaskDelay(pdMS_TO_TICKS(16));
    }   
}

void engine_init(Engine* self, DisplayDriver dd, InputDriver id, SoundDriver sd,
                 uint16_t* framebuffer) {
    //drivers
    self->dd = dd;
    self->id = id;
    self->sd = sd;
    self->framebuffer = framebuffer;
    self->is_running  = false;

    self->init     = engine_init;
    self->update   = engine_update;
    self->shutdown = engine_shutdown;
    self->start    = engine_start;

    self->dd.init(&self->dd);
    self->id.init(&self->id);
    self->sd.init(&self->sd);


    //grx
    self->ggf.buffer=self->framebuffer;
    self->ggf.height=self->dd.height;
    self->ggf.width=self->dd.width;
}


