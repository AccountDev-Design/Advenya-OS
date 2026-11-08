/* ADVENYA OS · declaraciones internas de la interfaz del asistente. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "advenya_assets.h"
#include "advenya_settings.h"
#include "i18n/ui_i18n_gen.h"

#define UI_W 480
#define UI_H 800

_Static_assert(ADV_LANG_COUNT == ADV_SETTINGS_LANG_COUNT, "idiomas: settings y traducciones no coinciden");
_Static_assert(ADV_WP_COUNT == ADV_SETTINGS_WALLPAPER_COUNT, "fondos: settings y assets no coinciden");

/* ------------------------------------------------------------ movimiento */
/* "Fluid Motion": valores elegidos dentro de los rangos pedidos. */
#define UI_NAV_SHIFT_PX        20    /* 16..24 px                          */
#define UI_NAV_OUT_MS          260   /* salida, ease in                    */
#define UI_NAV_IN_MS           300   /* entrada, ease out                  */
#define UI_NAV_IN_DELAY_MS     110   /* solape: total = 410 ms (350..450)  */
#define UI_SEL_MS              180   /* cambios de seleccion (150..220)    */
#define UI_PRESS_MS            120   /* respuesta tactil                   */
#define UI_BACKDROP_FADE_MS    320   /* crossfade de fondos                */
#define UI_BOOT_FADE_MS        300   /* transicion final del arranque      */

/* ---------------------------------------------------------------- tema */
typedef struct {
    bool dark;
    lv_color_t bg;               /* color del velo / fondo base             */
    lv_color_t text;             /* titulos y etiquetas                     */
    lv_color_t text_welcome;     /* "¡Bienvenido!"                          */
    lv_color_t text_secondary;   /* subtitulos y descripciones              */
    lv_color_t text_tertiary;    /* "Mas tarde"                             */
    lv_color_t logo;             /* lockup ADVENYA                          */
    lv_color_t surface;          /* tarjetas y filas                        */
    lv_opa_t   surface_opa;
    lv_color_t surface_pressed;
    lv_color_t border;
    lv_opa_t   border_opa;
    lv_color_t radio;            /* circulo sin seleccionar                 */
    lv_color_t sheet;            /* panel de documentos (opaco)             */
    /* derivados del acento */
    lv_color_t accent;
    lv_color_t accent_soft;      /* fondo de la fila seleccionada           */
    lv_color_t accent_border;    /* borde de la fila seleccionada           */
    lv_color_t accent_tile;      /* fondo de los iconos de privacidad       */
    lv_color_t accent_text;      /* enlaces / iconos (contraste suficiente) */
    lv_color_t on_accent;        /* texto/icono sobre el acento             */
    lv_color_t btn_grad_a;       /* degradado del boton principal           */
    lv_color_t btn_grad_b;
} ui_palette_t;

extern const uint32_t ui_accent_rgb[ADV_SETTINGS_ACCENT_COUNT];

typedef struct {
    lv_style_t screen, title, title_welcome, subtitle, body, small;
    lv_style_t logo, icon_accent, icon_text, icon_secondary, icon_on_accent;
    lv_style_t btn, btn_pressed, btn_disabled, btn_label;
    lv_style_t link, link_pressed;
    lv_style_t card, card_pressed, card_selected;
    lv_style_t row_label, row_label_selected;
    lv_style_t tile;
    lv_style_t radio, radio_checked;
    lv_style_t checkbox, checkbox_checked;
    lv_style_t section;
    lv_style_t dot, dot_selected;
    lv_style_t thumb, thumb_selected;
    lv_style_t badge;
    lv_style_t sheet, scrim, sheet_btn, sheet_btn_pressed;
    lv_style_transition_dsc_t trans_sel, trans_press;
} ui_styles_t;

extern ui_styles_t ui_st;

void ui_theme_init(void);
void ui_theme_stop(void);
/** Recalcula la paleta desde los ajustes y actualiza TODOS los estilos. */
void ui_theme_apply(void);
const ui_palette_t *ui_theme_palette(void);
bool ui_theme_is_dark(void);
lv_color_t ui_mix(lv_color_t a, lv_color_t b, uint8_t t_255);

