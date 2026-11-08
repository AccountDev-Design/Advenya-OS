/*
 * ADVENYA OS · pantalla 3: privacidad, permisos y condiciones (ref_03_privacidad.png)
 *
 * - Tres apartados interactivos: abren el documento correspondiente.
 * - Aceptacion EXPLICITA: "Aceptar y continuar" solo avanza con la casilla
 *   marcada; sin ella, la casilla se resalta brevemente (sin rebotes).
 * - "Mas tarde" vuelve a la bienvenida SIN registrar la aceptacion.
 */
#include "ui_internal.h"
#include "legal/legal_docs.h"

#define CARD_X  36
#define CARD_W  409
#define CARD_H  88
#define CARD_Y0 308
#define CARD_DY 92

static lv_obj_t *s_root, *s_check, *s_check_icon, *s_terms;
static lv_span_t *s_span[5];
static bool s_checked;
static lv_timer_t *s_attn_timer;

static void set_checked(bool on, bool animate)
{
    s_checked = on;
    ui_set_selected(s_check, on);
    ui_fade_indicator(s_check_icon, on, animate);
}

static void refresh_terms(void *user)
{
    (void)user;
    static const adv_txt_t keys[5] = {TXT_ACCEPT_PREFIX, TXT_ACCEPT_LINK_PRIVACY, TXT_ACCEPT_MID,
                                      TXT_ACCEPT_LINK_TERMS, TXT_ACCEPT_SUFFIX};
    if (!s_terms) {
        return;                    /* pantalla ya destruida */
    }
    const ui_palette_t *p = ui_theme_palette();
    for (int i = 0; i < 5; i++) {
        lv_span_set_text_static(s_span[i], ui_tr(keys[i]));
        lv_style_set_text_color(lv_span_get_style(s_span[i]), (i == 1 || i == 3) ? p->accent_text : p->text);
    }
    lv_spangroup_refr_mode(s_terms);
}

static void on_toggle(lv_event_t *e)
{
    (void)e;
    set_checked(!s_checked, true);
}

static void attn_end(lv_timer_t *t)
{
    lv_obj_remove_local_style_prop(s_check, LV_STYLE_BORDER_COLOR, 0);
    lv_obj_remove_local_style_prop(s_check, LV_STYLE_BORDER_WIDTH, 0);
    lv_timer_delete(t);
    s_attn_timer = NULL;
}

static void on_check_deleted(lv_event_t *e)
{
    (void)e;
    s_terms = NULL;
    s_root = NULL;
    if (s_attn_timer) {
        lv_timer_delete(s_attn_timer);
        s_attn_timer = NULL;
    }
}

static void on_accept(lv_event_t *e)
{
    (void)e;
    if (!s_checked) {
        /* Respuesta sutil: el borde de la casilla toma el color de acento. */
        if (!s_attn_timer) {
            lv_obj_set_style_border_color(s_check, ui_theme_palette()->accent, 0);
            lv_obj_set_style_border_width(s_check, 3, 0);
            s_attn_timer = lv_timer_create(attn_end, 600, NULL);
        }
        return;
    }
    if (ui_wizard_busy()) {
        return;
    }
    adv_settings_t *s = adv_settings_edit();
    s->privacy_accepted = 1;
    s->legal_version_accepted = ADV_LEGAL_BUNDLE_VERSION;
    adv_settings_commit();
    ui_wizard_go(UI_SCR_PERSONALIZE, +1);
}

static void on_later(lv_event_t *e)
{
    (void)e;
    if (ui_wizard_go(UI_SCR_WELCOME, -1)) {
        /* No se registra nada: la casilla vuelve a lo que estaba guardado. */
        set_checked(adv_settings_get()->privacy_accepted, false);
    }
}

