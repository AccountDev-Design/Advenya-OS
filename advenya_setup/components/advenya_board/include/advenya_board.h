/*
 * ADVENYA OS · capa de hardware de la placa GUITION JC4880P443C_I_W
 *
 * Esta capa NO conoce LVGL ni la interfaz: solo enciende el panel, entrega su
 * handle esp_lcd, controla la retroiluminacion y lee el GT911.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_mipi_dsi.h"
#include "driver/i2c_master.h"
#include "advenya_board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ panel */

typedef struct {
    esp_lcd_panel_handle_t    panel;   /* panel DPI (framebuffer de hardware)  */
    esp_lcd_panel_io_handle_t io;      /* canal DBI para comandos DCS          */
    esp_lcd_dsi_bus_handle_t  bus;
    void                     *ldo;     /* esp_ldo_channel_handle_t             */
    int                       h_res;
    int                       v_res;
} adv_display_t;

/**
 * Enciende el panel: LDO canal 3 (2,5 V) -> bus DSI 2 lanes @500 Mbps ->
 * canal DBI -> panel DPI 480x800 @34 MHz con DMA2D -> reset fisico -> tabla
 * DCS del ST7701 -> SLPOUT -> DISPON. La retroiluminacion NO se enciende aqui.
 * En caso de error libera lo que haya creado y devuelve el codigo.
 */
esp_err_t adv_board_display_init(adv_display_t *out);

/** Libera panel, canal DBI, bus DSI y LDO (orden inverso a la creacion). */
void adv_board_display_deinit(adv_display_t *d);

/**
 * Registra el callback de "fin de copia" (on_color_trans_done). Con
 * CONFIG_LCD_DSI_ISR_CACHE_SAFE el callback DEBE estar en IRAM y su contexto
 * en RAM interna.
 */
esp_err_t adv_board_display_register_trans_done(adv_display_t *d,
                                                esp_lcd_dpi_panel_color_trans_done_cb_t cb,
                                                void *ctx);

/** DCS 0x28 DISPOFF / 0x29 DISPON (reversibles sin reinicializar). */
esp_err_t adv_board_display_set_on(adv_display_t *d, bool on);

/* -------------------------------------------------------- retroiluminacion */

esp_err_t adv_board_backlight_init(void);              /* arranca APAGADA      */
void      adv_board_backlight_set(int percent);        /* 5..100 (0 = apagar)  */
int       adv_board_backlight_get(void);

/* ------------------------------------------------------------------ tactil */

typedef struct {
    bool     present;        /* GT911 detectado                               */
    uint8_t  addr;           /* 0x5D o 0x14                                   */
    char     product_id[5];  /* registro 0x8140                               */
    uint16_t cfg_res_x;      /* "X/Y Output Max" (0x8048..0x804B)             */
    uint16_t cfg_res_y;
    uint32_t read_errors;    /* lecturas I2C fallidas acumuladas              */
    uint32_t bus_recoveries; /* recuperaciones del bus realizadas             */
} adv_touch_info_t;

typedef enum {
    ADV_TOUCH_NO_DATA = -1,  /* sin frame nuevo (mantener el estado anterior) */
    ADV_TOUCH_RELEASED = 0,
    ADV_TOUCH_PRESSED = 1,
} adv_touch_state_t;

/** Inicializa el GT911. Fallo SUAVE: sin tactil el sistema sigue arrancando. */
esp_err_t adv_board_touch_init(void);

/**
 * Lee un frame. Coordenadas nativas 0..479 x 0..799, ya escaladas a la
 * resolucion de la pantalla. Debe llamarse siempre desde el mismo hilo.
 */
adv_touch_state_t adv_board_touch_read(uint16_t *x, uint16_t *y);

void adv_board_touch_get_info(adv_touch_info_t *info);

/** Bus I2C compartido (GPIO7/8), para futuros drivers del mismo bus. */
i2c_master_bus_handle_t adv_board_i2c_bus(void);

#ifdef __cplusplus
}
#endif
