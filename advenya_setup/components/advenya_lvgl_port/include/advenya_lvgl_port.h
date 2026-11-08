/*
 * ADVENYA OS · port de LVGL 9 para la placa JC4880P443C_I_W (ESP-IDF 5.5.5)
 *
 * Une la capa de hardware (advenya_board) con LVGL: pantalla en modo parcial
 * con dos buffers en RAM interna que se copian al framebuffer DPI por DMA2D,
 * indev tactil sobre el GT911, base de tiempo con esp_timer y una tarea propia
 * protegida por un mutex recursivo.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"
#include "advenya_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Crea display, indev y la tarea de LVGL. `disp` debe seguir vivo. */
esp_err_t adv_lvgl_port_init(adv_display_t *disp);

/** Bloquea LVGL para usarlo desde otra tarea. timeout_ms = 0 -> esperar siempre. */
bool adv_lvgl_lock(uint32_t timeout_ms);
void adv_lvgl_unlock(void);

lv_display_t *adv_lvgl_display(void);
lv_indev_t *adv_lvgl_indev(void);

/**
 * Llama a `cb` (desde la tarea de LVGL) cuando el primer fotograma completo ya
 * esta en el panel. Se usa para encender la retroiluminacion sin destello.
 */
void adv_lvgl_on_first_frame(void (*cb)(void *), void *arg);

/* ---- diagnostico ---- */
typedef struct {
    float    fps;              /* refrescos completados por segundo           */
    uint32_t render_avg_us;    /* tiempo medio de un refresco (render+flush)  */
    uint32_t render_max_us;
    uint32_t flush_wait_max_us;
    uint32_t flush_timeouts;
    uint32_t flush_errors;
    size_t   int_free, int_min_free, int_largest;
    size_t   psram_free, psram_min_free, psram_largest;
    uint32_t lvgl_stack_hwm;   /* minimo de pila libre de la tarea (bytes)    */
} adv_diag_t;

void adv_diag_get(adv_diag_t *out);

#ifdef __cplusplus
}
#endif
