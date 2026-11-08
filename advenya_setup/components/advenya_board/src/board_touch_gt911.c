/*
 * ADVENYA OS · tactil GT911 por I2C (GPIO7 SDA / GPIO8 SCL / GPIO3 RST)
 *
 * Adaptacion a ESP-IDF 5.5.5 (driver/i2c_master.h) de flexTouchInit(), gtPoll(),
 * gtReadRes(), gtBusGuard() e i2cBusRecover() de FlexOS_Ultra_HAL.h:
 *
 *  - Wire.* -> i2c_master_bus / i2c_master_dev con plazo de 8 ms por
 *    transaccion (I2C_BUS_TIMEOUT_MS de Flex) en lugar de 50 ms.
 *  - Deteccion en 0x5D y, si no contesta, en 0x14.
 *  - Lectura de la resolucion configurada (0x8048) y escalado a 480x800 si
 *    difiere; los ejes cambiados no se escalan (se avisa), como en Flex.
 *  - Lectura de estado 0x814E (bit7 = frame nuevo, bits3..0 = dedos), punto 1
 *    en 0x8150 y limpieza del estado escribiendo 0 en 0x814E.
 *  - Recuperacion del bus compartido tras 8 lecturas fallidas seguidas, con 1 s
 *    de enfriamiento: se libera el periferico, se dan hasta 9 pulsos de SCL en
 *    drenador abierto, condicion de STOP manual y se vuelve a montar el bus;
 *    si SDA queda libre pero el GT911 no contesta, se le da su pulso de reset.
 *    Nunca se da el tactil por perdido para siempre.
 *
 * Todo se llama desde UN solo hilo (la tarea de LVGL), igual que en Flex.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "sdkconfig.h"
#include "advenya_board.h"

static const char *TAG = "adv_gt911";

#define GT_REG_STATUS     0x814E
#define GT_REG_POINT1     0x8150
#define GT_REG_PRODUCT_ID 0x8140
#define GT_REG_CFG_RES    0x8048

#define GT_FAIL_RECOVER_N   8
#define GT_RECOVER_GAP_MS   1000
#define I2C_REC_HALF_US     10

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static adv_touch_info_t s_info;
static uint16_t s_res_x = ADV_LCD_H_RES, s_res_y = ADV_LCD_V_RES;
static uint8_t s_fails;
static int64_t s_recover_us;

static esp_err_t bus_create(void)
{
    const i2c_master_bus_config_t cfg = {
        .i2c_port = -1,                     /* puerto libre automatico */
        .sda_io_num = ADV_PIN_TP_SDA,
        .scl_io_num = ADV_PIN_TP_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &s_bus);
}

static void bus_destroy(void)
{
    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (s_bus) {
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
    }
}

static esp_err_t dev_attach(uint8_t addr)
{
    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    const i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = ADV_TP_I2C_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(s_bus, &dcfg, &s_dev);
    if (err == ESP_OK) {
        s_info.addr = addr;
    }
    return err;
}

static bool probe(uint8_t addr)
{
    return s_bus && i2c_master_probe(s_bus, addr, ADV_TP_I2C_TIMEOUT_MS) == ESP_OK;
}

static bool gt_rd(uint16_t reg, uint8_t *buf, size_t n)
{
    if (!s_dev) {
        return false;
    }
    const uint8_t r[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
    return i2c_master_transmit_receive(s_dev, r, 2, buf, n, ADV_TP_I2C_TIMEOUT_MS) == ESP_OK;
}

static bool gt_wr(uint16_t reg, uint8_t val)
{
    if (!s_dev) {
        return false;
    }
    const uint8_t b[3] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val};
    return i2c_master_transmit(s_dev, b, 3, ADV_TP_I2C_TIMEOUT_MS) == ESP_OK;
}

