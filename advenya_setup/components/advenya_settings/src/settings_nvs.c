/*
 * ADVENYA OS · backend NVS
 *
 * Un unico blob ("advenya"/"setup", 16 bytes): NVS lo escribe de forma atomica,
 * asi que un corte de alimentacion deja el valor anterior o el nuevo, nunca uno
 * a medias. La escritura se hace en una tarea propia de baja prioridad: la
 * interfaz no espera a la flash (un borrado de pagina puede durar decenas de
 * ms). Si llegan varias peticiones seguidas solo se escribe la ultima.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "advenya_settings_nvs.h"

static const char *TAG = "adv_settings";
#define NVS_NS  "advenya"
#define NVS_KEY "setup"

static QueueHandle_t s_q;            /* longitud 1: siempre la ultima peticion */
static SemaphoreHandle_t s_idle;     /* dado cuando no hay escritura pendiente */

static bool be_load(adv_settings_t *out)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(*out);
    esp_err_t err = nvs_get_blob(h, NVS_KEY, out, &len);
    nvs_close(h);
    return err == ESP_OK && len == sizeof(*out);
}

static bool be_save(const adv_settings_t *in)
{
    xSemaphoreTake(s_idle, 0);                  /* hay escritura pendiente */
    return xQueueOverwrite(s_q, in) == pdPASS;
}

static void be_erase(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, NVS_KEY);
        nvs_commit(h);
        nvs_close(h);
    }
}

static uint32_t be_now(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void writer_task(void *arg)
{
    adv_settings_t s;
    for (;;) {
        if (xQueueReceive(s_q, &s, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        nvs_handle_t h;
        esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_blob(h, NVS_KEY, &s, sizeof(s));
            if (err == ESP_OK) {
                err = nvs_commit(h);
            }
            nvs_close(h);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "no se pudo guardar el estado: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "estado guardado (etapa %u, idioma %u, tema %u, color %u, fondo %u, privacidad %u, completo %u)",
                     s.stage, s.language, s.theme_mode, s.accent, s.wallpaper, s.privacy_accepted, s.setup_complete);
        }
        if (uxQueueMessagesWaiting(s_q) == 0) {
            xSemaphoreGive(s_idle);
        }
    }
}

static const adv_settings_backend_t s_backend = {
    .load = be_load,
    .save = be_save,
    .erase = be_erase,
    .now_ms = be_now,
};

esp_err_t adv_settings_nvs_init(const adv_settings_backend_t **out_backend)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "particion NVS inutilizable (%s): se reformatea", esp_err_to_name(err));
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_flash_init");
    s_q = xQueueCreate(1, sizeof(adv_settings_t));
    s_idle = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_q && s_idle, ESP_ERR_NO_MEM, TAG, "sin memoria");
    xSemaphoreGive(s_idle);
    ESP_RETURN_ON_FALSE(xTaskCreate(writer_task, "adv_nvs", 3072, NULL, 2, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "tarea de escritura");
    *out_backend = &s_backend;
    return ESP_OK;
}

bool adv_settings_nvs_flush(uint32_t timeout_ms)
{
    if (xSemaphoreTake(s_idle, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        xSemaphoreGive(s_idle);
        return true;
    }
    return false;
}
