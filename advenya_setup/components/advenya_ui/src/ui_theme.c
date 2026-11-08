/*
 * ADVENYA OS · tema del asistente (claro / oscuro / automatico + acento)
 *
 * Todos los objetos usan estilos COMPARTIDOS (ui_st). Cambiar el tema o el
 * acento reescribe esos estilos y notifica a LVGL una sola vez, de modo que
 * el cambio llega a la vez a todas las pantallas ya creadas, sin recrearlas.
 * Colores tomados de las referencias (muestreo de pixeles); ver
 * docs/DISENO.md.
 */
#include "ui_internal.h"
#include "ui_platform.h"

/* Paleta de acentos de la referencia de personalizacion, en orden. */
const uint32_t ui_accent_rgb[ADV_SETTINGS_ACCENT_COUNT] = {
    0x0066FC, /* azul      */
    0x855FEE, /* violeta   */
    0xD158BB, /* magenta   */
    0xE5363D, /* rojo      */
    0xFD9440, /* naranja   */
    0xFDCD43, /* amarillo  */
    0x72C456, /* verde     */
    0x31B8B7, /* turquesa  */
};

ui_styles_t ui_st;
static ui_palette_t s_pal;
static bool s_inited;
static lv_timer_t *s_auto_timer;

static const lv_style_prop_t s_sel_props[] = {
    LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_WIDTH,
    LV_STYLE_BORDER_OPA, LV_STYLE_OUTLINE_WIDTH, LV_STYLE_OUTLINE_OPA, LV_STYLE_PROP_INV,
};
static const lv_style_prop_t s_press_props[] = {
    LV_STYLE_OPA, LV_STYLE_TRANSLATE_Y, LV_STYLE_BG_COLOR, LV_STYLE_SHADOW_OPA, LV_STYLE_PROP_INV,
};

lv_color_t ui_mix(lv_color_t a, lv_color_t b, uint8_t t)
{
    /* t = 0 -> a, 255 -> b */
    return lv_color_mix(b, a, t);
}

static uint8_t perceived(lv_color_t c)
{
    return (uint8_t)((299u * c.red + 587u * c.green + 114u * c.blue) / 1000u);
}

const ui_palette_t *ui_theme_palette(void)
{
    return &s_pal;
}

bool ui_theme_is_dark(void)
{
    return s_pal.dark;
}

static bool resolve_dark(void)
{
    const adv_settings_t *s = adv_settings_get();
    if (s->theme_mode == ADV_THEME_DARK) {
        return true;
    }
    if (s->theme_mode == ADV_THEME_AUTO) {
        /* Sin hora conocida (el asistente no tiene red ni RTC ajustado) se
         * resuelve a claro; ver docs/DISENO.md. */
        return ui_platform_is_night() == 1;
    }
    return false;
}

static void build_palette(ui_palette_t *p)
{
    const adv_settings_t *s = adv_settings_get();
    const lv_color_t white = lv_color_hex(0xFFFFFF);
    const lv_color_t black = lv_color_hex(0x000000);
    p->dark = resolve_dark();
    p->accent = lv_color_hex(ui_accent_rgb[s->accent]);
    if (!p->dark) {
        p->bg = white;
        p->text = lv_color_hex(0x0B1016);
        p->text_welcome = lv_color_hex(0x0B2367);
        p->text_secondary = lv_color_hex(0x4A5568);
        p->text_tertiary = lv_color_hex(0x3E4A5C);
        p->logo = black;
        p->surface = white;
        p->surface_opa = 205;
        p->surface_pressed = lv_color_hex(0xEEF3FA);
        p->border = lv_color_hex(0xDFE6F0);
        p->border_opa = LV_OPA_COVER;
        p->radio = lv_color_hex(0x9298A2);
        p->sheet = white;
        p->accent_soft = ui_mix(white, p->accent, 36);       /* ~14 % */
        p->accent_border = ui_mix(white, p->accent, 140);    /* ~55 % */
        p->accent_tile = ui_mix(white, p->accent, 20);       /* ~8 %  */
        p->accent_text = perceived(p->accent) > 150 ? ui_mix(p->accent, black, 90) : ui_mix(p->accent, black, 30);
    } else {
        p->bg = lv_color_hex(0x0A0F1C);
        p->text = lv_color_hex(0xF2F5FA);
        p->text_welcome = lv_color_hex(0xE8EEFF);
        p->text_secondary = lv_color_hex(0xA3AEC2);
        p->text_tertiary = lv_color_hex(0xB4BDCC);
        p->logo = white;
        p->surface = white;
        p->surface_opa = 16;
        p->surface_pressed = lv_color_hex(0x232B3D);
        p->border = white;
        p->border_opa = 30;
        p->radio = lv_color_hex(0x6B7488);
        p->sheet = lv_color_hex(0x151C2C);
        p->accent_soft = ui_mix(p->bg, p->accent, 60);
        p->accent_border = ui_mix(p->bg, p->accent, 180);
        p->accent_tile = ui_mix(p->bg, p->accent, 46);
        p->accent_text = ui_mix(p->accent, white, 64);
    }
    p->on_accent = perceived(p->accent) > 185 ? lv_color_hex(0x1A1F2B) : white;
    p->btn_grad_a = ui_mix(p->accent, white, 77);
    p->btn_grad_b = ui_mix(p->accent, lv_color_hex(0x000033), 115);
}

