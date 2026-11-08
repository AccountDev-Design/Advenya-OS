/*
 * ADVENYA OS · entrega de control al nucleo
 *
 * Sin nucleo registrado no se inventa ninguna pantalla: se anota la entrega y
 * se deja visible lo que el asistente mostraba (logotipo fijo o fondo).
 */
#include <stdio.h>
#include "advenya_handoff.h"

static adv_core_entry_t s_entry;
static bool s_done;

void adv_handoff_set_core(adv_core_entry_t entry)
{
    s_entry = entry;
}

bool adv_handoff_done(void)
{
    return s_done;
}

static void default_entry(const adv_handoff_t *h)
{
    printf("[advenya] entrega al nucleo: motivo=%s idioma=%s tema=%s acento=#%06lX fondo=%u%s\n",
           h->reason == ADV_HANDOFF_SETUP_JUST_COMPLETED ? "setup_completado" : "setup_ya_hecho",
           h->language_code, h->dark ? "oscuro" : "claro", (unsigned long)h->accent_rgb,
           (unsigned)h->wallpaper_index + 1,
           h->settings.personalization_deferred ? " (personalizacion pospuesta)" : "");
    printf("[advenya] ningun nucleo registrado (adv_handoff_set_core): la pantalla queda como esta\n");
}

void adv_handoff_dispatch(const adv_handoff_t *h)
{
    if (s_done || !h) {
        return;
    }
    s_done = true;
    (s_entry ? s_entry : default_entry)(h);
}
