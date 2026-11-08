/*
 * ADVENYA OS · fondo del asistente
 *
 * El fondo es el fondo de pantalla elegido visto a traves de un "velo" del
 * color del tema (blanco en claro, azul noche en oscuro). La mascara del velo
 * (wp_veil_mask) deja el centro limpio y muestra el fondo en trazos curvos en
 * las esquinas, como en las referencias. Elegir otro fondo o cambiar de tema
 * produce un crossfade.
 *
 * Horneado: escala x2 bilineal desde 240x400 RGB888, mezcla con el velo y
 * tramado ordenado 4x4 a RGB565, en un buffer de PSRAM de 480x800. Se hace
 * fuera de la tarea de LVGL (ui_platform_run_async) y solo cuando cambia el
 * fondo o el tema: dibujar el fondo despues es copiar una imagen opaca.
 *
 * Tres buffers rotativos: el visible, el que entra en el crossfade y el que
 * se esta horneando. Si llegan peticiones durante un horneado solo se atiende
 * la ultima.
 */
#include <string.h>
#include "ui_internal.h"
#include "ui_platform.h"

#define BD_W UI_W
#define BD_H UI_H
#define NBUF 3

typedef struct {
    uint16_t *px;
    lv_image_dsc_t dsc;
} bd_buf_t;

typedef struct {
    int idx;
    uint8_t wp;
    bool dark;
    bool animate;
} bd_job_t;

static bd_buf_t s_buf[NBUF];
static lv_obj_t *s_bottom, *s_top;
static int s_bottom_idx = -1, s_top_idx = -1;
static bool s_baking, s_ready, s_release_pending;
static bd_job_t s_job;
static struct {
    bool valid;
    uint8_t wp;
    bool dark;
    bool animate;
} s_pending;
static struct {
    bool valid;
    uint8_t wp;
    bool dark;
} s_shown;

static const uint8_t *const s_wp_src[ADV_WP_COUNT] = {
    wp_src_blue, wp_src_violet, wp_src_peach, wp_src_night, wp_src_silver,
};

static const uint8_t s_bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

/* Muestra bilineal de una imagen de 1 o 3 canales a media resolucion. */
static inline void coord(int v, int src_max, int *i0, int *i1, int *w)
{
    int f = v * 128 - 64;                    /* (v + 0.5) / 2 - 0.5, en 1/256 px */
    if (f < 0) {
        f = 0;
    }
    *i0 = f >> 8;
    *w = f & 255;
    *i1 = *i0 + 1 < src_max ? *i0 + 1 : *i0;
}

static void bake(uint16_t *dst, uint8_t wp, bool dark)
{
    const uint8_t *src = s_wp_src[wp];
    const uint8_t *mask = wp_veil_mask;
    const int vr = dark ? 0x0A : 0xFF, vg = dark ? 0x0F : 0xFF, vb = dark ? 0x1C : 0xFF;
    for (int y = 0; y < BD_H; y++) {
        int my0, my1, mwy;
        coord(y, ADV_WP_SRC_H, &my0, &my1, &mwy);
        /* En la mitad superior izquierda se muestrea el fondo girado 180 grados
         * para que ambas esquinas tengan su tono caracteristico (la frontera
         * cae bajo el velo casi opaco del centro y no se aprecia). */
        int ry0, ry1, rwy;
        coord(BD_H - 1 - y, ADV_WP_SRC_H, &ry0, &ry1, &rwy);
        uint16_t *out = dst + (size_t)y * BD_W;
        for (int x = 0; x < BD_W; x++) {
            int mx0, mx1, mwx;
            coord(x, ADV_WP_SRC_W, &mx0, &mx1, &mwx);
            /* velo */
            int m00 = mask[my0 * ADV_WP_SRC_W + mx0], m01 = mask[my0 * ADV_WP_SRC_W + mx1];
            int m10 = mask[my1 * ADV_WP_SRC_W + mx0], m11 = mask[my1 * ADV_WP_SRC_W + mx1];
            int mt = m00 + (((m01 - m00) * mwx) >> 8);
            int mb = m10 + (((m11 - m10) * mwx) >> 8);
            int m = mt + (((mb - mt) * mwy) >> 8);
            /* fondo */
            bool rot = (x * BD_H + y * BD_W) < (BD_W * BD_H);
            int sx0, sx1, swx, sy0 = my0, sy1 = my1, swy = mwy;
            if (rot) {
                coord(BD_W - 1 - x, ADV_WP_SRC_W, &sx0, &sx1, &swx);
                sy0 = ry0;
                sy1 = ry1;
                swy = rwy;
            } else {
                sx0 = mx0;
                sx1 = mx1;
                swx = mwx;
            }
            const uint8_t *p00 = src + (sy0 * ADV_WP_SRC_W + sx0) * 3;
            const uint8_t *p01 = src + (sy0 * ADV_WP_SRC_W + sx1) * 3;
            const uint8_t *p10 = src + (sy1 * ADV_WP_SRC_W + sx0) * 3;
            const uint8_t *p11 = src + (sy1 * ADV_WP_SRC_W + sx1) * 3;
            int c[3];
            for (int k = 0; k < 3; k++) {
                int t = p00[k] + (((p01[k] - p00[k]) * swx) >> 8);
                int b = p10[k] + (((p11[k] - p10[k]) * swx) >> 8);
                c[k] = t + (((b - t) * swy) >> 8);
            }
            int r = (c[0] * (255 - m) + vr * m) / 255;
            int g = (c[1] * (255 - m) + vg * m) / 255;
            int b = (c[2] * (255 - m) + vb * m) / 255;
            int d = s_bayer[y & 3][x & 3];
            r += (d >> 1) - 4;
            g += (d >> 2) - 2;
            b += (d >> 1) - 4;
            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);
            out[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        }
    }
}

