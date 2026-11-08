/*
 * ADVENYA OS · panel de documento legal (se abre desde privacidad)
 *
 * Es un panel inferior sobre la pantalla de privacidad, no una pantalla nueva
 * del asistente. Muestra el documento oficial si esta incorporado
 * (legal/legal_docs.c) o, si no, el aviso traducido de "documento pendiente".
 * Se cierra con "Cerrar", tocando fuera o con el gesto de retroceso.
 */
#include "ui_internal.h"
#include "legal/legal_docs.h"

#define SHEET_W   452
#define SHEET_H   560
#define OPEN_MS   300
#define CLOSE_MS  220

static lv_obj_t *s_scrim, *s_sheet;
static bool s_closing;

static void opa_exec(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void ty_exec(void *var, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0);
}

static void anim(lv_obj_t *obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                 lv_anim_path_cb_t path, lv_anim_completed_cb_t done)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, path);
    if (done) {
        lv_anim_set_completed_cb(&a, done);
    }
    lv_anim_start(&a);
}

static void closed(lv_anim_t *a)
{
    (void)a;
    if (s_scrim) {
        lv_obj_delete(s_scrim);          /* el panel es hijo del velo */
    }
    s_scrim = s_sheet = NULL;
    s_closing = false;
}

bool ui_legal_sheet_close(void)
{
    if (!s_scrim) {
        return false;
    }
    if (!s_closing) {
        s_closing = true;
        anim(s_sheet, ty_exec, 0, 48, CLOSE_MS, lv_anim_path_ease_in, NULL);
        anim(s_scrim, opa_exec, 255, 0, CLOSE_MS, lv_anim_path_ease_in, closed);
    }
    return true;
}

static void on_close(lv_event_t *e)
{
    (void)e;
    ui_legal_sheet_close();
}

static void on_scrim(lv_event_t *e)
{
    if (lv_event_get_target(e) == s_scrim) {   /* solo fuera del panel */
        ui_legal_sheet_close();
    }
}

static void on_gesture(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_RIGHT || dir == LV_DIR_BOTTOM) {
        lv_indev_wait_release(lv_indev_active());
        ui_legal_sheet_close();
    }
}

void ui_legal_sheet_open(lv_obj_t *parent, int doc)
{
    if (s_scrim || doc < 0 || doc >= ADV_LEGAL_DOC_COUNT) {
        return;
    }
    const adv_legal_doc_t *d = &adv_legal_docs[doc];
    const uint8_t lang = ui_i18n_language();
    const bool have_text = d->text[lang] != NULL;

    s_scrim = lv_obj_create(parent);
    lv_obj_remove_style_all(s_scrim);
    lv_obj_add_style(s_scrim, &ui_st.scrim, 0);
    lv_obj_set_size(s_scrim, UI_W, UI_H);
    lv_obj_set_pos(s_scrim, 0, 0);
    lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_scrim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_scrim, on_scrim, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_scrim, on_gesture, LV_EVENT_GESTURE, NULL);

    s_sheet = lv_obj_create(s_scrim);
    lv_obj_remove_style_all(s_sheet);
    lv_obj_add_style(s_sheet, &ui_st.sheet, 0);
    lv_obj_set_size(s_sheet, SHEET_W, SHEET_H);
    lv_obj_align(s_sheet, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_CLICKABLE);         /* no cierra al tocar dentro */
    lv_obj_remove_flag(s_sheet, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *title = lv_label_create(s_sheet);
    lv_obj_add_style(title, &ui_st.section, 0);
    lv_obj_set_style_text_font(title, &adv_font_sb22, 0);
    lv_label_set_text_static(title, ui_tr(d->title));
    lv_obj_set_width(title, SHEET_W - 48);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *status = lv_label_create(s_sheet);
    lv_obj_add_style(status, &ui_st.small, 0);
    if (d->version) {
        lv_label_set_text_static(status, d->version);
    } else {
        lv_label_set_text_static(status, ui_tr(TXT_LEGAL_STATUS_PENDING));
    }
    lv_obj_align(status, LV_ALIGN_TOP_LEFT, 0, 36);

    /* cuerpo desplazable (los documentos oficiales seran largos) */
    lv_obj_t *body = lv_obj_create(s_sheet);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, SHEET_W - 48, SHEET_H - 48 - 70 - 66);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, 70);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_t *txt = lv_label_create(body);
    lv_obj_add_style(txt, &ui_st.body, 0);
    lv_obj_set_width(txt, SHEET_W - 52);
    lv_label_set_text_static(txt, have_text ? d->text[lang] : ui_tr(TXT_LEGAL_PENDING_BODY));

    lv_obj_t *close = lv_obj_create(s_sheet);
    lv_obj_remove_style_all(close);
    lv_obj_add_style(close, &ui_st.sheet_btn, 0);
    lv_obj_add_style(close, &ui_st.sheet_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(close, 200, 46);
    lv_obj_align(close, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(close, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close, on_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cl = lv_label_create(close);
    lv_label_set_text_static(cl, ui_tr(TXT_BTN_CLOSE));
    lv_obj_center(cl);
    ui_test_register("legal.close", close);

    lv_obj_set_style_opa(s_scrim, LV_OPA_TRANSP, 0);
    anim(s_scrim, opa_exec, 0, 255, OPEN_MS, lv_anim_path_ease_out, NULL);
    anim(s_sheet, ty_exec, 60, 0, OPEN_MS, lv_anim_path_ease_out, NULL);
}
