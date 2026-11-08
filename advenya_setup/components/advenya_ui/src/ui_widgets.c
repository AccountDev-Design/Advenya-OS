/* ADVENYA OS · piezas comunes de las pantallas del asistente. */
#include <string.h>
#include "ui_internal.h"

#define BTN_H 54

lv_obj_t *ui_screen_root(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, &ui_st.screen, 0);
    lv_obj_set_size(o, UI_W, UI_H);
    lv_obj_set_pos(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t *ui_lockup(lv_obj_t *parent, const lv_image_dsc_t *img, int32_t y)
{
    lv_obj_t *o = lv_image_create(parent);
    lv_image_set_src(o, img);
    lv_obj_add_style(o, &ui_st.logo, 0);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, lv_style_t *style, adv_txt_t key)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_add_style(l, style, 0);
    ui_i18n_bind(l, key);
    return l;
}

/* Ajuste de la etiqueta del boton principal a textos largos (p. ej. aleman):
 * centrada si cabe entre las dos zonas reservadas a la flecha; si no, se
 * desplaza a la izquierda y, en ultimo caso, baja de tamano. */
#define MAX_BUTTONS 8
static lv_obj_t *s_buttons[MAX_BUTTONS];
static size_t s_nbuttons;

static void fit_label(lv_obj_t *btn)
{
    lv_obj_t *l = lv_obj_get_child(btn, 0);
    const char *txt = lv_label_get_text(l);
    const int32_t w = lv_obj_get_style_width(btn, 0);   /* valor fijado (el layout aun no corre) */
    static const lv_font_t *const fonts[] = {&adv_font_sb22, &adv_font_sb20, &adv_font_m16};
    lv_point_t sz;
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        lv_text_get_size(&sz, txt, fonts[i], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (sz.x <= w - 2 * 58) {
            lv_obj_set_style_text_font(l, fonts[i], 0);
            lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
            return;
        }
        if (sz.x <= w - 58 - 22) {
            lv_obj_set_style_text_font(l, fonts[i], 0);
            lv_obj_align(l, LV_ALIGN_LEFT_MID, 22 + (w - 58 - 22 - sz.x) / 2, 0);
            return;
        }
    }
    lv_obj_set_style_text_font(l, fonts[2], 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 22, 0);
}

static void refit_all(void *user)
{
    (void)user;
    for (size_t i = 0; i < s_nbuttons; i++) {
        fit_label(s_buttons[i]);
    }
}

static void on_button_deleted(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    for (size_t i = 0; i < s_nbuttons; i++) {
        if (s_buttons[i] == b) {
            s_buttons[i] = s_buttons[--s_nbuttons];
            return;
        }
    }
}

lv_obj_t *ui_primary_button(lv_obj_t *parent, adv_txt_t key, int32_t w, int32_t y,
                            lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &ui_st.btn, 0);
    lv_obj_add_style(b, &ui_st.btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(b, &ui_st.btn_disabled, LV_STATE_DISABLED);
    lv_obj_set_size(b, w, BTN_H);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = ui_label(b, &ui_st.btn_label, key);
    lv_obj_center(l);
    lv_obj_t *arrow = lv_image_create(b);
    lv_image_set_src(arrow, &img_ic_arrow);
    lv_obj_add_style(arrow, &ui_st.icon_on_accent, 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -24, 0);
    if (cb) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    }
    if (s_nbuttons == 0) {
        ui_i18n_listen(refit_all, NULL);
    }
    if (s_nbuttons < MAX_BUTTONS) {
        s_buttons[s_nbuttons++] = b;
        lv_obj_add_event_cb(b, on_button_deleted, LV_EVENT_DELETE, NULL);
    }
    fit_label(b);
    return b;
}

lv_obj_t *ui_link_button(lv_obj_t *parent, adv_txt_t key, int32_t y, lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &ui_st.link, 0);
    lv_obj_add_style(b, &ui_st.link_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(b);
    ui_i18n_bind(l, key);
    if (cb) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    }
    return b;
}

lv_obj_t *ui_check_badge(lv_obj_t *parent, int32_t size)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &ui_st.badge, 0);
    lv_obj_set_size(b, size, size);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ic = lv_image_create(b);
    lv_image_set_src(ic, &img_ic_check);
    lv_obj_add_style(ic, &ui_st.icon_on_accent, 0);
    if (size < 24) {
        lv_image_set_scale(ic, 200);
    }
    lv_obj_center(ic);
    return b;
}

void ui_set_selected(lv_obj_t *obj, bool selected)
{
    if (selected) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(obj, LV_STATE_CHECKED);
    }
}

/* Aparicion/desaparicion suave de un indicador de seleccion (150..220 ms). */
static void opa_exec(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

void ui_fade_indicator(lv_obj_t *obj, bool show, bool animate)
{
    lv_anim_delete(obj, opa_exec);
    lv_opa_t target = show ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (!animate) {
        lv_obj_set_style_opa(obj, target, 0);
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, opa_exec);
    lv_anim_set_values(&a, lv_obj_get_style_opa(obj, 0), target);
    lv_anim_set_duration(&a, UI_SEL_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void ui_add_feedback(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &ui_st.card_pressed, LV_STATE_PRESSED);
}

/* ---- registro de identificadores para las pruebas del simulador ---- */
#define MAX_TEST_IDS 64
static struct {
    const char *id;
    lv_obj_t *obj;
} s_ids[MAX_TEST_IDS];
static size_t s_nids;

static void on_test_obj_deleted(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    for (size_t i = 0; i < s_nids; i++) {
        if (s_ids[i].obj == obj) {
            s_ids[i] = s_ids[--s_nids];
            return;
        }
    }
}

void ui_test_register(const char *id, lv_obj_t *obj)
{
    if (s_nids < MAX_TEST_IDS) {
        s_ids[s_nids].id = id;
        s_ids[s_nids].obj = obj;
        s_nids++;
        lv_obj_add_event_cb(obj, on_test_obj_deleted, LV_EVENT_DELETE, NULL);
    }
}

lv_obj_t *ui_test_find(const char *id)
{
    for (size_t i = 0; i < s_nids; i++) {
        if (strcmp(s_ids[i].id, id) == 0) {
            return s_ids[i].obj;
        }
    }
    return NULL;
}
