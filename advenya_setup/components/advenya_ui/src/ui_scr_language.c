/*
 * ADVENYA OS · pantalla 2: seleccion de idioma (ref_02_idioma.png)
 * Espanol, English, Portugues (Brasil), Francais, Deutsch. Al tocar una fila
 * el idioma cambia en el acto en todo el asistente.
 */
#include "ui_internal.h"

#define ROW_X   55
#define ROW_W   371
#define ROW_H   55
#define ROW_Y0  294
#define ROW_DY  66

static const lv_image_dsc_t *const s_flags[ADV_LANG_COUNT] = {
    &img_flag_es, &img_flag_gb, &img_flag_br, &img_flag_fr, &img_flag_de,
};

static struct {
    lv_obj_t *row, *label, *radio, *check;
} s_rows[ADV_LANG_COUNT];

static void select_row(uint8_t sel, bool animate)
{
    for (int i = 0; i < ADV_LANG_COUNT; i++) {
        bool on = (i == sel);
        ui_set_selected(s_rows[i].row, on);
        ui_set_selected(s_rows[i].label, on);
        ui_set_selected(s_rows[i].radio, on);
        ui_fade_indicator(s_rows[i].check, on, animate);
    }
}

static void on_row(lv_event_t *e)
{
    uint8_t lang = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (lang == adv_settings_get()->language) {
        return;
    }
    adv_settings_edit()->language = lang;
    adv_settings_changed();
    ui_i18n_set_language(lang);
    select_row(lang, true);
}

static void on_continue(lv_event_t *e)
{
    (void)e;
    ui_wizard_go(UI_SCR_PRIVACY, +1);
}

lv_obj_t *ui_scr_language_create(lv_obj_t *parent)
{
    static const char *const ids[ADV_LANG_COUNT] = {"lang.es", "lang.en", "lang.pt-BR", "lang.fr", "lang.de"};
    lv_obj_t *root = ui_screen_root(parent);
    ui_lockup(root, &img_lockup_md, 85);
    lv_obj_t *title = ui_label(root, &ui_st.title, TXT_LANG_TITLE);
    ui_test_register("language.title", title);
    lv_obj_set_width(title, 440);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 220);

    for (int i = 0; i < ADV_LANG_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(root);
        lv_obj_remove_style_all(row);
        lv_obj_add_style(row, &ui_st.card, 0);
        lv_obj_add_style(row, &ui_st.card_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(row, &ui_st.card_selected, LV_STATE_CHECKED);
        lv_obj_set_size(row, ROW_W, ROW_H);
        lv_obj_set_pos(row, ROW_X, ROW_Y0 + i * ROW_DY);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, on_row, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        lv_obj_t *flag = lv_image_create(row);
        lv_image_set_src(flag, s_flags[i]);
        lv_obj_align(flag, LV_ALIGN_LEFT_MID, 22, 0);

        lv_obj_t *lbl = lv_label_create(row);
        lv_obj_add_style(lbl, &ui_st.row_label, 0);
        lv_obj_add_style(lbl, &ui_st.row_label_selected, LV_STATE_CHECKED);
        lv_label_set_text_static(lbl, adv_i18n_lang_native[i]);   /* nombre nativo */
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 84, 0);

        lv_obj_t *radio = lv_obj_create(row);
        lv_obj_remove_style_all(radio);
        lv_obj_add_style(radio, &ui_st.radio, 0);
        lv_obj_add_style(radio, &ui_st.radio_checked, LV_STATE_CHECKED);
        lv_obj_set_size(radio, 24, 24);
        lv_obj_align(radio, LV_ALIGN_RIGHT_MID, -25, 0);
        lv_obj_remove_flag(radio, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *chk = lv_image_create(radio);
        lv_image_set_src(chk, &img_ic_check);
        lv_obj_add_style(chk, &ui_st.icon_on_accent, 0);
        lv_obj_center(chk);

        s_rows[i].row = row;
        s_rows[i].label = lbl;
        s_rows[i].radio = radio;
        s_rows[i].check = chk;
        ui_test_register(ids[i], row);
    }
    lv_obj_t *btn = ui_primary_button(root, TXT_BTN_CONTINUE, 325, 680, on_continue, NULL);
    ui_test_register("language.continue", btn);
    select_row(adv_settings_get()->language, false);
    return root;
}

void ui_scr_language_on_show(void)
{
    if (!s_rows[0].row) {
        return;                 /* pantalla aun no creada */
    }
    select_row(adv_settings_get()->language, false);
}