static void on_card(lv_event_t *e)
{
    if (ui_wizard_busy()) {
        return;
    }
    ui_legal_sheet_open(s_root, (int)(uintptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *make_card(lv_obj_t *parent, int idx, const lv_image_dsc_t *icon, adv_txt_t t, adv_txt_t d)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_add_style(c, &ui_st.card, 0);
    lv_obj_add_style(c, &ui_st.card_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(c, CARD_W, CARD_H);
    lv_obj_set_pos(c, CARD_X, CARD_Y0 + idx * CARD_DY);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(c, on_card, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);

    lv_obj_t *tile = lv_obj_create(c);
    lv_obj_remove_style_all(tile);
    lv_obj_add_style(tile, &ui_st.tile, 0);
    lv_obj_set_size(tile, 52, 52);
    lv_obj_align(tile, LV_ALIGN_LEFT_MID, 13, 0);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ic = lv_image_create(tile);
    lv_image_set_src(ic, icon);
    lv_obj_add_style(ic, &ui_st.icon_accent, 0);
    lv_obj_center(ic);

    lv_obj_t *tl = ui_label(c, &ui_st.section, t);
    lv_obj_set_pos(tl, 78, 7);
    lv_obj_t *dl = ui_label(c, &ui_st.small, d);
    lv_obj_set_width(dl, 302);
    lv_obj_set_pos(dl, 78, 30);

    lv_obj_t *chev = lv_image_create(c);
    lv_image_set_src(chev, &img_ic_chevron);
    lv_obj_add_style(chev, &ui_st.icon_secondary, 0);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -10, 0);
    return c;
}

lv_obj_t *ui_scr_privacy_create(lv_obj_t *parent)
{
    lv_obj_t *root = ui_screen_root(parent);
    s_root = root;
    ui_lockup(root, &img_lockup_md, 85);
    lv_obj_t *title = ui_label(root, &ui_st.title, TXT_PRIVACY_TITLE);
    ui_test_register("privacy.title", title);
    lv_obj_set_width(title, 450);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 199);
    lv_obj_t *sub = ui_label(root, &ui_st.subtitle, TXT_PRIVACY_SUBTITLE);
    lv_obj_set_width(sub, 410);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 239);

    ui_test_register("privacy.card.privacy", make_card(root, 0, &img_ic_shield, TXT_PRIVACY_ITEM_TITLE, TXT_PRIVACY_ITEM_DESC));
    ui_test_register("privacy.card.perms", make_card(root, 1, &img_ic_layers, TXT_PERMS_ITEM_TITLE, TXT_PERMS_ITEM_DESC));
    ui_test_register("privacy.card.terms", make_card(root, 2, &img_ic_doc, TXT_TERMS_ITEM_TITLE, TXT_TERMS_ITEM_DESC));

    /* fila de aceptacion: toda la fila (casilla + texto) alterna la casilla */
    lv_obj_t *acc = lv_obj_create(root);
    lv_obj_remove_style_all(acc);
    lv_obj_set_size(acc, CARD_W, 52);
    lv_obj_set_pos(acc, CARD_X, 590);
    lv_obj_add_flag(acc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(acc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(acc, on_toggle, LV_EVENT_CLICKED, NULL);
    ui_test_register("privacy.accept_row", acc);

    s_check = lv_obj_create(acc);
    lv_obj_remove_style_all(s_check);
    lv_obj_add_style(s_check, &ui_st.checkbox, 0);
    lv_obj_add_style(s_check, &ui_st.checkbox_checked, LV_STATE_CHECKED);
    lv_obj_set_size(s_check, 26, 26);
    lv_obj_align(s_check, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_remove_flag(s_check, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_check, on_check_deleted, LV_EVENT_DELETE, NULL);
    s_check_icon = lv_image_create(s_check);
    lv_image_set_src(s_check_icon, &img_ic_check);
    lv_obj_add_style(s_check_icon, &ui_st.icon_on_accent, 0);
    lv_obj_center(s_check_icon);

    s_terms = lv_spangroup_create(acc);
    lv_obj_add_style(s_terms, &ui_st.body, 0);
    lv_obj_set_width(s_terms, CARD_W - 50);
    lv_spangroup_set_mode(s_terms, LV_SPAN_MODE_BREAK);
    lv_obj_align(s_terms, LV_ALIGN_LEFT_MID, 47, 0);
    lv_obj_remove_flag(s_terms, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 5; i++) {
        s_span[i] = lv_spangroup_new_span(s_terms);
    }
    refresh_terms(NULL);
    ui_i18n_listen(refresh_terms, NULL);

    lv_obj_t *btn = ui_primary_button(root, TXT_BTN_ACCEPT_CONTINUE, 331, 668, on_accept, NULL);
    ui_test_register("privacy.accept", btn);
    lv_obj_t *later = ui_link_button(root, TXT_BTN_LATER, 732, on_later, NULL);
    ui_test_register("privacy.later", later);

    set_checked(adv_settings_get()->privacy_accepted, false);
    return root;
}

void ui_scr_privacy_on_show(void)
{
    /* Se conserva lo que el usuario dejo marcado al ir y volver; solo "Mas
     * tarde" lo devuelve al estado guardado. */
}

bool ui_scr_privacy_checked(void)
{
    return s_checked;
}
