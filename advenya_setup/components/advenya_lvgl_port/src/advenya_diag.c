/*
 * ADVENYA OS · diagnostico de rendimiento y memoria
 *
 * Cada CONFIG_ADV_DIAG_PERIOD_MS (desde un lv_timer, en la tarea de LVGL) se
 * publica por el log:
 *   fps         refrescos con contenido nuevo por segundo en la ventana
 *   render      tiempo medio / maximo de un refresco (render + ultimo flush)
 *   flush_wait  espera maxima del fin de copia DMA2D
 *   int / psram memoria libre, minimo historico y mayor bloque
 *   stack       pila libre minima de la tarea de LVGL
 * Con la interfaz quieta fps = 0 (LVGL no redibuja si nada cambia).
 */
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "lvgl.h"
#include "advenya_lvgl_port.h"
#include "advenya_lvgl_port_priv.h"

static const char *TAG = "adv_diag";

static adv_diag_t s_last;
static uint32_t s_prev_frames;
static uint64_t s_prev_sum;
static int64_t s_prev_t;

static void sample(void)
{
    adv_port_stats_t *st = &g_adv_port_stats;
    int64_t now = esp_timer_get_time();
    uint32_t frames = st->frames - s_prev_frames;
    uint64_t sum = st->render_sum_us - s_prev_sum;
    float secs = s_prev_t ? (float)(now - s_prev_t) / 1e6f : 0.f;
    s_last.fps = secs > 0 ? frames / secs : 0.f;
    s_last.render_avg_us = frames ? (uint32_t)(sum / frames) : 0;
    s_last.render_max_us = st->render_max_us;
    s_last.flush_wait_max_us = st->flush_wait_max_us;
    s_last.flush_timeouts = st->flush_timeouts;
    s_last.flush_errors = st->flush_errors;
    s_last.int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s_last.int_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    s_last.int_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    s_last.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s_last.psram_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    s_last.psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    TaskHandle_t t = adv_lvgl_task_handle();
    s_last.lvgl_stack_hwm = t ? (uint32_t)uxTaskGetStackHighWaterMark(t) : 0;
    s_prev_frames = st->frames;
    s_prev_sum = st->render_sum_us;
    s_prev_t = now;
    st->render_max_us = 0;            /* maximos por ventana */
    st->flush_wait_max_us = 0;
}

#if CONFIG_ADV_DIAG_ENABLE
static void diag_timer_cb(lv_timer_t *t)
{
    sample();
    ESP_LOGI(TAG, "fps=%.1f render=%lu/%lu us flush_wait_max=%lu us (timeouts=%lu err=%lu) | "
             "int %u KB libres (min %u, bloque %u) | psram %u KB libres (min %u, bloque %u) | pila lvgl %lu B",
             s_last.fps, (unsigned long)s_last.render_avg_us, (unsigned long)s_last.render_max_us,
             (unsigned long)s_last.flush_wait_max_us, (unsigned long)s_last.flush_timeouts,
             (unsigned long)s_last.flush_errors,
             (unsigned)(s_last.int_free / 1024), (unsigned)(s_last.int_min_free / 1024),
             (unsigned)(s_last.int_largest / 1024), (unsigned)(s_last.psram_free / 1024),
             (unsigned)(s_last.psram_min_free / 1024), (unsigned)(s_last.psram_largest / 1024),
             (unsigned long)s_last.lvgl_stack_hwm);
}
#endif

void adv_diag_start(void)
{
    s_prev_t = esp_timer_get_time();
#if CONFIG_ADV_DIAG_ENABLE
    lv_timer_create(diag_timer_cb, CONFIG_ADV_DIAG_PERIOD_MS, NULL);
#endif
}

void adv_diag_get(adv_diag_t *out)
{
    if (out) {
        *out = s_last;
    }
}
