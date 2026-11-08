/*
 * ADVENYA OS · animacion de arranque del logotipo (~3,0 s)
 *
 * Storyboard (assets/reference/ref_05_storyboard_boot.png):
 *   0,0-0,6 s  aparicion de la A: se revela desde su centro con fundido y un
 *              ligero brillo azul que se asienta en blanco       (ease out)
 *   0,6-1,2 s  las tres ondas se dibujan de izquierda a derecha con trazo
 *              luminoso                                           (ease in-out)
 *   1,2-1,8 s  la orbita se traza sobre su elipse y termina en la estrella
 *   1,8-2,4 s  la estrella aparece con un destello breve          (ease out)
 *   2,4-3,0 s  logotipo completo y estable
 *   final      fundido de 300 ms a la bienvenida (o se queda fijo hasta que
 *              el nucleo tome el control).
 *
 * Cada capa es un subconjunto exacto de los pixeles del logotipo original
 * (tools/gen_assets.py). El revelado se calcula por umbral sobre un mapa de
 * orden precalculado: sin geometria en tiempo de ejecucion, unos 50 k pixeles
 * por capa activa y fotograma. Desde 2,4 s se muestra la imagen completa del
 * logotipo original, de modo que el resultado final es identico a la
 * referencia. No hay esperas bloqueantes: todo va en una lv_anim.
 */
#include <math.h>
#include <string.h>
#include "ui_internal.h"
#include "ui_platform.h"

#define BOOT_TOTAL_MS  3000
#define LAYERS         4
#define SOFT           34          /* anchura del borde de revelado (0..255) */
#define GLOW_COLOR     0x4DA3FF

typedef struct {
    const lv_image_dsc_t *alpha;
    const uint8_t *order;
    uint8_t *buf;
    lv_image_dsc_t dsc;
    lv_obj_t *img;
    int32_t last_p;                /* 0..1024, -1 = nunca calculado */
} boot_layer_t;

static boot_layer_t s_l[LAYERS];
static lv_obj_t *s_overlay, *s_logo, *s_full, *s_glow;
static ui_boot_done_cb_t s_at_end;
static bool s_active, s_full_shown;

