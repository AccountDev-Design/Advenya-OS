/* ADVENYA OS · servicios de plataforma que necesita la interfaz. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Memoria grande (fondos ~768 KB, capas del logotipo): PSRAM en la placa. */
void *ui_platform_alloc_large(size_t bytes);
void ui_platform_free_large(void *p);

/*
 * Ejecuta `work(arg)` fuera de la tarea de LVGL y despues `done(arg)` DENTRO
 * de la tarea de LVGL. En el simulador se ejecuta todo de forma sincrona.
 * Devuelve false si no se pudo encolar (entonces no se llama a nada).
 */
bool ui_platform_run_async(void (*work)(void *), void (*done)(void *), void *arg);

/* Tema "Automatico": 1 = noche, 0 = dia, -1 = hora desconocida. */
int ui_platform_is_night(void);

/* Uso de memoria para el log de diagnostico de la interfaz. */
void ui_platform_log_mem(const char *where);
