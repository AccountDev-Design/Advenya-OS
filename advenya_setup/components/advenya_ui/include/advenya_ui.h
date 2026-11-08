/*
 * ADVENYA OS · primera experiencia (animacion de arranque + asistente)
 *
 * Codigo portable: solo depende de LVGL 9.2, advenya_settings, advenya_assets
 * y advenya_handoff. Lo especifico de la plataforma esta en ui_platform.h
 * (ESP-IDF: src/ui_platform_esp.c; simulador: sim/platform_sim.c).
 */
#pragma once

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Arranca la primera experiencia. Requiere adv_settings_init() hecho y el
 * bloqueo de LVGL tomado. Secuencia:
 *   animacion del logotipo (~3 s) ->
 *     setup completo  : entrega al nucleo (el logotipo queda fijo)
 *     setup pendiente : pantalla de la etapa guardada (bienvenida si es nuevo)
 */
void adv_ui_start(lv_display_t *disp, lv_indev_t *indev);

#ifdef __cplusplus
}
#endif
