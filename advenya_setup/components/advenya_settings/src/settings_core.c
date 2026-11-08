/*
 * ADVENYA OS · nucleo portable del estado persistente (sin ESP-IDF ni LVGL)
 */
#include <string.h>
#include "advenya_settings.h"

static const adv_settings_backend_t *s_be;
static adv_settings_t s_cur;        /* estado vivo                         */
static adv_settings_t s_saved;      /* ultimo contenido entregado al backend */
static bool s_have_saved;
static bool s_dirty;
static uint32_t s_dirty_since;
static uint32_t s_writes;

void adv_settings_defaults(adv_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->layout_version = ADV_SETTINGS_LAYOUT_VERSION;
    s->language = 0;                 /* espanol (primero de la lista)        */
    s->theme_mode = ADV_THEME_LIGHT; /* seleccion de la referencia           */
    s->accent = 0;                   /* azul                                 */
    s->wallpaper = 0;                /* fondo azul                           */
    s->stage = ADV_STAGE_WELCOME;
}

/* Normaliza valores fuera de rango y combinaciones incoherentes. */
static void sanitize(adv_settings_t *s)
{
    adv_settings_t d;
    adv_settings_defaults(&d);
    if (s->language >= ADV_SETTINGS_LANG_COUNT) {
        s->language = d.language;
    }
    if (s->theme_mode >= ADV_THEME_COUNT) {
        s->theme_mode = d.theme_mode;
    }
    if (s->accent >= ADV_SETTINGS_ACCENT_COUNT) {
        s->accent = d.accent;
    }
    if (s->wallpaper >= ADV_SETTINGS_WALLPAPER_COUNT) {
        s->wallpaper = d.wallpaper;
    }
    s->privacy_accepted = s->privacy_accepted ? 1 : 0;
    s->setup_complete = s->setup_complete ? 1 : 0;
    s->personalization_deferred = s->personalization_deferred ? 1 : 0;
    if (s->stage >= ADV_STAGE_COUNT) {
        s->stage = ADV_STAGE_WELCOME;
    }
    /* Sin aceptacion no se puede estar mas alla de privacidad. */
    if (!s->privacy_accepted && s->stage > ADV_STAGE_PRIVACY) {
        s->stage = ADV_STAGE_PRIVACY;
        s->setup_complete = 0;
    }
    if (s->setup_complete) {
        s->stage = ADV_STAGE_COMPLETE;
    } else if (s->stage == ADV_STAGE_COMPLETE) {
        s->stage = ADV_STAGE_PERSONALIZE;
    }
    memset(s->reserved, 0, sizeof(s->reserved));
}

bool adv_settings_init(const adv_settings_backend_t *backend)
{
    s_be = backend;
    s_dirty = false;
    s_writes = 0;
    adv_settings_t tmp;
    bool ok = s_be && s_be->load && s_be->load(&tmp);
    if (ok && tmp.layout_version == ADV_SETTINGS_LAYOUT_VERSION) {
        s_cur = tmp;
        sanitize(&s_cur);
        s_saved = tmp;
        s_have_saved = true;
        return true;
    }
    /* Sin datos o formato desconocido: valores por defecto (no se escribe
     * nada hasta el primer cambio real). */
    adv_settings_defaults(&s_cur);
    s_have_saved = false;
    return false;
}

const adv_settings_t *adv_settings_get(void)
{
    return &s_cur;
}

adv_settings_t *adv_settings_edit(void)
{
    return &s_cur;
}

static uint32_t now(void)
{
    return (s_be && s_be->now_ms) ? s_be->now_ms() : 0;
}

void adv_settings_changed(void)
{
    sanitize(&s_cur);
    if (s_have_saved && memcmp(&s_cur, &s_saved, sizeof(s_cur)) == 0) {
        s_dirty = false;          /* se volvio a lo guardado: nada que escribir */
        return;
    }
    s_dirty = true;
    s_dirty_since = now();
}

void adv_settings_commit(void)
{
    sanitize(&s_cur);
    s_dirty = false;
    if (s_have_saved && memcmp(&s_cur, &s_saved, sizeof(s_cur)) == 0) {
        return;
    }
    if (s_be && s_be->save && s_be->save(&s_cur)) {
        s_saved = s_cur;
        s_have_saved = true;
        s_writes++;
    }
}

void adv_settings_poll(void)
{
    if (s_dirty && (uint32_t)(now() - s_dirty_since) >= ADV_SETTINGS_DEBOUNCE_MS) {
        adv_settings_commit();
    }
}

void adv_settings_reset(void)
{
    if (s_be && s_be->erase) {
        s_be->erase();
    }
    adv_settings_defaults(&s_cur);
    s_have_saved = false;
    s_dirty = false;
}

uint32_t adv_settings_write_count(void)
{
    return s_writes;
}
