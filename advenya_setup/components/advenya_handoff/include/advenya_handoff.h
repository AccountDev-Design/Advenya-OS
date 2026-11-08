/*
 * ADVENYA OS · punto de integracion entre el asistente y el futuro nucleo
 *
 * El asistente de primer arranque NO crea escritorio ni pantalla final. Cuando
 * termina (o al arrancar con la configuracion ya completada) entrega el
 * control llamando a adv_core_takeover() UNA vez, desde la tarea de LVGL y con
 * el bloqueo de LVGL tomado.
 *
 * El nucleo se conecta REGISTRANDO su punto de entrada con
 * adv_handoff_set_core() antes de arrancar la interfaz (ver
 * docs/INTEGRACION_NUCLEO.md). Si nadie se registra, la entrega por defecto
 * solo la anota en el log y deja la pantalla como esta. No hace falta tocar
 * el asistente. (Se usa registro explicito y no un simbolo debil porque con
 * bibliotecas estaticas el enlazador puede quedarse con la version debil.)
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "advenya_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADV_HANDOFF_SETUP_JUST_COMPLETED = 0,  /* el usuario acaba de terminar        */
    ADV_HANDOFF_SETUP_ALREADY_DONE,        /* arranque normal, setup ya completo  */
} adv_handoff_reason_t;

typedef struct {
    adv_handoff_reason_t reason;
    adv_settings_t settings;         /* copia del estado persistido               */
    const char *language_code;       /* "es", "en", "pt-BR", "fr", "de"           */
    bool dark;                       /* tema efectivo (Automatico ya resuelto)    */
    uint32_t accent_rgb;             /* 0xRRGGBB del color de acento elegido      */
    uint8_t wallpaper_index;         /* 0..4                                      */
    void *lv_display;                /* lv_display_t* activo                      */
    void *lv_indev;                  /* lv_indev_t* tactil (puede ser NULL)       */
    /* Retira lo que el asistente deja en pantalla (logotipo de arranque o fondo
     * del asistente) y libera su memoria (~2,3 MB de PSRAM del fondo). Llamar
     * desde la tarea de LVGL cuando el nucleo ya haya pintado su primera
     * pantalla. Idempotente. */
    void (*release_setup_ui)(void);
} adv_handoff_t;

typedef void (*adv_core_entry_t)(const adv_handoff_t *handoff);

/** El nucleo registra aqui su punto de entrada (antes de adv_ui_start()). */
void adv_handoff_set_core(adv_core_entry_t entry);

/** Usado por el asistente: entrega el control (una sola vez por arranque). */
void adv_handoff_dispatch(const adv_handoff_t *handoff);

/** true si ya se entrego el control en este arranque. */
bool adv_handoff_done(void);

#ifdef __cplusplus
}
#endif
