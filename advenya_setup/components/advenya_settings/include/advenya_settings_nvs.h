/* ADVENYA OS · backend NVS del estado persistente (solo ESP-IDF). */
#pragma once
#include "esp_err.h"
#include "advenya_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa la particion NVS (la reformatea solo si NVS informa de que no
 * tiene paginas libres o de una version nueva de formato) y arranca la tarea
 * de escritura de baja prioridad. Devuelve el backend para adv_settings_init().
 */
esp_err_t adv_settings_nvs_init(const adv_settings_backend_t **out_backend);

/**
 * Funciones que rodean cada escritura de NVS (p. ej. la guardia de flash del
 * port de LVGL). Opcional; registrar antes del primer guardado.
 */
void adv_settings_nvs_set_write_guard(void (*begin)(void), void (*end)(void));

/** Bloquea hasta que la ultima escritura pedida este en flash (o timeout). */
bool adv_settings_nvs_flush(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
