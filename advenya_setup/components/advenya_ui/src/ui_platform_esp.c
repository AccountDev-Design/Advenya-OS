/*
 * ADVENYA OS · servicios de plataforma de la interfaz sobre ESP-IDF
 *
 * - Memoria grande en PSRAM alineada a 64 B (linea de cache).
 * - Trabajo pesado (horneado del fondo) en una tarea "adv_work" de baja
 *   prioridad en el nucleo 0; el resultado vuelve a la tarea de LVGL con
 *   lv_async_call() bajo el bloqueo de LVGL.
 */
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "ui_platform.h"
#include "advenya_lvgl_port.h"

static const char *TAG = "adv_ui";

typedef struct {
    void (*work)(void *);
    void (*done)(void *);
    void *arg;
} job_t;

static QueueHandle_t s_q;

void *ui_platform_alloc_large(size_t bytes)
{
    void *p = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_8BIT);
    }
    return p;
}

void ui_platform_free_large(void *p)
{
    heap_caps_free(p);
}

static void done_trampoline(void *arg)
{
    job_t *j = arg;
    j->done(j->arg);
    free(j);
}

static void worker(void *unused)
{
    job_t *j;
    for (;;) {
        if (xQueueReceive(s_q, &j, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        int64_t t0 = esp_timer_get_time();
        j->work(j->arg);
        ESP_LOGD(TAG, "trabajo en segundo plano: %lld us", (long long)(esp_timer_get_time() - t0));
        adv_lvgl_lock(0);
        lv_async_call(done_trampoline, j);
        adv_lvgl_unlock();
    }
}

bool ui_platform_run_async(void (*work)(void *), void (*done)(void *), void *arg)
{
    if (!s_q) {
        s_q = xQueueCreate(4, sizeof(job_t *));
        if (!s_q || xTaskCreatePinnedToCore(worker, "adv_work", 4096, NULL, 2, NULL, 0) != pdPASS) {
            return false;
        }
    }
    job_t *j = malloc(sizeof(*j));
    if (!j) {
        return false;
    }
    j->work = work;
    j->done = done;
    j->arg = arg;
    if (xQueueSend(s_q, &j, 0) != pdTRUE) {
        free(j);
        return false;
    }
    return true;
}

int ui_platform_is_night(void)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 < 2025) {
        return -1;          /* reloj sin ajustar: hora desconocida */
    }
    return (tm.tm_hour < 7 || tm.tm_hour >= 20) ? 1 : 0;
}

void ui_platform_log_mem(const char *where)
{
    ESP_LOGI(TAG, "[mem] %s: interna %u B libres (min %u), PSRAM %u B libres (min %u)", where,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}