static float clamp01(float v)
{
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

static float ease_out(float x)
{
    float k = 1.f - x;
    return 1.f - k * k * k;
}

static float ease_in_out(float x)
{
    if (x < 0.5f) {
        return 4.f * x * x * x;
    }
    float k = -2.f * x + 2.f;
    return 1.f - k * k * k / 2.f;
}

static void reveal(boot_layer_t *l, int32_t p)
{
    if (p == l->last_p || !l->buf) {
        return;
    }
    l->last_p = p;
    const size_t n = (size_t)ADV_BOOT_LOGO_W * ADV_BOOT_LOGO_H;
    const uint8_t *a = l->alpha->data;
    if (p <= 0) {
        memset(l->buf, 0, n);
    } else if (p >= 1024) {
        memcpy(l->buf, a, n);
    } else {
        const int32_t th = p * (255 + SOFT) / 1024;
        const uint8_t *o = l->order;
        for (size_t i = 0; i < n; i++) {
            int32_t v = (th - (int32_t)o[i]) * 256 / SOFT;
            v = v < 0 ? 0 : (v > 256 ? 256 : v);
            l->buf[i] = (uint8_t)((a[i] * v) >> 8);
        }
    }
    lv_image_cache_drop(&l->dsc);
    lv_obj_invalidate(l->img);
}

static lv_color_t glow_to_white(float k)
{
    return lv_color_mix(lv_color_hex(0xFFFFFF), lv_color_hex(GLOW_COLOR), (uint8_t)(clamp01(k) * 255));
}

static void boot_update(int32_t t)
{
    if (t >= 2400) {
        if (!s_full_shown) {
            s_full_shown = true;
            lv_obj_remove_flag(s_full, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < LAYERS; i++) {
                lv_obj_add_flag(s_l[i].img, LV_OBJ_FLAG_HIDDEN);
            }
            lv_obj_add_flag(s_glow, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    /* 1) A */
    float pa = ease_out(clamp01(t / 600.f));
    reveal(&s_l[0], (int32_t)(pa * 1024));
    lv_obj_set_style_image_opa(s_l[0].img, (lv_opa_t)(40 + pa * 215), 0);
    lv_obj_set_style_image_recolor(s_l[0].img, glow_to_white((t - 150) / 650.f), 0);
    /* 2) ondas */
    float pw = ease_in_out(clamp01((t - 600) / 600.f));
    reveal(&s_l[1], (int32_t)(pw * 1024));
    lv_obj_set_style_image_recolor(s_l[1].img, glow_to_white((t - 900) / 600.f), 0);
    /* 3) orbita */
    float po = ease_in_out(clamp01((t - 1200) / 600.f));
    reveal(&s_l[2], (int32_t)(po * 1024));
    lv_obj_set_style_image_recolor(s_l[2].img, glow_to_white((t - 1500) / 500.f), 0);
    /* 4) estrella + destello */
    float ps = ease_out(clamp01((t - 1800) / 260.f));
    reveal(&s_l[3], (int32_t)(ps * 1024));
    lv_obj_set_style_image_recolor(s_l[3].img, glow_to_white((t - 1950) / 400.f), 0);
    float g = clamp01((t - 1800) / 560.f);
    if (g > 0.f && g < 1.f) {
        lv_obj_remove_flag(s_glow, LV_OBJ_FLAG_HIDDEN);
        float rise = g < 0.25f ? g / 0.25f : 1.f - (g - 0.25f) / 0.75f;    /* destello breve */
        lv_obj_set_style_image_opa(s_glow, (lv_opa_t)(ease_out(clamp01(rise)) * 230), 0);
        lv_image_set_scale(s_glow, (uint32_t)(256 * (0.35f + 0.95f * ease_out(g))));
    } else {
        lv_obj_add_flag(s_glow, LV_OBJ_FLAG_HIDDEN);
    }
}

static void anim_exec(void *var, int32_t v)
{
    (void)var;
    boot_update(v);
}

static void anim_done(lv_anim_t *a)
{
    (void)a;
    boot_update(BOOT_TOTAL_MS);
    if (s_at_end) {
        s_at_end();
    }
}

static lv_obj_t *layer_img(lv_obj_t *parent, lv_image_dsc_t *dsc)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, dsc);
    lv_obj_set_pos(img, 0, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(img, lv_color_hex(GLOW_COLOR), 0);
    return img;
}

void ui_boot_start(lv_obj_t *layer, ui_boot_done_cb_t at_end)
{
    static const lv_image_dsc_t *const alphas[LAYERS] = {&img_boot_a, &img_boot_waves, &img_boot_orbit, &img_boot_star};
    static const uint8_t *const orders[LAYERS] = {boot_order_a, boot_order_waves, boot_order_orbit, boot_order_star};
    s_at_end = at_end;
    s_full_shown = false;
    s_active = true;

    s_overlay = lv_obj_create(layer);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, UI_W, UI_H);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);      /* absorbe toques */
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    s_logo = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_logo);
    lv_obj_set_size(s_logo, ADV_BOOT_LOGO_W, ADV_BOOT_LOGO_H);
    lv_obj_center(s_logo);
    lv_obj_remove_flag(s_logo, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    const size_t n = (size_t)ADV_BOOT_LOGO_W * ADV_BOOT_LOGO_H;
    /* orden de apilado igual que el logotipo: orbita detras, luego A, ondas, estrella */
    static const int z[LAYERS] = {2, 0, 1, 3};
    for (int k = 0; k < LAYERS; k++) {
        int i = z[k];
        boot_layer_t *l = &s_l[i];
        l->alpha = alphas[i];
        l->order = orders[i];
        l->last_p = -1;
        l->buf = ui_platform_alloc_large(n);
        if (!l->buf) {
            LV_LOG_ERROR("boot: sin memoria para la capa %d", i);
            l->buf = NULL;
        }
        memset(&l->dsc, 0, sizeof(l->dsc));
        l->dsc.header = alphas[i]->header;
        l->dsc.data_size = (uint32_t)n;
        l->dsc.data = l->buf ? l->buf : alphas[i]->data;
        if (l->buf) {
            memset(l->buf, 0, n);
        }
        l->img = layer_img(s_logo, &l->dsc);
    }
    s_full = lv_image_create(s_logo);
    lv_image_set_src(s_full, &img_logo_mark);
    lv_obj_set_style_image_recolor(s_full, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_image_recolor_opa(s_full, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_full, LV_OBJ_FLAG_HIDDEN);

    s_glow = lv_image_create(s_overlay);
    lv_image_set_src(s_glow, &img_glow);
    lv_obj_set_style_image_recolor(s_glow, lv_color_hex(0xBFE0FF), 0);
    lv_obj_set_style_image_recolor_opa(s_glow, LV_OPA_COVER, 0);
    lv_image_set_pivot(s_glow, img_glow.header.w / 2, img_glow.header.h / 2);
    lv_obj_set_pos(s_glow, (UI_W - ADV_BOOT_LOGO_W) / 2 + ADV_BOOT_STAR_X - img_glow.header.w / 2,
                   (UI_H - ADV_BOOT_LOGO_H) / 2 + ADV_BOOT_STAR_Y - img_glow.header.h / 2);
    lv_obj_add_flag(s_glow, LV_OBJ_FLAG_HIDDEN);

    boot_update(0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_overlay);
    lv_anim_set_exec_cb(&a, anim_exec);
    lv_anim_set_values(&a, 0, BOOT_TOTAL_MS);
    lv_anim_set_duration(&a, BOOT_TOTAL_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_completed_cb(&a, anim_done);
    lv_anim_start(&a);
}

bool ui_boot_active(void)
{
    return s_active;
}

void ui_boot_release(void)
{
    if (!s_overlay) {
        return;
    }
    lv_anim_delete(s_overlay, NULL);
    lv_obj_delete(s_overlay);          /* borra tambien las imagenes hijas */
    s_overlay = s_logo = s_full = s_glow = NULL;
    for (int i = 0; i < LAYERS; i++) {
        if (s_l[i].buf) {
            lv_image_cache_drop(&s_l[i].dsc);
            ui_platform_free_large(s_l[i].buf);
            s_l[i].buf = NULL;
        }
        s_l[i].img = NULL;
    }
    s_active = false;
}

static void fade_exec(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void fade_done(lv_anim_t *a)
{
    (void)a;
    ui_boot_release();
}

void ui_boot_fade_out(void)
{
    if (!s_overlay) {
        return;
    }
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);  /* deja pasar el toque ya */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_overlay);
    lv_anim_set_exec_cb(&a, fade_exec);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_duration(&a, UI_BOOT_FADE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, fade_done);
    lv_anim_start(&a);
}
