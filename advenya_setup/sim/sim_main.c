/*
 * ADVENYA setup · simulador de host con escenarios de prueba
 *
 * PRUEBAS SIMULADAS: ejecutan el codigo real de la interfaz (components/
 * advenya_ui) y del nucleo de persistencia sobre LVGL 9.2.2 en el PC, con una
 * pantalla virtual 480x800 RGB565, un tactil guionizado y un reloj virtual.
 * NO prueban el hardware (panel, DMA2D, GT911, NVS real): eso solo se puede
 * comprobar en la placa.
 *
 * Uso: advenya_sim <escenario> <archivo_estado> <carpeta_salida>
 * Cada "arranque" de la placa es un proceso nuevo: los cortes de
 * alimentacion se simulan terminando el proceso sin mas.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lvgl.h"
#include "advenya_ui.h"
#include "ui_internal.h"
#include "sim.h"

#if defined(__SANITIZE_ADDRESS__)
/* API publica del runtime de ASan (la cabecera no siempre esta instalada) */
size_t __sanitizer_get_current_allocated_bytes(void);
#define HAVE_ASAN_STATS 1
#else
#include <malloc.h>
#endif

#define W 480
#define H 800
#define BUF_LINES 80

static uint16_t s_fb[W * H];
static uint8_t s_buf1[W * BUF_LINES * 2], s_buf2[W * BUF_LINES * 2];
static uint32_t s_now;
static int s_px, s_py;
static bool s_pressed;
static const char *s_out = ".";
static int s_fail, s_pass;

/* metricas de render (tiempo de CPU del HOST, no representativo de la placa) */
static double s_frame_us_sum, s_frame_us_max;
static unsigned s_frames;
static struct timespec s_t0;
static bool s_in_frame;

/* entrega capturada */
static bool s_handoff;
static adv_handoff_t s_handoff_data;

uint32_t sim_now_ms(void)
{
    return s_now;
}

size_t sim_heap_in_use(void)
{
#ifdef HAVE_ASAN_STATS
    return __sanitizer_get_current_allocated_bytes();
#else
    struct mallinfo2 mi = mallinfo2();
    return mi.uordblks;
#endif
}

static uint32_t tick_cb(void)
{
    return s_now;
}

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    const uint16_t *src = (const uint16_t *)px;
    int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; y++) {
        memcpy(&s_fb[y * W + a->x1], src, (size_t)w * 2);
        src += w;
    }
    lv_display_flush_ready(d);
}

static double elapsed_us(const struct timespec *a)
{
    struct timespec b;
    clock_gettime(CLOCK_MONOTONIC, &b);
    return (b.tv_sec - a->tv_sec) * 1e6 + (b.tv_nsec - a->tv_nsec) / 1e3;
}

static void refr_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_RENDER_START) {
        if (!s_in_frame) {
            s_in_frame = true;
            clock_gettime(CLOCK_MONOTONIC, &s_t0);
        }
    } else if (s_in_frame) {
        s_in_frame = false;
        double us = elapsed_us(&s_t0);
        s_frame_us_sum += us;
        if (us > s_frame_us_max) {
            s_frame_us_max = us;
        }
        s_frames++;
    }
}

static void read_cb(lv_indev_t *i, lv_indev_data_t *d)
{
    d->point.x = s_px;
    d->point.y = s_py;
    d->state = s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void step(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 5) {
        s_now += 5;
        lv_timer_handler();
    }
}

static void check(bool ok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("  %s ", ok ? "[PASS]" : "[FAIL]");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    if (ok) {
        s_pass++;
    } else {
        s_fail++;
    }
}

static void shot(const char *name)
{
    lv_refr_now(NULL);
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", s_out, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    static uint8_t row[W * 3];
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint16_t c = s_fb[y * W + x];
            row[x * 3 + 0] = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
            row[x * 3 + 1] = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
            row[x * 3 + 2] = (uint8_t)((c & 0x1F) * 255 / 31);
        }
        fwrite(row, 1, sizeof(row), f);
    }
    fclose(f);
}

