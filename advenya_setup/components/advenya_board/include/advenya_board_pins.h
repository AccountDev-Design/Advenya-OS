/*
 * ADVENYA OS · Placa GUITION JC4880P443C_I_W (ESP32-P4)
 * -----------------------------------------------------
 * Datos de hardware VERIFICADOS en la implementacion de referencia de
 * Flex OS Ultra (rama claude/flexos-ota-system-blq9g9-2csacx):
 *
 *   FlexOS_Ultra/FlexOS_Ultra_Types.h  -> SCR_W/SCR_H, PIN_LCD_RST, PIN_LCD_BL,
 *                                         PIN_TP_SDA, PIN_TP_SCL, PIN_TP_RST
 *   FlexOS_Ultra/FlexOS_Ultra_HAL.h    -> LDO, bus DSI, timings DPI, tabla DCS,
 *                                         protocolo GT911, recuperacion del bus
 *   FlexOS_Ultra/FlexOS_Ultra.ino      -> CPU 360 MHz, flash 16 MB QIO 80 MHz,
 *                                         PSRAM habilitada
 *
 * Lo que NO esta verificado se marca como tal y no se usa.
 */
#pragma once

#include "driver/gpio.h"

/* ---- Panel (ST7701 por MIPI-DSI) ---------------------------------------- */
#define ADV_LCD_H_RES              480      /* SCR_W, retrato nativo        */
#define ADV_LCD_V_RES              800      /* SCR_H                        */
#define ADV_LCD_BITS_PER_PIXEL     16       /* COLMOD 0x55 -> RGB565        */

#define ADV_PIN_LCD_RST            GPIO_NUM_5   /* reset del ST7701           */
#define ADV_PIN_LCD_BL             GPIO_NUM_23  /* retroiluminacion (PWM)     */

/* Alimentacion del PHY MIPI: LDO interno del P4, canal 3 a 2,5 V */
#define ADV_MIPI_PHY_LDO_CHAN      3
#define ADV_MIPI_PHY_LDO_MV        2500

/* Bus DSI */
#define ADV_DSI_LANES              2
#define ADV_DSI_LANE_MBPS          500

/* Video DPI (FlexOS_Ultra_HAL.h, flexPanelInit) */
#define ADV_DPI_CLOCK_MHZ          34
#define ADV_DPI_HSYNC_PW           12
#define ADV_DPI_HSYNC_BP           42
#define ADV_DPI_HSYNC_FP           42
#define ADV_DPI_VSYNC_PW           2
#define ADV_DPI_VSYNC_BP           8
#define ADV_DPI_VSYNC_FP           166
/* Refresco resultante: 34 MHz / ((480+12+42+42) * (800+2+8+166)) = ~60,5 Hz */

/* Retroiluminacion por LEDC: 20 kHz, 8 bits (ledcAttach(PIN_LCD_BL, 20000, 8)) */
#define ADV_BL_PWM_FREQ_HZ         20000

/* ---- Tactil (GT911 capacitivo por I2C) ---------------------------------- */
#define ADV_PIN_TP_SDA             GPIO_NUM_7
#define ADV_PIN_TP_SCL             GPIO_NUM_8
#define ADV_PIN_TP_RST             GPIO_NUM_3
/* INT del GT911: Flex OS Ultra NUNCA lo cablea y documenta que su GPIO no esta
 * confirmado en ninguna fuente (FlexOS_Ultra_Types.h, POFF_WAKE_GPIO = -1).
 * Se trabaja por sondeo, igual que la referencia. */
#define ADV_PIN_TP_INT             GPIO_NUM_NC

#define ADV_TP_I2C_HZ              400000   /* Wire.begin(..., 400000)            */
#define ADV_TP_I2C_TIMEOUT_MS      8        /* I2C_BUS_TIMEOUT_MS                 */
#define ADV_TP_ADDR_PRIMARY        0x5D
#define ADV_TP_ADDR_SECONDARY      0x14

/* El bus I2C (GPIO7/GPIO8) es COMPARTIDO en la placa con el codec ES8311 (y,
 * en el montaje de Flex, con un IMU externo). Este proyecto solo usa el GT911;
 * cualquier otro driver que se anada debe usar adv_board_i2c_bus() y hablar
 * desde el MISMO hilo, como establece docs/BUS-I2C-COMPARTIDO.md de Flex. */