static void text_style(lv_style_t *st, const lv_font_t *font, lv_color_t color)
{
    lv_style_set_text_font(st, font);
    lv_style_set_text_color(st, color);
}

static void icon_style(lv_style_t *st, lv_color_t c)
{
    lv_style_set_image_recolor(st, c);
    lv_style_set_image_recolor_opa(st, LV_OPA_COVER);
}

static void write_styles(void)
{
    const ui_palette_t *p = &s_pal;

    /* contenedor de pantalla: transparente, sin relleno ni desplazamiento */
    lv_style_set_bg_opa(&ui_st.screen, LV_OPA_TRANSP);
    lv_style_set_border_width(&ui_st.screen, 0);
    lv_style_set_pad_all(&ui_st.screen, 0);
    lv_style_set_radius(&ui_st.screen, 0);

    text_style(&ui_st.title, &adv_font_b28, p->text);
    lv_style_set_text_align(&ui_st.title, LV_TEXT_ALIGN_CENTER);
    text_style(&ui_st.title_welcome, &adv_font_b36, p->text_welcome);
    lv_style_set_text_align(&ui_st.title_welcome, LV_TEXT_ALIGN_CENTER);
    text_style(&ui_st.subtitle, &adv_font_r16, p->text_secondary);
    lv_style_set_text_align(&ui_st.subtitle, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_line_space(&ui_st.subtitle, 4);
    text_style(&ui_st.body, &adv_font_r16, p->text);
    lv_style_set_text_line_space(&ui_st.body, 3);
    text_style(&ui_st.small, &adv_font_r14, p->text_secondary);
    lv_style_set_text_line_space(&ui_st.small, 2);

    icon_style(&ui_st.logo, p->logo);
    icon_style(&ui_st.icon_accent, p->accent_text);
    icon_style(&ui_st.icon_text, p->text);
    icon_style(&ui_st.icon_secondary, p->text_secondary);
    icon_style(&ui_st.icon_on_accent, p->on_accent);

    /* boton principal: degradado del acento y halo suave */
    lv_style_set_radius(&ui_st.btn, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&ui_st.btn, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_st.btn, p->btn_grad_a);
    lv_style_set_bg_grad_color(&ui_st.btn, p->btn_grad_b);
    lv_style_set_bg_grad_dir(&ui_st.btn, LV_GRAD_DIR_HOR);
    lv_style_set_border_width(&ui_st.btn, 0);
    lv_style_set_shadow_color(&ui_st.btn, p->accent);
    lv_style_set_shadow_width(&ui_st.btn, 28);
    lv_style_set_shadow_spread(&ui_st.btn, -6);
    lv_style_set_shadow_offset_y(&ui_st.btn, 10);
    lv_style_set_shadow_opa(&ui_st.btn, p->dark ? 70 : 95);
    lv_style_set_pad_all(&ui_st.btn, 0);
    lv_style_set_transition(&ui_st.btn, &ui_st.trans_press);
    lv_style_set_opa(&ui_st.btn_pressed, 222);
    lv_style_set_translate_y(&ui_st.btn_pressed, 1);
    lv_style_set_shadow_opa(&ui_st.btn_pressed, p->dark ? 40 : 55);
    lv_style_set_opa(&ui_st.btn_disabled, 120);
    text_style(&ui_st.btn_label, &adv_font_sb22, p->on_accent);

    text_style(&ui_st.link, &adv_font_m16, p->text_tertiary);
    lv_style_set_bg_opa(&ui_st.link, LV_OPA_TRANSP);
    lv_style_set_pad_hor(&ui_st.link, 18);
    lv_style_set_pad_ver(&ui_st.link, 10);
    lv_style_set_transition(&ui_st.link, &ui_st.trans_press);
    lv_style_set_opa(&ui_st.link_pressed, 130);

    /* tarjetas y filas */
    lv_style_set_bg_color(&ui_st.card, p->surface);
    lv_style_set_bg_opa(&ui_st.card, p->surface_opa);
    lv_style_set_border_color(&ui_st.card, p->border);
    lv_style_set_border_opa(&ui_st.card, p->border_opa);
    lv_style_set_border_width(&ui_st.card, 1);
    lv_style_set_radius(&ui_st.card, 18);
    lv_style_set_pad_all(&ui_st.card, 0);
    lv_style_set_transition(&ui_st.card, &ui_st.trans_sel);
    lv_style_set_bg_color(&ui_st.card_pressed, p->surface_pressed);
    lv_style_set_bg_opa(&ui_st.card_pressed, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_st.card_selected, p->accent_soft);
    lv_style_set_bg_opa(&ui_st.card_selected, LV_OPA_COVER);
    lv_style_set_border_color(&ui_st.card_selected, p->accent_border);
    lv_style_set_border_opa(&ui_st.card_selected, LV_OPA_COVER);
    lv_style_set_border_width(&ui_st.card_selected, 2);

    text_style(&ui_st.row_label, &adv_font_m20, p->text);
    text_style(&ui_st.row_label_selected, &adv_font_sb20, p->text);

    lv_style_set_radius(&ui_st.tile, 16);
    lv_style_set_bg_color(&ui_st.tile, p->accent_tile);
    lv_style_set_bg_opa(&ui_st.tile, LV_OPA_COVER);
    lv_style_set_border_width(&ui_st.tile, 0);

    /* seleccion circular (idioma) */
    lv_style_set_radius(&ui_st.radio, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&ui_st.radio, LV_OPA_TRANSP);
    lv_style_set_bg_color(&ui_st.radio, p->accent);
    lv_style_set_border_color(&ui_st.radio, p->radio);
    lv_style_set_border_width(&ui_st.radio, 2);
    lv_style_set_border_opa(&ui_st.radio, LV_OPA_COVER);
    lv_style_set_pad_all(&ui_st.radio, 0);
    lv_style_set_transition(&ui_st.radio, &ui_st.trans_sel);
    lv_style_set_bg_opa(&ui_st.radio_checked, LV_OPA_COVER);
    lv_style_set_border_opa(&ui_st.radio_checked, LV_OPA_TRANSP);

    /* casilla de aceptacion */
    lv_style_set_radius(&ui_st.checkbox, 7);
    lv_style_set_bg_color(&ui_st.checkbox, p->surface);
    lv_style_set_bg_opa(&ui_st.checkbox, p->dark ? 16 : LV_OPA_COVER);
    lv_style_set_border_color(&ui_st.checkbox, p->radio);
    lv_style_set_border_width(&ui_st.checkbox, 2);
    lv_style_set_border_opa(&ui_st.checkbox, LV_OPA_COVER);
    lv_style_set_pad_all(&ui_st.checkbox, 0);
    lv_style_set_transition(&ui_st.checkbox, &ui_st.trans_sel);
    lv_style_set_bg_color(&ui_st.checkbox_checked, p->accent);
    lv_style_set_bg_opa(&ui_st.checkbox_checked, LV_OPA_COVER);
    lv_style_set_border_color(&ui_st.checkbox_checked, p->accent);

    text_style(&ui_st.section, &adv_font_sb18, p->text);

    /* puntos de color: el color propio va en estilo local */
    lv_style_set_radius(&ui_st.dot, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&ui_st.dot, LV_OPA_COVER);
    lv_style_set_border_color(&ui_st.dot, p->bg);
    lv_style_set_border_width(&ui_st.dot, 0);
    lv_style_set_outline_width(&ui_st.dot, 0);
    lv_style_set_outline_pad(&ui_st.dot, 0);
    lv_style_set_pad_all(&ui_st.dot, 0);
    lv_style_set_transition(&ui_st.dot, &ui_st.trans_sel);
    lv_style_set_border_width(&ui_st.dot_selected, 3);
    lv_style_set_outline_width(&ui_st.dot_selected, 2);
    lv_style_set_outline_opa(&ui_st.dot_selected, LV_OPA_COVER);

    lv_style_set_radius(&ui_st.thumb, 14);
    lv_style_set_outline_width(&ui_st.thumb, 0);
    lv_style_set_outline_color(&ui_st.thumb, p->accent);
    lv_style_set_outline_pad(&ui_st.thumb, 2);
    lv_style_set_transition(&ui_st.thumb, &ui_st.trans_sel);
    lv_style_set_outline_width(&ui_st.thumb_selected, 3);
    lv_style_set_outline_opa(&ui_st.thumb_selected, LV_OPA_COVER);

    lv_style_set_radius(&ui_st.badge, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&ui_st.badge, p->accent);
    lv_style_set_bg_opa(&ui_st.badge, LV_OPA_COVER);
    lv_style_set_border_color(&ui_st.badge, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&ui_st.badge, 2);
    lv_style_set_pad_all(&ui_st.badge, 0);

    /* panel de documentos legales */
    lv_style_set_bg_color(&ui_st.sheet, p->sheet);
    lv_style_set_bg_opa(&ui_st.sheet, LV_OPA_COVER);
    lv_style_set_radius(&ui_st.sheet, 26);
    lv_style_set_border_width(&ui_st.sheet, p->dark ? 1 : 0);
    lv_style_set_border_color(&ui_st.sheet, p->border);
    lv_style_set_border_opa(&ui_st.sheet, p->border_opa);
    lv_style_set_pad_all(&ui_st.sheet, 24);
    lv_style_set_bg_color(&ui_st.scrim, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&ui_st.scrim, p->dark ? 150 : 90);
    lv_style_set_border_width(&ui_st.scrim, 0);
    lv_style_set_radius(&ui_st.scrim, 0);
    lv_style_set_radius(&ui_st.sheet_btn, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&ui_st.sheet_btn, LV_OPA_TRANSP);
    lv_style_set_border_color(&ui_st.sheet_btn, p->accent_text);
    lv_style_set_border_width(&ui_st.sheet_btn, 2);
    text_style(&ui_st.sheet_btn, &adv_font_m16, p->accent_text);
    lv_style_set_transition(&ui_st.sheet_btn, &ui_st.trans_press);
    lv_style_set_bg_color(&ui_st.sheet_btn_pressed, p->accent_soft);
    lv_style_set_bg_opa(&ui_st.sheet_btn_pressed, LV_OPA_COVER);
}

static void auto_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (adv_settings_get()->theme_mode == ADV_THEME_AUTO && resolve_dark() != s_pal.dark) {
        ui_theme_apply();
        ui_backdrop_request(adv_settings_get()->wallpaper, s_pal.dark, true);
    }
}

