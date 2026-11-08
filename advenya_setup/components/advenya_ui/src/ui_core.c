/*
 * ADVENYA OS · orquestacion de la primera experiencia
 *
 *   adv_ui_start()
 *     ├─ prepara tema, idioma y (si el setup no esta completo) el fondo y la
 *     │  pantalla de la etapa guardada, ocultos bajo la animacion de arranque
 *     ├─ animacion del logotipo (~3 s)
 *     └─ al terminar:
 *          setup pendiente -> espera a que el fondo este listo (el logotipo
 *                             queda fijo mientras tanto) y funde 300 ms a la
 *                             pantalla de la etapa guardada
 *          setup completo  -> entrega al nucleo; el logotipo queda fijo
 *                             hasta que el nucleo llame a release_setup_ui()
 */
#include "advenya_ui.h"
#include "ui_internal.h"
#include "ui_platform.h"
#include "advenya_handoff.h"

static lv_display_t *s_disp;
static lv_indev_t *s_indev;
static lv_obj_t *s_host;
static lv_timer_t *s_wait;
static bool s_setup_done_at_boot;

static void host_colors(void *user)
{
    (void)user;
    if (s_host) {
        lv_obj_set_style_bg_color(s_host, ui_theme_palette()->bg, 0);
    }
}

void ui_core_handoff(adv_handoff_reason_t reason, void (*release)(void))
{
    const adv_settings_t *s = adv_settings_get();
    adv_handoff_t h = {
        .reason = reason,
        .settings = *s,
        .language_code = adv_i18n_lang_code[s->language],
        .dark = ui_theme_is_dark(),
        .accent_rgb = ui_accent_rgb[s->accent],
        .wallpaper_index = s->wallpaper,
        .lv_display = s_disp,
        .lv_indev = s_indev,
        .release_setup_ui = release,
    };
    ui_theme_stop();
    ui_platform_log_mem("entrega al nucleo");
    adv_handoff_dispatch(&h);
}

static void release_after_boot(void)
{
    ui_boot_fade_out();          /* funde el logotipo y libera sus buffers */
    ui_backdrop_release();
}

static void wait_ready_cb(lv_timer_t *t)
{
    if (!ui_backdrop_ready()) {
        return;                  /* "mantener el logo hasta que el sistema este listo" */
    }
    lv_timer_delete(t);
    s_wait = NULL;
    ui_boot_fade_out();
}

static void boot_end(void)
{
    if (s_setup_done_at_boot) {
        ui_core_handoff(ADV_HANDOFF_SETUP_ALREADY_DONE, release_after_boot);
        return;
    }
    s_wait = lv_timer_create(wait_ready_cb, 20, NULL);
    lv_timer_ready(s_wait);
}

void adv_ui_start(lv_display_t *disp, lv_indev_t *indev)
{
    s_disp = disp;
    s_indev = indev;
    const adv_settings_t *s = adv_settings_get();
    s_setup_done_at_boot = s->setup_complete;
    ui_platform_log_mem("inicio de la interfaz");

    ui_i18n_init(s->language);
    ui_theme_init();

    s_host = lv_screen_active();
    lv_obj_remove_flag(s_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_host, LV_OPA_COVER, 0);
    host_colors(NULL);
    ui_i18n_listen(host_colors, NULL);   /* se llama tambien al cambiar el tema */

    if (!s_setup_done_at_boot) {
        ui_backdrop_create(s_host);
        ui_backdrop_request(s->wallpaper, ui_theme_is_dark(), false);
        ui_wizard_init(s_host);
        ui_scr_t first = UI_SCR_WELCOME;
        switch (s->stage) {
        case ADV_STAGE_LANGUAGE:    first = UI_SCR_LANGUAGE; break;
        case ADV_STAGE_PRIVACY:     first = UI_SCR_PRIVACY; break;
        case ADV_STAGE_PERSONALIZE: first = UI_SCR_PERSONALIZE; break;
        default:                    first = UI_SCR_WELCOME; break;
        }
        ui_wizard_show_first(first);
    }
    ui_boot_start(lv_layer_top(), boot_end);
}
