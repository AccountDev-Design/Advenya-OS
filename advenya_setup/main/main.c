/*
 * ADVENYA OS · advenya_setup
 * Primera experiencia: animacion del logotipo + asistente de configuracion.
 * Placa GUITION JC4880P443C_I_W (ESP32-P4), ESP-IDF 5.5.5, LVGL 9.2.2.
 *
 * Orden de arranque:
 *   1. NVS + estado del asistente (para saber en que etapa se quedo)
 *   2. retroiluminacion APAGADA, panel ST7701 por MIPI-DSI, tactil GT911
 *   3. LVGL (buffers, flush DMA2D, indev, tarea propia)
 *   4. interfaz; la retroiluminacion se enciende cuando el primer fotograma
 *      ya esta en el panel (sin destello blanco).
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "sdkconfig.h"
#include "advenya_board.h"
#include "advenya_lvgl_port.h"
#include "advenya_settings.h"
#include "advenya_settings_nvs.h"
#include "advenya_ui.h"

static const char *TAG = "advenya";
static adv_display_t s_display;

static void backlight_on(void *arg)
{
    (void)arg;
    adv_board_backlight_set(CONFIG_ADV_BOARD_BACKLIGHT_DEFAULT);
    ESP_LOGI(TAG, "primer fotograma en el panel: retroiluminacion al %d %%", CONFIG_ADV_BOARD_BACKLIGHT_DEFAULT);
}

static void fatal_display(void)
{
    /* Igual que Flex OS Ultra: sin panel, la retroiluminacion parpadea para
     * que el fallo sea visible sin monitor serie. */
    for (;;) {
        adv_board_backlight_set(100);
        vTaskDelay(pdMS_TO_TICKS(150));
        adv_board_backlight_set(0);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "ADVENYA setup · ESP32-P4 rev v%d.%d · %d nucleos", chip.revision / 100, chip.revision % 100,
             chip.cores);

    /* 1) estado persistente */
    const adv_settings_backend_t *backend = NULL;
    ESP_ERROR_CHECK(adv_settings_nvs_init(&backend));
#if CONFIG_ADV_SETUP_RESET_ON_BOOT
    adv_settings_init(backend);
    adv_settings_reset();
    ESP_LOGW(TAG, "ADV_SETUP_RESET_ON_BOOT: estado del asistente borrado");
#else
    bool had = adv_settings_init(backend);
    const adv_settings_t *s = adv_settings_get();
    ESP_LOGI(TAG, "estado %s: etapa %u, completo %u, idioma %u, tema %u, color %u, fondo %u, privacidad %u",
             had ? "recuperado" : "nuevo", s->stage, s->setup_complete, s->language, s->theme_mode, s->accent,
             s->wallpaper, s->privacy_accepted);
#endif

    /* 2) hardware */
    ESP_ERROR_CHECK(adv_board_backlight_init());
    esp_err_t err = adv_board_display_init(&s_display);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "panel: %s; segundo intento", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(150));
        err = adv_board_display_init(&s_display);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo inicializar el panel: %s", esp_err_to_name(err));
        fatal_display();
    }
    if (adv_board_touch_init() != ESP_OK) {
        ESP_LOGW(TAG, "se continua sin tactil");
    }

    /* 3) LVGL */
    adv_lvgl_on_first_frame(backlight_on, NULL);
    ESP_ERROR_CHECK(adv_lvgl_port_init(&s_display));
    /* Las escrituras de NVS no se solapan con una copia DMA2D en curso. */
    adv_settings_nvs_set_write_guard(adv_lvgl_flash_guard_begin, adv_lvgl_flash_guard_end);

    /* 4) interfaz */
    adv_lvgl_lock(0);
    adv_ui_start(adv_lvgl_display(), adv_lvgl_indev());
    adv_lvgl_unlock();
}
