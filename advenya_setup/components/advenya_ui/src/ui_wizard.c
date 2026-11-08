/*
 * ADVENYA OS · navegacion del asistente ("Fluid Motion")
 *
 * Avance:     la pantalla actual sale hacia la izquierda (-20 px) con fundido
 *             (260 ms, ease in) y la siguiente entra desde la derecha
 *             (+20 px -> 0, 300 ms, ease out) con 110 ms de solape: 410 ms.
 * Retroceso:  lo mismo en espejo.
 * Durante una transicion se ignora cualquier otra navegacion.
 * Sin indicadores de progreso.
 *
 * Retroceso: gesto de deslizar hacia la derecha (las referencias no tienen
 * boton de atras) y "Mas tarde" en privacidad.
 *
 * Las pantallas se crean la primera vez que se visitan y se conservan (no se
 * recrean al ir y volver). Al terminar se destruyen todas.
 */
#include "ui_internal.h"
#include "ui_platform.h"
#include "advenya_handoff.h"

typedef lv_obj_t *(*create_fn_t)(lv_obj_t *);
typedef void (*show_fn_t)(void);

static const create_fn_t s_create[UI_SCR_COUNT] = {
    ui_scr_welcome_create, ui_scr_language_create, ui_scr_privacy_create, ui_scr_personalize_create,
};
static const show_fn_t s_on_show[UI_SCR_COUNT] = {
    NULL, ui_scr_language_on_show, ui_scr_privacy_on_show, ui_scr_personalize_on_show,
};
static const adv_stage_t s_stage_of[UI_SCR_COUNT] = {
    ADV_STAGE_WELCOME, ADV_STAGE_LANGUAGE, ADV_STAGE_PRIVACY, ADV_STAGE_PERSONALIZE,
};

static lv_obj_t *s_host;
static lv_obj_t *s_scr[UI_SCR_COUNT];
static ui_scr_t s_cur = UI_SCR_WELCOME;
static bool s_busy, s_finished;
static lv_obj_t *s_leaving;
static lv_timer_t *s_poll;

static lv_obj_t *get_scr(ui_scr_t id)
{
    if (!s_scr[id]) {
        s_scr[id] = s_create[id](s_host);
        lv_obj_add_flag(s_scr[id], LV_OBJ_FLAG_HIDDEN);
    }
    return s_scr[id];
}

static void opa_exec(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void tx_exec(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}

static void run(lv_obj_t *obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms, uint32_t delay,
                lv_anim_path_cb_t path, lv_anim_completed_cb_t done)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, path);
    lv_anim_set_early_apply(&a, true);
    if (done) {
        lv_anim_set_completed_cb(&a, done);
    }
    lv_anim_start(&a);
}

static void enter_stage(ui_scr_t id)
{
    adv_settings_t *s = adv_settings_edit();
    s->stage = s_stage_of[id];
    adv_settings_commit();                 /* punto de recuperacion */
}