/* ---------------------------------------------------------------- i18n */
typedef void (*ui_i18n_listener_t)(void *user);
void ui_i18n_init(uint8_t lang);
void ui_i18n_set_language(uint8_t lang);
uint8_t ui_i18n_language(void);
const char *ui_tr(adv_txt_t key);
/** La etiqueta mostrara siempre `key` en el idioma actual. */
void ui_i18n_bind(lv_obj_t *label, adv_txt_t key);
/** Para textos compuestos: se llama al cambiar el idioma o el tema. */
void ui_i18n_listen(ui_i18n_listener_t fn, void *user);
void ui_i18n_refresh_all(void);

/* ---------------------------------------------------------------- fondo */
void ui_backdrop_create(lv_obj_t *parent);
/** Pide el fondo (fondo de pantalla + velo del tema). Crossfade si `animate`. */
void ui_backdrop_request(uint8_t wallpaper, bool dark, bool animate);
bool ui_backdrop_ready(void);
void ui_backdrop_release(void);

/* ------------------------------------------------------ arranque (logo) */
typedef void (*ui_boot_done_cb_t)(void);
void ui_boot_start(lv_obj_t *layer, ui_boot_done_cb_t at_end);
/** Funde el logotipo (UI_BOOT_FADE_MS) y libera sus recursos al terminar. */
void ui_boot_fade_out(void);
void ui_boot_release(void);
bool ui_boot_active(void);

/* ----------------------------------------------------------- asistente */
typedef enum {
    UI_SCR_WELCOME = 0,
    UI_SCR_LANGUAGE,
    UI_SCR_PRIVACY,
    UI_SCR_PERSONALIZE,
    UI_SCR_COUNT
} ui_scr_t;

void ui_wizard_init(lv_obj_t *host);
void ui_wizard_show_first(ui_scr_t scr);     /* sin animacion lateral        */
/** Navega; dir = +1 avance, -1 retroceso. Ignorado durante una transicion. */
bool ui_wizard_go(ui_scr_t target, int dir);
bool ui_wizard_busy(void);
ui_scr_t ui_wizard_current(void);
/** Fin del asistente (Continuar / Mas tarde en personalizacion). */
void ui_wizard_finish(bool deferred);

/* Pantallas: crean su arbol de objetos UNA vez; on_show sincroniza estado. */
lv_obj_t *ui_scr_welcome_create(lv_obj_t *parent);
lv_obj_t *ui_scr_language_create(lv_obj_t *parent);
lv_obj_t *ui_scr_privacy_create(lv_obj_t *parent);
lv_obj_t *ui_scr_personalize_create(lv_obj_t *parent);
void ui_scr_language_on_show(void);
void ui_scr_privacy_on_show(void);
void ui_scr_personalize_on_show(void);
bool ui_scr_privacy_checked(void);

/* -------------------------------------------------------------- widgets */
lv_obj_t *ui_screen_root(lv_obj_t *parent);
lv_obj_t *ui_lockup(lv_obj_t *parent, const lv_image_dsc_t *img, int32_t y);
lv_obj_t *ui_label(lv_obj_t *parent, lv_style_t *style, adv_txt_t key);
lv_obj_t *ui_primary_button(lv_obj_t *parent, adv_txt_t key, int32_t w, int32_t y,
                            lv_event_cb_t cb, void *user);
lv_obj_t *ui_link_button(lv_obj_t *parent, adv_txt_t key, int32_t y, lv_event_cb_t cb, void *user);
lv_obj_t *ui_check_badge(lv_obj_t *parent, int32_t size);
void ui_set_selected(lv_obj_t *obj, bool selected);
void ui_add_feedback(lv_obj_t *obj);
void ui_fade_indicator(lv_obj_t *obj, bool show, bool animate);

/* ------------------------------------------------------------ legal */
void ui_legal_sheet_open(lv_obj_t *parent, int doc);
bool ui_legal_sheet_close(void);

/* ---------------------------------------------------------- entrega */
#include "advenya_handoff.h"
/** Rellena adv_handoff_t y la entrega (una sola vez). */
void ui_core_handoff(adv_handoff_reason_t reason, void (*release)(void));
/** Libera logotipo de arranque y fondo (usado por release_setup_ui). */
void ui_wizard_release_for_core(void);

/* ------------------------------------------------- pruebas (simulador) */
void ui_test_register(const char *id, lv_obj_t *obj);
lv_obj_t *ui_test_find(const char *id);
