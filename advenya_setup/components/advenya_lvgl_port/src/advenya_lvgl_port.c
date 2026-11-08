/*
 * ADVENYA OS · port de LVGL 9.2 sobre la capa advenya_board
 *
 * Contrato de presentacion (el mismo que flxFlush() de Flex OS Ultra, ahora con
 * dos buffers de LVGL en lugar de un framebuffer propio):
 *   flush_cb       -> esp_lcd_panel_draw_bitmap(): DMA2D copia el buffer de
 *                     LVGL al framebuffer DPI y la funcion vuelve en seguida.
 *   ISR (IRAM)     -> on_color_trans_done: libera un semaforo binario.
 *   flush_wait_cb  -> LVGL duerme en ese semaforo antes de reutilizar el
 *                     buffer; espera acotada (CONFIG_ADV_LVGL_FLUSH_TIMEOUT_MS),
 *                     nunca infinita.
 * Un unico propietario del pipeline: la tarea de LVGL.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "sdkconfig.h"
#include "advenya_lvgl_port.h"
#include "advenya_lvgl_port_priv.h"

/* El panel copia los buffers de LVGL con esp_async_fbcpy (DMA2D). En ESP-IDF
 * 5.5.5 su callback on_job_picked esta en flash, y con DMA2D_ISR_IRAM_SAFE
 * dma2d_enqueue() lo rechaza: ninguna copia llegaria al panel. */
#if CONFIG_DMA2D_ISR_IRAM_SAFE
#error "CONFIG_DMA2D_ISR_IRAM_SAFE es incompatible con la copia DMA2D de esp_lcd (esp_async_fbcpy) en ESP-IDF 5.5.5. Borra 'sdkconfig' y vuelve a ejecutar 'idf.py set-target esp32p4' (o desactivala en menuconfig: Component config > Hardware Settings > 2D-DMA)."
#endif

static const char *TAG = "adv_lvgl";

#define TOUCH_STALE_US   (150 * 1000)   /* sin frames del GT911 -> se da por soltado */
#define INDEV_PERIOD_MS  16

static adv_display_t *s_hw;
static lv_display_t *s_disp;
static lv_indev_t *s_indev;
static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_flush_sem;
static TaskHandle_t s_task;
static void *s_buf[2];

static void (*s_first_cb)(void *);
static void *s_first_arg;
static bool s_first_done;

adv_port_stats_t g_adv_port_stats;

/* ------------------------------------------------------------- pantalla */

static bool IRAM_ATTR on_trans_done(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *ctx)
{
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)ctx, &hp);
    return hp == pdTRUE;
}

/*
 * Estado de la copia DMA2D en curso. Solo se toca con el bloqueo de LVGL
 * tomado (flush_cb / flush_wait_cb corren en la tarea de LVGL;
 * adv_lvgl_flash_guard_begin() toma el bloqueo); la ISR solo da el semaforo.
 * Asi el unico token del semaforo binario siempre lo consume quien sabe que
 * hay una copia pendiente, aunque una espera haya vencido.
 */
static bool s_inflight;

/* Espera el fin de la copia pendiente (si la hay). true si termino. */
static bool wait_inflight(uint32_t timeout_ms)
{
    if (!s_inflight) {
        return true;
    }
    int64_t t0 = esp_timer_get_time();
    bool done = xSemaphoreTake(s_flush_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    uint32_t dt = (uint32_t)(esp_timer_get_time() - t0);
    if (dt > g_adv_port_stats.flush_wait_max_us) {
        g_adv_port_stats.flush_wait_max_us = dt;
    }
    if (done) {
        s_inflight = false;
    }
    return done;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    /* Si una espera anterior vencio, la DMA2D sigue ocupada: esp_lcd
     * rechazaria la nueva copia (ESP_ERR_INVALID_STATE). Se espera de nuevo
     * en lugar de perder la banda. */
    if (!wait_inflight(CONFIG_ADV_LVGL_FLUSH_TIMEOUT_MS)) {
        g_adv_port_stats.flush_timeouts++;
        g_adv_port_stats.flush_errors++;
        return;                                   /* banda descartada, sin bloqueo */
    }
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_hw->panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px);
    if (err == ESP_OK) {
        s_inflight = true;
    } else {
        g_adv_port_stats.flush_errors++;
        ESP_LOGE(TAG, "draw_bitmap (%d,%d)-(%d,%d): %s", (int)area->x1, (int)area->y1, (int)area->x2,
                 (int)area->y2, esp_err_to_name(err));
    }
}

static void flush_wait_cb(lv_display_t *disp)
{
    if (!wait_inflight(CONFIG_ADV_LVGL_FLUSH_TIMEOUT_MS)) {
        g_adv_port_stats.flush_timeouts++;
    }
}

/*
 * Guardia de flash. Mientras se borra o escribe la flash la cache esta
 * apagada; la ISR de la DMA2D de esp_lcd NO es cache-safe en IDF 5.5.5 (ver
 * sdkconfig.defaults), asi que su "fin de copia" se retrasaria todo el borrado
 * y LVGL podria reutilizar un buffer que la DMA2D aun lee. Antes de tocar la
 * flash se toma el bloqueo de LVGL (no empieza ningun render ni flush) y se
 * espera a que termine la copia en curso. Durante el borrado el codigo de
 * LVGL (en flash) quedaria parado igualmente, asi que no se pierde nada. El
 * refresco del panel continua: la ISR del puente DSI si es cache-safe.
 */
void adv_lvgl_flash_guard_begin(void)
{
    adv_lvgl_lock(0);
    if (!wait_inflight(CONFIG_ADV_LVGL_FLUSH_TIMEOUT_MS)) {
        g_adv_port_stats.flush_timeouts++;
    }
}