static void gt_reset_pulse(uint32_t high_ms)
{
    gpio_set_level(ADV_PIN_TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(ADV_PIN_TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(high_ms));
}

static void gt_read_res(void)
{
    uint8_t cfg[4];
    if (!gt_rd(GT_REG_CFG_RES, cfg, 4)) {
        return;
    }
    uint16_t rx = (uint16_t)(cfg[0] | (cfg[1] << 8));
    uint16_t ry = (uint16_t)(cfg[2] | (cfg[3] << 8));
#if CONFIG_ADV_BOARD_TOUCH_SWAP_XY
    uint16_t t = rx; rx = ry; ry = t;
#endif
    s_info.cfg_res_x = rx;
    s_info.cfg_res_y = ry;
    if (rx < 64 || rx > 4096 || ry < 64 || ry > 4096) {
        return;     /* valor absurdo (lectura fallida o clon): se usa la pantalla */
    }
    if ((rx > ry) != (ADV_LCD_H_RES > ADV_LCD_V_RES)) {
        ESP_LOGW(TAG, "GT911 configurado a %ux%u: ejes cambiados (revisa ADV_BOARD_TOUCH_SWAP_XY)", rx, ry);
        return;
    }
    s_res_x = rx;
    s_res_y = ry;
    if (rx != ADV_LCD_H_RES || ry != ADV_LCD_V_RES) {
        ESP_LOGI(TAG, "GT911 configurado a %ux%u: se escala a %dx%d", rx, ry, ADV_LCD_H_RES, ADV_LCD_V_RES);
    }
}

esp_err_t adv_board_touch_init(void)
{
    memset(&s_info, 0, sizeof(s_info));
    const gpio_config_t rst = {.pin_bit_mask = 1ULL << ADV_PIN_TP_RST, .mode = GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&rst), TAG, "gpio RST");
    gt_reset_pulse(100);          /* LOW 10 ms, HIGH 100 ms (flexTouchInit) */

    ESP_RETURN_ON_ERROR(bus_create(), TAG, "i2c_new_master_bus");

    uint8_t addr = 0;
    if (probe(ADV_TP_ADDR_PRIMARY)) {
        addr = ADV_TP_ADDR_PRIMARY;
    } else if (probe(ADV_TP_ADDR_SECONDARY)) {
        addr = ADV_TP_ADDR_SECONDARY;
    }
    if (!addr) {
        ESP_LOGW(TAG, "GT911 NO detectado (0x5D/0x14): se continua sin tactil");
        return ESP_ERR_NOT_FOUND;     /* fallo suave: el bus queda montado */
    }
    ESP_RETURN_ON_ERROR(dev_attach(addr), TAG, "add_device");
    s_info.present = true;
    uint8_t pid[4] = {0};
    gt_rd(GT_REG_PRODUCT_ID, pid, 4);
    memcpy(s_info.product_id, pid, 4);
    s_info.product_id[4] = 0;
    gt_read_res();
    ESP_LOGI(TAG, "GT911 en 0x%02X, ID '%.4s', configurado %ux%u", addr, s_info.product_id,
             s_info.cfg_res_x, s_info.cfg_res_y);
    return ESP_OK;
}

/* ---- recuperacion del bus (i2cBusRecover de Flex) ----------------------- */

static void od_release(gpio_num_t pin)
{
    gpio_set_level(pin, 1);           /* en drenador abierto, 1 = soltar la linea */
}

static void od_low(gpio_num_t pin)
{
    gpio_set_level(pin, 0);
}

static bool bus_recover(void)
{
    bus_destroy();
    const gpio_config_t od = {
        .pin_bit_mask = (1ULL << ADV_PIN_TP_SDA) | (1ULL << ADV_PIN_TP_SCL),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&od);
    od_release(ADV_PIN_TP_SDA);
    od_release(ADV_PIN_TP_SCL);
    esp_rom_delay_us(I2C_REC_HALF_US);
    for (int i = 0; i < 9 && gpio_get_level(ADV_PIN_TP_SDA) == 0; i++) {
        od_low(ADV_PIN_TP_SCL);
        esp_rom_delay_us(I2C_REC_HALF_US);
        od_release(ADV_PIN_TP_SCL);
        esp_rom_delay_us(I2C_REC_HALF_US);
    }
    bool freed = gpio_get_level(ADV_PIN_TP_SDA) != 0;
    /* STOP manual: con SCL alto, SDA pasa de 0 a 1 */
    od_low(ADV_PIN_TP_SDA);
    esp_rom_delay_us(I2C_REC_HALF_US);
    od_release(ADV_PIN_TP_SCL);
    esp_rom_delay_us(I2C_REC_HALF_US);
    od_release(ADV_PIN_TP_SDA);
    esp_rom_delay_us(I2C_REC_HALF_US);
    gpio_reset_pin(ADV_PIN_TP_SDA);
    gpio_reset_pin(ADV_PIN_TP_SCL);
    if (bus_create() != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo volver a montar el bus I2C");
    }
    s_info.bus_recoveries++;
    return freed;
}

