/*
 * ADVENYA OS · traducciones centralizadas
 *
 * Los textos viven en translations/<idioma>.json y se compilan a una tabla
 * (i18n/ui_i18n_gen.c). Cada etiqueta se "enlaza" a una clave; al cambiar el
 * idioma se reescriben todas las etiquetas enlazadas en el acto, en todas las
 * pantallas ya creadas.
 */
#include <string.h>
#include "ui_internal.h"

#define MAX_BINDS     64
#define MAX_LISTENERS 8

typedef struct {
    lv_obj_t *label;
    adv_txt_t key;
} bind_t;

static bind_t s_binds[MAX_BINDS];
static size_t s_nbinds;
static struct {
    ui_i18n_listener_t fn;
    void *user;
} s_listeners[MAX_LISTENERS];
static size_t s_nlisteners;
static uint8_t s_lang;

void ui_i18n_init(uint8_t lang)
{
    s_lang = lang < ADV_LANG_COUNT ? lang : 0;
    s_nbinds = 0;
    s_nlisteners = 0;
}

uint8_t ui_i18n_language(void)
{
    return s_lang;
}

const char *ui_tr(adv_txt_t key)
{
    return key < TXT_COUNT ? adv_i18n_table[s_lang][key] : "";
}

static void on_label_deleted(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    for (size_t i = 0; i < s_nbinds; i++) {
        if (s_binds[i].label == obj) {
            s_binds[i] = s_binds[--s_nbinds];
            return;
        }
    }
}

void ui_i18n_bind(lv_obj_t *label, adv_txt_t key)
{
    lv_label_set_text_static(label, ui_tr(key));
    for (size_t i = 0; i < s_nbinds; i++) {
        if (s_binds[i].label == label) {
            s_binds[i].key = key;
            return;
        }
    }
    LV_ASSERT_MSG(s_nbinds < MAX_BINDS, "ui_i18n: demasiadas etiquetas enlazadas");
    if (s_nbinds < MAX_BINDS) {
        s_binds[s_nbinds++] = (bind_t){label, key};
        lv_obj_add_event_cb(label, on_label_deleted, LV_EVENT_DELETE, NULL);
    }
}

void ui_i18n_listen(ui_i18n_listener_t fn, void *user)
{
    if (s_nlisteners < MAX_LISTENERS) {
        s_listeners[s_nlisteners].fn = fn;
        s_listeners[s_nlisteners].user = user;
        s_nlisteners++;
    }
}

void ui_i18n_refresh_all(void)
{
    for (size_t i = 0; i < s_nbinds; i++) {
        lv_label_set_text_static(s_binds[i].label, ui_tr(s_binds[i].key));
    }
    for (size_t i = 0; i < s_nlisteners; i++) {
        s_listeners[i].fn(s_listeners[i].user);
    }
}

void ui_i18n_set_language(uint8_t lang)
{
    if (lang >= ADV_LANG_COUNT || lang == s_lang) {
        return;
    }
    s_lang = lang;
    ui_i18n_refresh_all();
}