void adv_lvgl_flash_guard_end(void)
{
    adv_lvgl_unlock();
}

static void refr_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RENDER_START) {
        if (!g_adv_port_stats.in_frame) {
            g_adv_port_stats.in_frame = true;
            g_adv_port_stats.frame_t0_us = esp_timer_get_time();
        }
    } else if (code == LV_EVENT_REFR_READY && g_adv_port_stats.in_frame) {
        g_adv_port_stats.in_frame = false;
        uint32_t dt = (uint32_t)(esp_timer_get_time() - g_adv_port_stats.frame_t0_us);
        g_adv_port_stats.frames++;
        g_adv_port_stats.render_sum_us += dt;
        if (dt > g_adv_port_stats.render_max_us) {
            g_adv_port_stats.render_max_us = dt;
        }
        if (!s_first_done) {
            s_first_done = true;
            if (s_first_cb) {
                s_first_cb(s_first_arg);
            }
        }
    }
}

/* --------------------------------------------------------------- tactil */

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static uint16_t lx, ly;
    static bool pressed;
    static int64_t last_us;
    uint16_t x, y;
    int64_t now = esp_timer_get_time();
    switch (adv_board_touch_read(&x, &y)) {
    case ADV_TOUCH_PRESSED:
        lx = x;
        ly = y;
        pressed = true;
        last_us = now;
        break;
    case ADV_TOUCH_RELEASED:
        pressed = false;
        break;
    default:
        /* Sin frame nuevo: se conserva el estado, salvo que el GT911 lleve
         * demasiado sin informar (frame de "soltar" perdido). */
        if (pressed && now - last_us > TOUCH_STALE_US) {
            pressed = false;
        }
        break;
    }
    data->point.x = lx;
    data->point.y = ly;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* ---------------------------------------------------------------- tarea */

static uint32_t tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "tarea de LVGL en el nucleo %d", xPortGetCoreID());
    for (;;) {
        uint32_t wait = 5;
        if (adv_lvgl_lock(0)) {
            wait = lv_timer_handler();
            adv_lvgl_unlock();
        }
        if (wait < 2) {
            wait = 2;
        } else if (wait > 33) {
            wait = 33;
        }
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

TaskHandle_t adv_lvgl_task_handle(void)
{
    return s_task;
}

esp_err_t adv_lvgl_port_init(adv_display_t *hw)
{
    ESP_RETURN_ON_FALSE(hw && hw->panel, ESP_ERR_INVALID_ARG, TAG, "display no inicializado");
    s_hw = hw;
    lv_init();
    lv_tick_set_cb(tick_cb);

    s_lock = xSemaphoreCreateRecursiveMutex();
    /* El contexto del ISR debe estar en RAM interna (requisito de CACHE_SAFE) */
    s_flush_sem = xSemaphoreCreateBinaryWithCaps(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_lock && s_flush_sem, ESP_ERR_NO_MEM, TAG, "sin memoria para semaforos");

    /* Dos buffers parciales en RAM interna, alineados a linea de cache (64 B):
     * el render por software es mas rapido en SRAM y DMA2D los lee tras el
     * write-back que hace esp_lcd. Si no caben, se recurre a PSRAM. */
    const size_t lines = CONFIG_ADV_LVGL_BUF_LINES;
    const size_t bytes = (size_t)hw->h_res * lines * sizeof(uint16_t);
    for (int i = 0; i < 2; i++) {
        s_buf[i] = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (!s_buf[i]) {
            ESP_LOGW(TAG, "buffer %d en PSRAM (RAM interna insuficiente)", i);
            s_buf[i] = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        ESP_RETURN_ON_FALSE(s_buf[i], ESP_ERR_NO_MEM, TAG, "sin memoria para el buffer %d", i);
    }

    s_disp = lv_display_create(hw->h_res, hw->v_res);
    ESP_RETURN_ON_FALSE(s_disp, ESP_ERR_NO_MEM, TAG, "lv_display_create");
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_disp, s_buf[0], s_buf[1], bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, flush_cb);
    lv_display_set_flush_wait_cb(s_disp, flush_wait_cb);
    lv_display_add_event_cb(s_disp, refr_event_cb, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(s_disp, refr_event_cb, LV_EVENT_REFR_READY, NULL);
    ESP_RETURN_ON_ERROR(adv_board_display_register_trans_done(hw, on_trans_done, s_flush_sem), TAG, "callback DMA2D");

    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, touch_read_cb);
    lv_indev_set_display(s_indev, s_disp);
    lv_timer_set_period(lv_indev_get_read_timer(s_indev), INDEV_PERIOD_MS);

    adv_diag_start();

    BaseType_t ok = xTaskCreatePinnedToCore(lvgl_task, "lvgl", CONFIG_ADV_LVGL_TASK_STACK, NULL,
                                            CONFIG_ADV_LVGL_TASK_PRIORITY, &s_task, CONFIG_ADV_LVGL_TASK_CORE);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "no se pudo crear la tarea de LVGL");
    ESP_LOGI(TAG, "LVGL %d.%d.%d listo: 2 buffers de %u lineas (%u B c/u)", LVGL_VERSION_MAJOR,
             LVGL_VERSION_MINOR, LVGL_VERSION_PATCH, (unsigned)lines, (unsigned)bytes);
    return ESP_OK;
}

bool adv_lvgl_lock(uint32_t timeout_ms)
{
    TickType_t t = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lock, t) == pdTRUE;
}

void adv_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(s_lock);
}

lv_display_t *adv_lvgl_display(void)
{
    return s_disp;
}

lv_indev_t *adv_lvgl_indev(void)
{
    return s_indev;
}

void adv_lvgl_on_first_frame(void (*cb)(void *), void *arg)
{
    s_first_cb = cb;
    s_first_arg = arg;
}