static void out_done(lv_anim_t *a)
{
    (void)a;
    if (s_leaving) {
        lv_obj_add_flag(s_leaving, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(s_leaving, LV_OPA_COVER, 0);
        lv_obj_set_style_translate_x(s_leaving, 0, 0);
        s_leaving = NULL;
    }
}

static void in_done(lv_anim_t *a)
{
    (void)a;
    s_busy = false;
}

bool ui_wizard_busy(void)
{
    return s_busy || s_finished;
}

ui_scr_t ui_wizard_current(void)
{
    return s_cur;
}

bool ui_wizard_go(ui_scr_t target, int dir)
{
    if (s_busy || s_finished || target == s_cur || target >= UI_SCR_COUNT) {
        return false;
    }
    s_busy = true;
    lv_obj_t *from = s_scr[s_cur];
    lv_obj_t *to = get_scr(target);
    if (s_on_show[target]) {
        s_on_show[target]();
    }
    const int32_t sh = UI_NAV_SHIFT_PX * (dir >= 0 ? 1 : -1);
    s_leaving = from;
    run(from, tx_exec, 0, -sh, UI_NAV_OUT_MS, 0, lv_anim_path_ease_in, NULL);
    run(from, opa_exec, 255, 0, UI_NAV_OUT_MS, 0, lv_anim_path_ease_in, out_done);

    lv_obj_set_style_opa(to, LV_OPA_TRANSP, 0);
    lv_obj_set_style_translate_x(to, sh, 0);
    lv_obj_remove_flag(to, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(to);
    run(to, tx_exec, sh, 0, UI_NAV_IN_MS, UI_NAV_IN_DELAY_MS, lv_anim_path_ease_out, NULL);
    run(to, opa_exec, 0, 255, UI_NAV_IN_MS, UI_NAV_IN_DELAY_MS, lv_anim_path_ease_out, in_done);

    s_cur = target;
    enter_stage(target);
    return true;
}

static void on_gesture(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_active();
    if (lv_indev_get_gesture_dir(indev) != LV_DIR_RIGHT) {
        return;
    }
    /* El dedo pudo empezar sobre una fila o un boton: al soltar NO debe
     * contar como toque (LVGL envia PRESS_LOST en lugar de CLICKED). */
    lv_indev_wait_release(indev);
    if (ui_legal_sheet_close()) {
        return;
    }
    if (s_cur > UI_SCR_WELCOME) {
        ui_wizard_go((ui_scr_t)(s_cur - 1), -1);
    }
}

static void poll_cb(lv_timer_t *t)
{
    (void)t;
    adv_settings_poll();                    /* escritura diferida de elecciones */
}

void ui_wizard_init(lv_obj_t *host)
{
    s_host = host;
    s_busy = s_finished = false;
    for (int i = 0; i < UI_SCR_COUNT; i++) {
        s_scr[i] = NULL;
    }
    lv_obj_add_event_cb(host, on_gesture, LV_EVENT_GESTURE, NULL);
    s_poll = lv_timer_create(poll_cb, 250, NULL);
}

void ui_wizard_show_first(ui_scr_t scr)
{
    s_cur = scr;
    lv_obj_t *o = get_scr(scr);
    if (s_on_show[scr]) {
        s_on_show[scr]();
    }
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    enter_stage(scr);
}

/* ---------------------------------------------------------------- final */

static void release_setup_ui(void)
{
    ui_boot_release();
    ui_backdrop_release();
    ui_platform_log_mem("tras liberar la interfaz del asistente");
}

static void destroy_screens(lv_anim_t *a)
{
    (void)a;
    for (int i = 0; i < UI_SCR_COUNT; i++) {
        if (s_scr[i]) {
            lv_obj_delete(s_scr[i]);
            s_scr[i] = NULL;
        }
    }
    ui_platform_log_mem("pantallas del asistente destruidas");
    ui_core_handoff(ADV_HANDOFF_SETUP_JUST_COMPLETED, release_setup_ui);
}

void ui_wizard_finish(bool deferred)
{
    if (s_busy || s_finished) {
        return;
    }
    s_finished = true;
    if (s_poll) {
        lv_timer_delete(s_poll);
        s_poll = NULL;
    }
    lv_obj_remove_event_cb(s_host, on_gesture);
    adv_settings_t *s = adv_settings_edit();
    s->personalization_deferred = deferred ? 1 : 0;
    s->setup_complete = 1;
    s->stage = ADV_STAGE_COMPLETE;
    adv_settings_commit();
    /* El contenido del asistente se funde y queda solo el fondo; el nucleo
     * recibe el control cuando las pantallas ya estan destruidas. */
    lv_obj_t *cur = s_scr[s_cur];
    run(cur, opa_exec, 255, 0, UI_BOOT_FADE_MS, 0, lv_anim_path_ease_in_out, destroy_screens);
}

void ui_wizard_release_for_core(void)
{
    release_setup_ui();
}