static void bus_guard(void)
{
    if (s_fails < GT_FAIL_RECOVER_N) {
        return;
    }
    int64_t now = esp_timer_get_time();
    if (s_recover_us && (now - s_recover_us) < (int64_t)GT_RECOVER_GAP_MS * 1000) {
        return;
    }
    s_recover_us = now;
    uint8_t addr = s_info.addr ? s_info.addr : ADV_TP_ADDR_PRIMARY;
    bool freed = bus_recover();
    if (probe(addr) && dev_attach(addr) == ESP_OK) {
        s_fails = 0;
        ESP_LOGW(TAG, "bus I2C recuperado");
        return;
    }
    if (!freed) {
        return;           /* SDA sigue a masa: lo retiene otro esclavo; se reintenta luego */
    }
    gt_reset_pulse(60);
    const uint8_t addrs[2] = {ADV_TP_ADDR_PRIMARY, ADV_TP_ADDR_SECONDARY};
    for (int i = 0; i < 2; i++) {
        if (probe(addrs[i]) && dev_attach(addrs[i]) == ESP_OK) {
            s_fails = 0;
            s_info.present = true;
            ESP_LOGW(TAG, "GT911 recuperado tras reset en 0x%02X", addrs[i]);
            return;
        }
    }
}

adv_touch_state_t adv_board_touch_read(uint16_t *x, uint16_t *y)
{
    if (!s_info.present) {
        return ADV_TOUCH_NO_DATA;
    }
    uint8_t status = 0;
    if (!gt_rd(GT_REG_STATUS, &status, 1)) {
        s_info.read_errors++;
        if (s_fails < 255) {
            s_fails++;
        }
        bus_guard();
        return ADV_TOUCH_NO_DATA;
    }
    s_fails = 0;
    if (!(status & 0x80)) {
        return ADV_TOUCH_NO_DATA;
    }
    adv_touch_state_t res = ADV_TOUCH_RELEASED;
    uint8_t n = status & 0x0F;
    if (n >= 1) {
        uint8_t d[6];
        if (gt_rd(GT_REG_POINT1, d, sizeof(d))) {
            uint16_t gx = (uint16_t)(d[0] | (d[1] << 8));
            uint16_t gy = (uint16_t)(d[2] | (d[3] << 8));
#if CONFIG_ADV_BOARD_TOUCH_SWAP_XY
            uint16_t t = gx; gx = gy; gy = t;
#endif
            if (s_res_x != ADV_LCD_H_RES) {
                gx = (uint16_t)((uint32_t)gx * ADV_LCD_H_RES / s_res_x);
            }
            if (s_res_y != ADV_LCD_V_RES) {
                gy = (uint16_t)((uint32_t)gy * ADV_LCD_V_RES / s_res_y);
            }
#if CONFIG_ADV_BOARD_TOUCH_FLIP_X
            gx = (ADV_LCD_H_RES - 1) - gx;
#endif
#if CONFIG_ADV_BOARD_TOUCH_FLIP_Y
            gy = (ADV_LCD_V_RES - 1) - gy;
#endif
            if (gx > ADV_LCD_H_RES - 1) {
                gx = ADV_LCD_H_RES - 1;
            }
            if (gy > ADV_LCD_V_RES - 1) {
                gy = ADV_LCD_V_RES - 1;
            }
            *x = gx;
            *y = gy;
            res = ADV_TOUCH_PRESSED;
        } else {
            s_info.read_errors++;
            res = ADV_TOUCH_NO_DATA;
        }
    }
    gt_wr(GT_REG_STATUS, 0);      /* limpiar el estado tras leer */
    return res;
}

void adv_board_touch_get_info(adv_touch_info_t *info)
{
    if (info) {
        *info = s_info;
    }
}

i2c_master_bus_handle_t adv_board_i2c_bus(void)
{
    return s_bus;
}