static bool alloc_buffers(void)
{
    for (int i = 0; i < NBUF; i++) {
        if (s_buf[i].px) {
            continue;
        }
        s_buf[i].px = ui_platform_alloc_large((size_t)BD_W * BD_H * 2);
        if (!s_buf[i].px) {
            LV_LOG_ERROR("backdrop: sin memoria para el buffer %d", i);
            return false;
        }
        lv_image_dsc_t *d = &s_buf[i].dsc;
        memset(d, 0, sizeof(*d));
        d->header.magic = LV_IMAGE_HEADER_MAGIC;
        d->header.cf = LV_COLOR_FORMAT_RGB565;
        d->header.w = BD_W;
        d->header.h = BD_H;
        d->header.stride = BD_W * 2;
        d->data_size = (uint32_t)BD_W * BD_H * 2;
        d->data = (const uint8_t *)s_buf[i].px;
    }
    return true;
}

static lv_obj_t *make_img(lv_obj_t *parent)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(img, 0, 0);
    lv_obj_set_size(img, BD_W, BD_H);
    return img;
}

void ui_backdrop_create(lv_obj_t *parent)
{
    s_bottom = make_img(parent);
    s_top = make_img(parent);
    lv_obj_move_to_index(s_top, 0);
    lv_obj_move_to_index(s_bottom, 0);    /* ambos al fondo de la pila */
    s_bottom_idx = s_top_idx = -1;
    s_ready = false;
    s_shown.valid = false;
}

static void set_src(lv_obj_t *img, int idx)
{
    lv_image_set_src(img, &s_buf[idx].dsc);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_HIDDEN);
}

static void fade_exec(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void fade_done(lv_anim_t *a)
{
    (void)a;
    if (s_top_idx < 0) {
        return;
    }
    s_bottom_idx = s_top_idx;
    s_top_idx = -1;
    set_src(s_bottom, s_bottom_idx);
    lv_obj_add_flag(s_top, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(s_top, LV_OPA_TRANSP, 0);
}

static void show(int idx, bool animate)
{
    if (s_top_idx >= 0) {                 /* crossfade en curso: se completa ya */
        lv_anim_delete(s_top, fade_exec);
        fade_done(NULL);
    }
    if (!animate || s_bottom_idx < 0) {
        s_bottom_idx = idx;
        set_src(s_bottom, idx);
        lv_obj_add_flag(s_top, LV_OBJ_FLAG_HIDDEN);
        s_ready = true;
        return;
    }
    s_top_idx = idx;
    lv_obj_set_style_opa(s_top, LV_OPA_TRANSP, 0);
    set_src(s_top, idx);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_top);
    lv_anim_set_exec_cb(&a, fade_exec);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, UI_BACKDROP_FADE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, fade_done);
    lv_anim_start(&a);
    s_ready = true;
}

static void start_bake(uint8_t wp, bool dark, bool animate);

static void bake_work(void *arg)
{
    bd_job_t *j = arg;
    bake(s_buf[j->idx].px, j->wp, j->dark);
}

static void bake_done(void *arg)
{
    bd_job_t *j = arg;
    s_baking = false;
    if (s_release_pending) {
        ui_backdrop_release();
        return;
    }
    lv_image_cache_drop(&s_buf[j->idx].dsc);
    s_shown.valid = true;
    s_shown.wp = j->wp;
    s_shown.dark = j->dark;
    show(j->idx, j->animate);
    if (s_pending.valid) {
        s_pending.valid = false;
        start_bake(s_pending.wp, s_pending.dark, s_pending.animate);
    }
}

static void start_bake(uint8_t wp, bool dark, bool animate)
{
    if (s_shown.valid && s_shown.wp == wp && s_shown.dark == dark) {
        return;                            /* ya es lo que se ve */
    }
    if (!alloc_buffers()) {
        return;
    }
    int idx = 0;
    while (idx == s_bottom_idx || idx == s_top_idx) {
        idx++;
    }
    s_job = (bd_job_t){idx, wp, dark, animate};
    s_baking = true;
    if (!ui_platform_run_async(bake_work, bake_done, &s_job)) {
        bake_work(&s_job);
        bake_done(&s_job);
    }
}

void ui_backdrop_request(uint8_t wallpaper, bool dark, bool animate)
{
    if (!s_bottom || wallpaper >= ADV_WP_COUNT) {
        return;
    }
    if (s_baking) {
        s_pending.valid = true;
        s_pending.wp = wallpaper;
        s_pending.dark = dark;
        s_pending.animate = animate;
        return;
    }
    start_bake(wallpaper, dark, animate);
}

bool ui_backdrop_ready(void)
{
    return s_ready;
}

void ui_backdrop_release(void)
{
    if (s_baking) {
        s_release_pending = true;          /* se libera al terminar el horneado */
        return;
    }
    s_release_pending = false;
    if (s_top) {
        lv_anim_delete(s_top, fade_exec);
        lv_obj_delete(s_top);
        s_top = NULL;
    }
    if (s_bottom) {
        lv_obj_delete(s_bottom);
        s_bottom = NULL;
    }
    for (int i = 0; i < NBUF; i++) {
        if (s_buf[i].px) {
            lv_image_cache_drop(&s_buf[i].dsc);
            ui_platform_free_large(s_buf[i].px);
            s_buf[i].px = NULL;
        }
    }
    s_bottom_idx = s_top_idx = -1;
    s_ready = false;
    s_shown.valid = false;
}
