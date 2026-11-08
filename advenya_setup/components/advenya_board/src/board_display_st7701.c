/*
 * ADVENYA OS · panel ST7701 por MIPI-DSI (JC4880P443C_I_W) sobre ESP-IDF 5.5.5
 *
 * Adaptacion de flexPanelInit() (FlexOS_Ultra/FlexOS_Ultra_HAL.h) a ESP-IDF puro:
 *  - mismos pasos y mismos valores (LDO, DSI, DBI, DPI, reset, tabla DCS);
 *  - pinMode/digitalWrite/delay -> driver/gpio + vTaskDelay;
 *  - pixel_format (obsoleto en IDF 5.5) -> in_color_format = LCD_COLOR_FMT_RGB565;
 *  - reloj de referencia del PHY elegido segun la revision de chip configurada
 *    (en IDF 5.5 el valor por defecto cambia para las revisiones >= 3.0);
 *  - liberacion completa de recursos si cualquier paso falla.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "advenya_board.h"
#include "st7701_init_table.h"

static const char *TAG = "adv_display";

static void lcd_hw_reset(void)
{
    /* Secuencia de flexPanelInit(): HIGH 5 ms, LOW 10 ms, HIGH 120 ms */
    gpio_set_level(ADV_PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(ADV_PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(ADV_PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

esp_err_t adv_board_display_init(adv_display_t *out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out NULL");
    memset(out, 0, sizeof(*out));
    esp_err_t ret = ESP_OK;

    /* Reset del panel como salida (nivel alto en reposo) */
    const gpio_config_t rst_cfg = {
        .pin_bit_mask = 1ULL << ADV_PIN_LCD_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&rst_cfg), TAG, "gpio reset");
    gpio_set_level(ADV_PIN_LCD_RST, 1);

    /* 1) LDO interno del P4 que alimenta el PHY MIPI */
    esp_ldo_channel_handle_t ldo = NULL;
    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = ADV_MIPI_PHY_LDO_CHAN,
        .voltage_mv = ADV_MIPI_PHY_LDO_MV,
    };
    ESP_GOTO_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &ldo), fail, TAG, "LDO MIPI (canal %d)", ADV_MIPI_PHY_LDO_CHAN);
    out->ldo = ldo;

    /* 2) Bus DSI: 2 lanes @ 500 Mbps */
    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = ADV_DSI_LANES,
#if CONFIG_ESP32P4_SELECTS_REV_LESS_V3
        /* Revisiones < 3.0: PLL_F20M, el mismo reloj que usaba Flex OS Ultra con
         * IDF 5.4 (MIPI_DSI_PHY_CLK_SRC_DEFAULT). */
        .phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT_LEGACY,
#else
        .phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT,
#endif
        .lane_bit_rate_mbps = ADV_DSI_LANE_MBPS,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &out->bus), fail, TAG, "esp_lcd_new_dsi_bus");

    /* 3) Canal DBI para la tabla DCS */
    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(out->bus, &dbi_cfg, &out->io), fail, TAG, "esp_lcd_new_panel_io_dbi");

    /* 4) Panel DPI: framebuffer de hardware (PSRAM) refrescado por el puente DSI */
    const esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = ADV_DPI_CLOCK_MHZ,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = ADV_LCD_H_RES,
            .v_size = ADV_LCD_V_RES,
            .hsync_pulse_width = ADV_DPI_HSYNC_PW,
            .hsync_back_porch = ADV_DPI_HSYNC_BP,
            .hsync_front_porch = ADV_DPI_HSYNC_FP,
            .vsync_pulse_width = ADV_DPI_VSYNC_PW,
            .vsync_back_porch = ADV_DPI_VSYNC_BP,
            .vsync_front_porch = ADV_DPI_VSYNC_FP,
        },
        .flags = {
            .use_dma2d = 1,   /* copia del buffer de LVGL al framebuffer por DMA2D */
        },
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(out->bus, &dpi_cfg, &out->panel), fail, TAG, "esp_lcd_new_panel_dpi");

    /* 5) Reset fisico del ST7701 y arranque del flujo DPI */
    lcd_hw_reset();
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(out->panel), fail, TAG, "esp_lcd_panel_init");

    /* 6) Tabla del fabricante + COLMOD/MADCTL/INVOFF + SLPOUT + DISPON */
    for (size_t i = 0; i < ST7701_INIT_LEN; i++) {
        ESP_GOTO_ON_ERROR(esp_lcd_panel_io_tx_param(out->io, ST7701_INIT[i].cmd,
                                                    ST7701_INIT[i].data, ST7701_INIT[i].len),
                          fail, TAG, "DCS 0x%02X", ST7701_INIT[i].cmd);
    }
    ESP_GOTO_ON_ERROR(esp_lcd_panel_io_tx_param(out->io, 0x11, NULL, 0), fail, TAG, "SLPOUT");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_GOTO_ON_ERROR(esp_lcd_panel_io_tx_param(out->io, 0x29, NULL, 0), fail, TAG, "DISPON");
    vTaskDelay(pdMS_TO_TICKS(20));

    out->h_res = ADV_LCD_H_RES;
    out->v_res = ADV_LCD_V_RES;
#if !CONFIG_LCD_DSI_ISR_CACHE_SAFE
    /* Mismo aviso que Flex: sin ISR cache-safe, escribir la flash (NVS) puede
     * dejar al puente DSI sin pixeles durante el borrado y el panel destella. */
    ESP_LOGW(TAG, "CONFIG_LCD_DSI_ISR_CACHE_SAFE desactivado: el panel puede destellar al escribir NVS");
#endif
    ESP_LOGI(TAG, "panel ST7701 %dx%d por MIPI-DSI (%d lanes @%d Mbps, DPI %d MHz) listo",
             ADV_LCD_H_RES, ADV_LCD_V_RES, ADV_DSI_LANES, ADV_DSI_LANE_MBPS, ADV_DPI_CLOCK_MHZ);
    return ESP_OK;

fail:
    adv_board_display_deinit(out);
    return ret;
}

void adv_board_display_deinit(adv_display_t *d)
{
    if (!d) {
        return;
    }
    if (d->panel) {
        esp_lcd_panel_del(d->panel);
        d->panel = NULL;
    }
    if (d->io) {
        esp_lcd_panel_io_del(d->io);
        d->io = NULL;
    }
    if (d->bus) {
        esp_lcd_del_dsi_bus(d->bus);
        d->bus = NULL;
    }
    if (d->ldo) {
        esp_ldo_release_channel((esp_ldo_channel_handle_t)d->ldo);
        d->ldo = NULL;
    }
}

esp_err_t adv_board_display_register_trans_done(adv_display_t *d,
                                                esp_lcd_dpi_panel_color_trans_done_cb_t cb,
                                                void *ctx)
{
    ESP_RETURN_ON_FALSE(d && d->panel, ESP_ERR_INVALID_STATE, TAG, "panel no inicializado");
    const esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = cb,
    };
    return esp_lcd_dpi_panel_register_event_callbacks(d->panel, &cbs, ctx);
}

esp_err_t adv_board_display_set_on(adv_display_t *d, bool on)
{
    ESP_RETURN_ON_FALSE(d && d->io, ESP_ERR_INVALID_STATE, TAG, "panel no inicializado");
    return esp_lcd_panel_io_tx_param(d->io, on ? 0x29 : 0x28, NULL, 0);
}
