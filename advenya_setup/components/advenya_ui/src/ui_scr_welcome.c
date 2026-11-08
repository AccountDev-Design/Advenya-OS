/*
 * ADVENYA OS · pantalla 1: bienvenida (assets/reference/ref_01_bienvenida.png)
 * Identidad ADVENYA, "¡Bienvenido!" y boton "Comenzar".
 */
#include "ui_internal.h"

static void on_start(lv_event_t *e)
{
    (void)e;
    ui_wizard_go(UI_SCR_LANGUAGE, +1);
}

lv_obj_t *ui_scr_welcome_create(lv_obj_t *parent)
{
    lv_obj_t *root = ui_screen_root(parent);
    ui_lockup(root, &img_lockup_lg, 226);
    lv_obj_t *title = ui_label(root, &ui_st.title_welcome, TXT_WELCOME_TITLE);
    ui_test_register("welcome.title", title);
    lv_obj_set_width(title, 440);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 476);
    lv_obj_t *btn = ui_primary_button(root, TXT_BTN_START, 316, 601, on_start, NULL);
    ui_test_register("welcome.start", btn);
    return root;
}