static bool obj_center(const char *id, int *x, int *y)
{
    lv_obj_t *o = ui_test_find(id);
    if (!o || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    *x = (a.x1 + a.x2) / 2;
    *y = (a.y1 + a.y2) / 2;
    return true;
}

static bool tap(const char *id)
{
    int x, y;
    if (!obj_center(id, &x, &y)) {
        printf("  (no se encuentra '%s')\n", id);
        return false;
    }
    s_px = x;
    s_py = y;
    s_pressed = true;
    step(80);
    s_pressed = false;
    step(30);
    return true;
}

static void swipe_right(void)
{
    s_px = 40;
    s_py = 430;
    s_pressed = true;
    step(20);
    for (int i = 0; i < 10; i++) {
        s_px += 32;
        step(16);
    }
    s_pressed = false;
    step(30);
}

static const char *label_text(const char *id)
{
    lv_obj_t *o = ui_test_find(id);
    return o ? lv_label_get_text(o) : "";
}

static void sim_core(const adv_handoff_t *h)
{
    s_handoff = true;
    s_handoff_data = *h;
    printf("  [nucleo simulado] entrega recibida: motivo=%d idioma=%s oscuro=%d acento=#%06X fondo=%u\n",
           h->reason, h->language_code, h->dark, (unsigned)h->accent_rgb, (unsigned)h->wallpaper_index);
}

static void boot_device(const char *state)
{
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *d = lv_display_create(W, H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, s_buf1, s_buf2, sizeof(s_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush_cb);
    lv_display_add_event_cb(d, refr_cb, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(d, refr_cb, LV_EVENT_REFR_READY, NULL);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, read_cb);
    lv_timer_set_period(lv_indev_get_read_timer(in), 16);
    adv_settings_init(sim_settings_backend(state));
    adv_handoff_set_core(sim_core);
    adv_ui_start(d, in);
}

static const adv_settings_t *S(void)
{
    return adv_settings_get();
}

/* Carga lo que hay en el "flash" (archivo) sin tocar el estado vivo. */
static bool stored(const char *state, adv_settings_t *out)
{
    FILE *f = fopen(state, "rb");
    if (!f) {
        return false;
    }
    bool ok = fread(out, 1, sizeof(*out), f) == sizeof(*out);
    fclose(f);
    return ok;
}

static void wait_idle(void)
{
    for (int i = 0; i < 200 && (ui_wizard_busy() || ui_boot_active()); i++) {
        step(10);
    }
}

static void report_render(const char *what)
{
    printf("  [render host] %s: %u fotogramas, media %.0f us, max %.0f us\n", what, s_frames,
           s_frames ? s_frame_us_sum / s_frames : 0.0, s_frame_us_max);
    s_frames = 0;
    s_frame_us_sum = s_frame_us_max = 0;
}

/* ===================================================================== */

static void sc_boot_fresh(const char *state)
{
    printf("== Escenario: primer arranque (animacion + bienvenida)\n");
    remove(state);
    boot_device(state);
    check(ui_boot_active(), "la animacion de arranque esta activa al inicio");
    static const uint32_t marks[] = {300, 600, 900, 1200, 1500, 1800, 2000, 2200, 2700};
    uint32_t t = 0;
    for (size_t i = 0; i < sizeof(marks) / sizeof(marks[0]); i++) {
        step(marks[i] - t);
        t = marks[i];
        char n[32];
        snprintf(n, sizeof(n), "boot_%04u", (unsigned)marks[i]);
        shot(n);
    }
    report_render("animacion de arranque");
    step(3000 + UI_BOOT_FADE_MS + 200 - t);
    check(!ui_boot_active(), "el logotipo se retira tras ~3,3 s");
    check(ui_wizard_current() == UI_SCR_WELCOME, "pantalla inicial: bienvenida");
    check(strcmp(label_text("welcome.title"), "\xc2\xa1" "Bienvenido!") == 0, "texto '¡Bienvenido!' (idioma por defecto)");
    shot("01_welcome_es_light");
    adv_settings_t st;
    check(stored(state, &st) && st.stage == ADV_STAGE_WELCOME, "etapa 'bienvenida' persistida");
}

static void sc_flow(const char *state)
{
    printf("== Escenario: navegacion, idiomas, privacidad y personalizacion\n");
    boot_device(state);
    step(3600);
    wait_idle();
    check(ui_wizard_current() == UI_SCR_WELCOME, "arranca en bienvenida");

    /* --- navegacion y bloqueo de dobles toques --- */
    int x, y;
    obj_center("welcome.start", &x, &y);
    s_px = x;
    s_py = y;
    s_pressed = true;
    step(60);
    s_pressed = false;
    step(40);
    check(ui_wizard_busy(), "transicion en curso tras 'Comenzar'");
    step(120);
    shot("transition_mid_welcome_to_language");
    s_pressed = true;          /* segundo toque durante la transicion */
    step(40);
    s_pressed = false;
    step(20);
    report_render("transicion bienvenida -> idioma");
    uint32_t t0 = s_now;
    wait_idle();
    printf("  duracion medida de la transicion: ~%u ms (desde el toque)\n", (unsigned)(s_now - t0 + 280));
    check(ui_wizard_current() == UI_SCR_LANGUAGE, "un unico avance a idioma (doble toque ignorado)");
    check(S()->stage == ADV_STAGE_LANGUAGE, "etapa idioma registrada");
    shot("02_language_es_light");

    /* --- los cinco idiomas, actualizacion inmediata --- */
    static const char *const rows[] = {"lang.en", "lang.pt-BR", "lang.fr", "lang.de", "lang.es"};
    static const uint8_t langs[] = {1, 2, 3, 4, 0};
    static const char *const names[] = {"en", "pt-BR", "fr", "de", "es"};
    for (int i = 0; i < 5; i++) {
        tap(rows[i]);
        step(250);
        const char *expect = adv_i18n_table[langs[i]][TXT_LANG_TITLE];
        check(S()->language == langs[i] && strcmp(label_text("language.title"), expect) == 0,
              "idioma %s aplicado al instante (titulo '%s')", names[i], label_text("language.title"));
        check(strcmp(label_text("welcome.title"), adv_i18n_table[langs[i]][TXT_WELCOME_TITLE]) == 0,
              "  ... tambien en la pantalla oculta de bienvenida");
        char n[48];
        snprintf(n, sizeof(n), "02_language_%s_light", names[i]);
        shot(n);
    }
    tap("lang.fr");
    step(300);
    adv_settings_t st;
    stored(state, &st);
    uint32_t writes_before = adv_settings_write_count();
    check(st.language != 3, "cambio de idioma NO escrito de inmediato (escritura diferida)");
    step(ADV_SETTINGS_DEBOUNCE_MS + 300);
    stored(state, &st);
    check(st.language == 3 && adv_settings_write_count() == writes_before + 1,
          "idioma escrito una sola vez tras %d ms sin cambios", ADV_SETTINGS_DEBOUNCE_MS);
    tap("lang.es");
    step(200);

    /* --- retroceso con gesto y conservacion --- */
    swipe_right();
    wait_idle();
    check(ui_wizard_current() == UI_SCR_WELCOME, "gesto a la derecha: vuelve a bienvenida");
    tap("welcome.start");
    wait_idle();
    check(ui_wizard_current() == UI_SCR_LANGUAGE && S()->language == 0, "al volver se conserva el idioma elegido");

    /* --- privacidad --- */
    tap("language.continue");
    wait_idle();
    check(ui_wizard_current() == UI_SCR_PRIVACY, "avanza a privacidad");
    shot("03_privacy_es_light");
    tap("privacy.accept");
    wait_idle();
    check(ui_wizard_current() == UI_SCR_PRIVACY && !S()->privacy_accepted,
          "sin marcar la casilla no se puede continuar");
    step(700);
    tap("privacy.card.privacy");
    step(400);
    shot("03b_privacy_sheet_es_light");
    check(ui_test_find("legal.close") != NULL, "el apartado 'Privacidad' abre su documento");
    tap("legal.close");
    step(400);
    check(ui_test_find("legal.close") == NULL, "el documento se cierra");
    tap("privacy.card.perms");
    step(400);
    swipe_right();
    step(400);
    check(ui_test_find("legal.close") == NULL && ui_wizard_current() == UI_SCR_PRIVACY,
          "el gesto de retroceso cierra el documento sin salir de privacidad");
    tap("privacy.card.terms");
    step(400);
    tap("legal.close");
    step(400);

    tap("privacy.accept_row");
    step(250);
    shot("03c_privacy_checked_es_light");
    tap("privacy.later");
    wait_idle();
    stored(state, &st);
    check(ui_wizard_current() == UI_SCR_WELCOME && !S()->privacy_accepted && !st.privacy_accepted,
          "'Mas tarde' vuelve a bienvenida sin registrar la aceptacion");
    tap("welcome.start");
    wait_idle();
    tap("language.continue");
    wait_idle();
    tap("privacy.accept_row");
    step(250);
    tap("privacy.accept");
    wait_idle();
    stored(state, &st);
    check(ui_wizard_current() == UI_SCR_PERSONALIZE && st.privacy_accepted && st.stage == ADV_STAGE_PERSONALIZE,
          "aceptacion explicita registrada y avance a personalizacion");
    shot("04_personalize_es_light");

    /* --- temas --- */
    tap("theme.dark");
    step(500);
    check(S()->theme_mode == ADV_THEME_DARK && ui_theme_is_dark(), "tema oscuro aplicado al instante");
    shot("04_personalize_es_dark");
    tap("theme.auto");
    step(500);
    check(S()->theme_mode == ADV_THEME_AUTO && !ui_theme_is_dark(),
          "automatico sin hora conocida se resuelve a claro");
    shot("04_personalize_es_auto");
    tap("theme.light");
    step(500);

    /* --- acentos --- */
    for (int i = 1; i < ADV_SETTINGS_ACCENT_COUNT; i++) {
        char id[16];
        snprintf(id, sizeof(id), "accent.%d", i);
        tap(id);
        step(250);
        check(S()->accent == i && ui_theme_palette()->accent.red == ((ui_accent_rgb[i] >> 16) & 0xFF),
              "acento %d aplicado (#%06X)", i, (unsigned)ui_accent_rgb[i]);
        if (i == 1 || i == 5) {
            char n[48];
            snprintf(n, sizeof(n), "04_personalize_accent%d", i);
            shot(n);
        }
    }
    tap("accent.0");
    step(250);

    /* --- fondos con crossfade --- */
    for (int i = 1; i <= ADV_WP_COUNT; i++) {
        int wp = i % ADV_WP_COUNT;
        char id[16];
        snprintf(id, sizeof(id), "wp.%d", wp);
        tap(id);
        step(UI_BACKDROP_FADE_MS / 2);
        if (wp == 2) {
            shot("04_personalize_wp_crossfade_mid");
        }
        step(UI_BACKDROP_FADE_MS);
        check(S()->wallpaper == wp, "fondo %d seleccionado", wp + 1);
        char n[48];
        snprintf(n, sizeof(n), "04_personalize_wp%d_light", wp + 1);
        shot(n);
    }
    report_render("cambios de fondo (incluye horneado sincrono en el host)");

    /* --- retroceso desde personalizacion conserva todo --- */
    tap("wp.3");
    tap("theme.dark");
    tap("accent.6");
    step(300);
    swipe_right();
    wait_idle();
    check(ui_wizard_current() == UI_SCR_PRIVACY && ui_scr_privacy_checked(),
          "retroceso a privacidad: la casilla sigue marcada (aceptado)");
    shot("03_privacy_es_dark_green");
    tap("privacy.accept");
    wait_idle();
    check(ui_wizard_current() == UI_SCR_PERSONALIZE && S()->wallpaper == 3 && S()->theme_mode == ADV_THEME_DARK &&
          S()->accent == 6, "al volver a personalizacion se conservan tema, color y fondo");
    step(ADV_SETTINGS_DEBOUNCE_MS + 400);
    stored(state, &st);
    check(st.wallpaper == 3 && st.theme_mode == ADV_THEME_DARK && st.accent == 6,
          "elecciones persistidas (escritura diferida)");
    printf("  escrituras de estado en todo el escenario: %u\n", (unsigned)adv_settings_write_count());
    /* el proceso termina aqui = corte de alimentacion en personalizacion */
}

static void sc_resume(const char *state)
{
    printf("== Escenario: recuperacion tras corte de alimentacion\n");
    boot_device(state);
    step(3700);
    wait_idle();
    check(ui_wizard_current() == UI_SCR_PERSONALIZE, "continua en personalizacion (etapa guardada)");
    check(S()->wallpaper == 3 && S()->theme_mode == ADV_THEME_DARK && S()->accent == 6 && S()->privacy_accepted,
          "elecciones recuperadas: fondo 4, oscuro, verde, privacidad aceptada");
    shot("05_resume_personalize");
    size_t heap_before = sim_heap_in_use();
    tap("perso.continue");
    step(UI_BOOT_FADE_MS + 200);
    check(s_handoff && s_handoff_data.reason == ADV_HANDOFF_SETUP_JUST_COMPLETED, "entrega al nucleo (setup completado)");
    check(s_handoff && strcmp(s_handoff_data.language_code, "es") == 0 && s_handoff_data.dark &&
          s_handoff_data.accent_rgb == ui_accent_rgb[6] && s_handoff_data.wallpaper_index == 3,
          "la entrega incluye idioma, tema, acento y fondo");
    check(ui_test_find("perso.continue") == NULL && ui_test_find("welcome.start") == NULL,
          "pantallas del asistente destruidas tras la entrega");
    adv_settings_t st;
    check(stored(state, &st) && st.setup_complete && st.stage == ADV_STAGE_COMPLETE, "setup marcado como completo");
    shot("06_after_handoff_backdrop");
    size_t heap_mid = sim_heap_in_use();
    s_handoff_data.release_setup_ui();
    step(50);
    size_t heap_after = sim_heap_in_use();
    printf("  heap: antes de terminar %zu B, tras destruir pantallas %zu B, tras release_setup_ui %zu B\n",
           heap_before, heap_mid, heap_after);
    check(heap_after + 2000000 < heap_before, "release_setup_ui libera el fondo (~2,3 MB)");
}

static void sc_boot_complete(const char *state)
{
    printf("== Escenario: arranque con el setup ya completado\n");
    boot_device(state);
    step(3200);
    check(s_handoff && s_handoff_data.reason == ADV_HANDOFF_SETUP_ALREADY_DONE, "entrega directa al nucleo");
    check(ui_boot_active(), "el logotipo queda fijo hasta que el nucleo lo retira");
    check(ui_test_find("welcome.start") == NULL, "no se crea ninguna pantalla del asistente");
    shot("07_boot_complete_logo_hold");
    s_handoff_data.release_setup_ui();
    step(UI_BOOT_FADE_MS + 100);
    check(!ui_boot_active(), "release_setup_ui funde y retira el logotipo");
}

static void goto_scr(ui_scr_t s)
{
    if (ui_wizard_current() == s) {
        return;
    }
    ui_wizard_go(s, s > ui_wizard_current() ? 1 : -1);
    wait_idle();
}

static void sc_matrix(const char *state)
{
    printf("== Escenario: matriz de capturas (5 idiomas x 4 pantallas, claro y oscuro)\n");
    remove(state);
    boot_device(state);
    step(3700);
    wait_idle();
    adv_settings_edit()->privacy_accepted = 1;
    static const char *const names[] = {"es", "en", "pt-BR", "fr", "de"};
    static const char *const scr[] = {"welcome", "language", "privacy", "personalize"};
    for (int dark = 0; dark < 2; dark++) {
        adv_settings_edit()->theme_mode = dark ? ADV_THEME_DARK : ADV_THEME_LIGHT;
        ui_theme_apply();
        ui_backdrop_request(S()->wallpaper, dark, false);
        for (int l = 0; l < ADV_LANG_COUNT; l++) {
            adv_settings_edit()->language = l;
            ui_i18n_set_language(l);
            ui_scr_language_on_show();
            for (int s = 0; s < UI_SCR_COUNT; s++) {
                goto_scr((ui_scr_t)s);
                step(100);
                char n[64];
                snprintf(n, sizeof(n), "m_%s_%s_%s", scr[s], names[l], dark ? "dark" : "light");
                shot(n);
            }
            goto_scr(UI_SCR_WELCOME);
        }
    }
    check(true, "matriz generada");
}

static void sc_stress(const char *state)
{
    printf("== Escenario: estabilidad de memoria (ciclos repetidos)\n");
    remove(state);
    boot_device(state);
    step(3700);
    wait_idle();
    adv_settings_edit()->privacy_accepted = 1;
    /* calentamiento: crea las cuatro pantallas y llena las caches */
    for (int s = 1; s < UI_SCR_COUNT; s++) {
        goto_scr((ui_scr_t)s);
    }
    goto_scr(UI_SCR_WELCOME);
    step(ADV_SETTINGS_DEBOUNCE_MS + 200);
    size_t h0 = sim_heap_in_use();
    for (int k = 0; k < 30; k++) {
        tap("welcome.start");
        wait_idle();
        tap(k % 2 ? "lang.en" : "lang.es");
        step(200);
        tap("language.continue");
        wait_idle();
        tap("privacy.card.terms");
        step(350);
        tap("legal.close");
        step(300);
        goto_scr(UI_SCR_PERSONALIZE);
        tap(k % 2 ? "theme.dark" : "theme.light");
        tap(k % 3 ? "accent.2" : "accent.0");
        char id[16];
        snprintf(id, sizeof(id), "wp.%d", k % 5);
        tap(id);
        step(UI_BACKDROP_FADE_MS + 50);
        swipe_right();
        wait_idle();
        swipe_right();
        wait_idle();
        swipe_right();
        wait_idle();
    }
    tap("welcome.start");
    wait_idle();
    tap("lang.es");
    step(200);
    goto_scr(UI_SCR_PERSONALIZE);
    tap("theme.light");
    tap("accent.0");
    tap("wp.0");
    step(UI_BACKDROP_FADE_MS + 50);
    goto_scr(UI_SCR_WELCOME);
    step(ADV_SETTINGS_DEBOUNCE_MS + 200);
    size_t h1 = sim_heap_in_use();
    long diff = (long)h1 - (long)h0;
    printf("  heap tras calentamiento %zu B, tras 30 ciclos completos %zu B (diferencia %ld B)\n", h0, h1, diff);
    check(diff < 4096 && diff > -4096, "sin crecimiento de memoria tras 30 ciclos (|diferencia| < 4 KB)");
    check(adv_settings_write_count() < 200, "escrituras de estado acotadas: %u en 30 ciclos",
          (unsigned)adv_settings_write_count());
}

static void sc_powercut_lang(const char *state)
{
    printf("== Escenario: corte durante la seleccion de idioma\n");
    remove(state);
    boot_device(state);
    step(3700);
    wait_idle();
    tap("welcome.start");
    wait_idle();
    tap("lang.de");
    step(ADV_SETTINGS_DEBOUNCE_MS + 300);
    /* corte: fin del proceso; el escenario 'powercut_lang_resume' verifica */
}

static void sc_powercut_lang_resume(const char *state)
{
    boot_device(state);
    step(3700);
    wait_idle();
    check(ui_wizard_current() == UI_SCR_LANGUAGE && S()->language == 4 &&
          strcmp(label_text("language.title"), adv_i18n_table[4][TXT_LANG_TITLE]) == 0,
          "tras el corte continua en idioma con el aleman ya aplicado");
    shot("08_resume_language_de");
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "uso: %s <escenario> <estado> <salida>\n", argv[0]);
        return 2;
    }
    s_out = argv[3];
    const char *sc = argv[1], *state = argv[2];
    if (!strcmp(sc, "boot_fresh")) {
        sc_boot_fresh(state);
    } else if (!strcmp(sc, "flow")) {
        sc_flow(state);
    } else if (!strcmp(sc, "resume")) {
        sc_resume(state);
    } else if (!strcmp(sc, "boot_complete")) {
        sc_boot_complete(state);
    } else if (!strcmp(sc, "matrix")) {
        sc_matrix(state);
    } else if (!strcmp(sc, "stress")) {
        sc_stress(state);
    } else if (!strcmp(sc, "powercut_lang")) {
        sc_powercut_lang(state);
    } else if (!strcmp(sc, "powercut_lang_resume")) {
        sc_powercut_lang_resume(state);
    } else {
        fprintf(stderr, "escenario desconocido: %s\n", sc);
        return 2;
    }
    printf("  resultado: %d correctas, %d fallidas\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
