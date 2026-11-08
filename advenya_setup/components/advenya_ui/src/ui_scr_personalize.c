/*
 * ADVENYA OS · pantalla 4: personalizacion (ref_04_personalizacion.png)
 *
 * Apariencia (claro / oscuro / automatico), color del sistema (8 acentos de
 * la referencia) y fondo de pantalla (5 fondos de la referencia). Cada cambio
 * se aplica en el acto a todo el asistente; el fondo cambia con crossfade.
 * Las elecciones se guardan con escritura diferida y se conservan al
 * retroceder.
 */
#include "ui_internal.h"

#define THEME_W   136
#define THEME_H   106
#define THEME_Y   282
#define DOT_D     34
#define DOT_Y     449
#define THUMB_Y   550
#define THUMB_GAP 8

static const lv_image_dsc_t *const s_theme_img[ADV_THEME_COUNT] = {&img_theme_light, &img_theme_dark, &img_theme_auto};
static const adv_txt_t s_theme_txt[ADV_THEME_COUNT] = {TXT_THEME_LIGHT, TXT_THEME_DARK, TXT_THEME_AUTO};
static const lv_image_dsc_t *const s_thumbs[ADV_WP_COUNT] = {
    &img_thumb_blue, &img_thumb_violet, &img_thumb_peach, &img_thumb_night, &img_thumb_silver,
};

static struct {
    lv_obj_t *card, *badge;
} s_theme[ADV_THEME_COUNT];
static struct {
    lv_obj_t *dot, *check;
} s_dot[ADV_SETTINGS_ACCENT_COUNT];
static struct {
    lv_obj_t *img, *badge;
} s_wp[ADV_WP_COUNT];

static void sync_selection(bool animate)
{
    const adv_settings_t *s = adv_settings_get();
    for (int i = 0; i < ADV_THEME_COUNT; i++) {
        ui_set_selected(s_theme[i].card, i == s->theme_mode);
        ui_fade_indicator(s_theme[i].badge, i == s->theme_mode, animate);
    }
    for (int i = 0; i < ADV_SETTINGS_ACCENT_COUNT; i++) {
        ui_set_selected(s_dot[i].dot, i == s->accent);
        ui_fade_indicator(s_dot[i].check, i == s->accent, animate);
    }
    for (int i = 0; i < ADV_WP_COUNT; i++) {
        ui_set_selected(s_wp[i].img, i == s->wallpaper);
        ui_fade_indicator(s_wp[i].badge, i == s->wallpaper, animate);
    }
}

static void on_theme(lv_event_t *e)
{
    uint8_t v = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (v == adv_settings_get()->theme_mode) {
        return;
    }
    adv_settings_edit()->theme_mode = v;
    adv_settings_changed();
    ui_theme_apply();
    ui_backdrop_request(adv_settings_get()->wallpaper, ui_theme_is_dark(), true);
    sync_selection(true);
}

static void on_accent(lv_event_t *e)
{
    uint8_t v = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (v == adv_settings_get()->accent) {
        return;
    }
    adv_settings_edit()->accent = v;
    adv_settings_changed();
    ui_theme_apply();
    sync_selection(true);
}

static void on_wallpaper(lv_event_t *e)
{
    uint8_t v = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (v == adv_settings_get()->wallpaper) {
        return;
    }
    adv_settings_edit()->wallpaper = v;
    adv_settings_changed();
    ui_backdrop_request(v, ui_theme_is_dark(), true);
    sync_selection(true);
}

static void on_continue(lv_event_t *e)
{
    (void)e;
    ui_wizard_finish(false);
}

static void on_later(lv_event_t *e)
{
    (void)e;
    ui_wizard_finish(true);
}

static void section(lv_obj_t *root, const lv_image_dsc_t *icon, adv_txt_t key, int32_t y)
{
    lv_obj_t *ic = lv_image_create(root);
    lv_image_set_src(ic, icon);
    lv_obj_add_style(ic, &ui_st.icon_text, 0);
    lv_obj_set_pos(ic, 30, y - 3);
    lv_obj_t *l = ui_label(root, &ui_st.section, key);
    lv_obj_set_pos(l, 68, y);
}

