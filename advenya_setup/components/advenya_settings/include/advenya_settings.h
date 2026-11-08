/*
 * ADVENYA OS · estado persistente del asistente de configuracion
 *
 * El nucleo (settings_core.c) es C portable: no depende de ESP-IDF ni de LVGL.
 * La escritura real la hace un "backend" (NVS en la placa, archivo en el
 * simulador). Politica de escritura:
 *   - adv_settings_changed(): cambio de una eleccion (idioma, tema, color,
 *     fondo). Se agrupa y se escribe tras ADV_SETTINGS_DEBOUNCE_MS sin cambios.
 *   - adv_settings_commit(): punto de recuperacion (cambio de etapa,
 *     aceptacion, final). Se escribe ya.
 *   - Nunca se escribe si el contenido es identico a lo ultimo persistido.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADV_SETTINGS_LAYOUT_VERSION 1
#define ADV_SETTINGS_DEBOUNCE_MS    1500

#define ADV_SETTINGS_LANG_COUNT      5   /* es, en, pt-BR, fr, de                */
#define ADV_SETTINGS_ACCENT_COUNT    8   /* paleta de la referencia              */
#define ADV_SETTINGS_WALLPAPER_COUNT 5   /* fondos de la referencia              */

typedef enum {
    ADV_THEME_LIGHT = 0,
    ADV_THEME_DARK,
    ADV_THEME_AUTO,
    ADV_THEME_COUNT
} adv_theme_mode_t;

/* Etapa del asistente: donde continuar si se corta la alimentacion. */
typedef enum {
    ADV_STAGE_WELCOME = 0,
    ADV_STAGE_LANGUAGE,
    ADV_STAGE_PRIVACY,
    ADV_STAGE_PERSONALIZE,
    ADV_STAGE_COMPLETE,
    ADV_STAGE_COUNT
} adv_stage_t;

/* Formato persistido (16 bytes, un unico blob atomico). */
typedef struct {
    uint8_t  layout_version;           /* ADV_SETTINGS_LAYOUT_VERSION            */
    uint8_t  language;                 /* adv_lang_t                             */
    uint8_t  theme_mode;               /* adv_theme_mode_t                       */
    uint8_t  accent;                   /* 0..ADV_SETTINGS_ACCENT_COUNT-1         */
    uint8_t  wallpaper;                /* 0..ADV_SETTINGS_WALLPAPER_COUNT-1      */
    uint8_t  privacy_accepted;         /* aceptacion explicita (checkbox)        */
    uint16_t legal_version_accepted;   /* version del paquete legal aceptado     */
    uint8_t  stage;                    /* adv_stage_t                            */
    uint8_t  setup_complete;           /* 1 = el nucleo toma el control al arrancar */
    uint8_t  personalization_deferred; /* "Mas tarde" en personalizacion         */
    uint8_t  reserved[5];
} adv_settings_t;

typedef struct {
    /* Devuelve true y rellena `out` si hay datos guardados. */
    bool (*load)(adv_settings_t *out);
    /* Persiste `in`. Puede ser asincrono (la copia se hace antes de volver). */
    bool (*save)(const adv_settings_t *in);
    /* Borra lo guardado. */
    void (*erase)(void);
    /* Reloj en milisegundos (monotono). */
    uint32_t (*now_ms)(void);
} adv_settings_backend_t;

/** Carga, valida y normaliza. Devuelve true si habia datos validos. */
bool adv_settings_init(const adv_settings_backend_t *backend);

/** Estado actual (solo lectura). */
const adv_settings_t *adv_settings_get(void);

/** Copia editable: modifica y llama a adv_settings_changed() o _commit(). */
adv_settings_t *adv_settings_edit(void);

/** Marca un cambio de eleccion (escritura diferida). */
void adv_settings_changed(void);

/** Escribe ya si difiere de lo persistido (punto de recuperacion). */
void adv_settings_commit(void);

/** Llamar periodicamente: ejecuta la escritura diferida cuando toca. */
void adv_settings_poll(void);

/** Borra lo guardado y vuelve a los valores por defecto (sin escribir). */
void adv_settings_reset(void);

/** Escrituras reales realizadas desde el arranque (diagnostico / pruebas). */
uint32_t adv_settings_write_count(void);

/** Valores por defecto: espanol, claro, azul, fondo 1, etapa bienvenida. */
void adv_settings_defaults(adv_settings_t *s);

#ifdef __cplusplus
}
#endif