void ui_theme_init(void)
{
    if (s_inited) {
        return;
    }
    lv_style_t *all = (lv_style_t *)&ui_st;
    size_t n = offsetof(ui_styles_t, trans_sel) / sizeof(lv_style_t);
    for (size_t i = 0; i < n; i++) {
        lv_style_init(&all[i]);
    }
    lv_style_transition_dsc_init(&ui_st.trans_sel, s_sel_props, lv_anim_path_ease_out, UI_SEL_MS, 0, NULL);
    lv_style_transition_dsc_init(&ui_st.trans_press, s_press_props, lv_anim_path_ease_out, UI_PRESS_MS, 0, NULL);
    s_inited = true;
    build_palette(&s_pal);
    write_styles();
    /* "Automatico": se reevalua cada minuto (sin efecto mientras la hora sea
     * desconocida). */
    s_auto_timer = lv_timer_create(auto_timer_cb, 60000, NULL);
}

void ui_theme_stop(void)
{
    if (s_auto_timer) {
        lv_timer_delete(s_auto_timer);   /* tras la entrega el tema es del nucleo */
        s_auto_timer = NULL;
    }
}

void ui_theme_apply(void)
{
    build_palette(&s_pal);
    write_styles();
    lv_obj_report_style_change(NULL);     /* todas las pantallas, sin recrearlas */
    ui_i18n_refresh_all();                /* textos compuestos con color (enlaces) */
}