lv_obj_t *ui_scr_personalize_create(lv_obj_t *parent)
{
    static const char *const theme_ids[ADV_THEME_COUNT] = {"theme.light", "theme.dark", "theme.auto"};
    static const char *const dot_ids[ADV_SETTINGS_ACCENT_COUNT] = {
        "accent.0", "accent.1", "accent.2", "accent.3", "accent.4", "accent.5", "accent.6", "accent.7"};
    static const char *const wp_ids[ADV_WP_COUNT] = {"wp.0", "wp.1", "wp.2", "wp.3", "wp.4"};

    lv_obj_t *root = ui_screen_root(parent);
    ui_lockup(root, &img_lockup_sm, 52);
    lv_obj_t *title = ui_label(root, &ui_st.title, TXT_PERSO_TITLE);
    ui_test_register("perso.title", title);
    lv_obj_set_width(title, 456);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 146);
    lv_obj_t *sub = ui_label(root, &ui_st.subtitle, TXT_PERSO_SUBTITLE);
    lv_obj_set_width(sub, 410);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 186);

    /* --- apariencia --- */
    section(root, &img_ic_appearance, TXT_SEC_APPEARANCE, 248);
    for (int i = 0; i < ADV_THEME_COUNT; i++) {
        lv_obj_t *c = lv_obj_create(root);
        lv_obj_remove_style_all(c);
        lv_obj_add_style(c, &ui_st.card, 0);
        lv_obj_add_style(c, &ui_st.card_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(c, &ui_st.card_selected, LV_STATE_CHECKED);
        lv_obj_set_size(c, THEME_W, THEME_H);
        lv_obj_set_pos(c, 30 + i * (THEME_W + 7), THEME_Y);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(c, on_theme, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *prev = lv_image_create(c);
        lv_image_set_src(prev, s_theme_img[i]);
        lv_obj_align(prev, LV_ALIGN_TOP_MID, 0, 12);
        lv_obj_t *l = ui_label(c, &ui_st.body, s_theme_txt[i]);
        lv_obj_set_style_text_font(l, &adv_font_m16, 0);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 74);
        s_theme[i].card = c;
        s_theme[i].badge = ui_check_badge(c, 24);
        lv_obj_align(s_theme[i].badge, LV_ALIGN_TOP_RIGHT, -5, 5);
        ui_test_register(theme_ids[i], c);
    }

    /* --- color del sistema --- */
    section(root, &img_ic_palette, TXT_SEC_ACCENT, 412);
    for (int i = 0; i < ADV_SETTINGS_ACCENT_COUNT; i++) {
        lv_color_t col = lv_color_hex(ui_accent_rgb[i]);
        lv_obj_t *d = lv_obj_create(root);
        lv_obj_remove_style_all(d);
        lv_obj_add_style(d, &ui_st.dot, 0);
        lv_obj_add_style(d, &ui_st.dot_selected, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(d, col, 0);
        lv_obj_set_style_outline_color(d, col, 0);
        lv_obj_set_size(d, DOT_D, DOT_D);
        lv_obj_set_pos(d, 70 + (i * 509) / 10 - DOT_D / 2, DOT_Y);
        lv_obj_add_flag(d, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(d, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_ext_click_area(d, 8);
        lv_obj_add_event_cb(d, on_accent, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *chk = lv_image_create(d);
        lv_image_set_src(chk, &img_ic_check);
        bool bright = (299u * col.red + 587u * col.green + 114u * col.blue) / 1000u > 185u;
        lv_obj_set_style_image_recolor(chk, bright ? lv_color_hex(0x1A1F2B) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_image_recolor_opa(chk, LV_OPA_COVER, 0);
        lv_obj_center(chk);
        s_dot[i].dot = d;
        s_dot[i].check = chk;
        ui_test_register(dot_ids[i], d);
    }

    /* --- fondo de pantalla --- */
    section(root, &img_ic_image, TXT_SEC_WALLPAPER, 515);
    const int32_t x0 = (UI_W - (ADV_WP_COUNT * ADV_THUMB_W + (ADV_WP_COUNT - 1) * THUMB_GAP)) / 2;
    for (int i = 0; i < ADV_WP_COUNT; i++) {
        lv_obj_t *t = lv_image_create(root);
        lv_image_set_src(t, s_thumbs[i]);
        lv_obj_add_style(t, &ui_st.thumb, 0);
        lv_obj_add_style(t, &ui_st.thumb_selected, LV_STATE_CHECKED);
        lv_obj_set_pos(t, x0 + i * (ADV_THUMB_W + THUMB_GAP), THUMB_Y);
        lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(t, on_wallpaper, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_wp[i].img = t;
        s_wp[i].badge = ui_check_badge(t, 24);
        lv_obj_align(s_wp[i].badge, LV_ALIGN_TOP_RIGHT, -6, 6);
        ui_test_register(wp_ids[i], t);
    }

    lv_obj_t *btn = ui_primary_button(root, TXT_BTN_CONTINUE, 325, 683, on_continue, NULL);
    ui_test_register("perso.continue", btn);
    lv_obj_t *later = ui_link_button(root, TXT_BTN_LATER, 738, on_later, NULL);
    ui_test_register("perso.later", later);
    sync_selection(false);
    return root;
}

void ui_scr_personalize_on_show(void)
{
    if (!s_theme[0].card) {
        return;
    }
    sync_selection(false);
}
