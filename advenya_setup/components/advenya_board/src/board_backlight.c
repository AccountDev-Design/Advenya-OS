/*
 * ADVENYA OS · retroiluminacion por PWM (LEDC) en GPIO23
 *
 * Adaptacion de setBacklight()/ledcAttach(PIN_LCD_BL, 20000, 8) de
 * FlexOS_Ultra_HAL.h: 20 kHz, 8 bits, y el mismo mapeo 5..100 % -> 25..255 de
 * duty (el suelo de 25 evita dejar la pantalla invisible). 0 % apaga del todo.
 * Si el LEDC no se puede configurar, se recurre a GPIO fijo (encendido/apagado),
 * igual que el "fallback" de Flex.
 */
#include "esp_log.h"
#include "esp_check.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "advenya_board.h"

static const char *TAG = "adv_backlight";

#define BL_MODE    LEDC_LOW_SPEED_MODE
#define BL_TIMER   LEDC_TIMER_0
#define BL_CHANNEL LEDC_CHANNEL_0
#define BL_RES     LEDC_TIMER_8_BIT

static bool s_pwm_ok;
static int s_percent;

esp_err_t adv_board_backlight_init(void)
{
    /* Apagada durante el arranque (evita el destello blanco del panel) */
    const gpio_config_t io = {.pin_bit_mask = 1ULL << ADV_PIN_LCD_BL, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&io);
    gpio_set_level(ADV_PIN_LCD_BL, 0);

    const ledc_timer_config_t tcfg = {
        .speed_mode = BL_MODE,
        .duty_resolution = BL_RES,
        .timer_num = BL_TIMER,
        .freq_hz = ADV_BL_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t ccfg = {
        .gpio_num = ADV_PIN_LCD_BL,
        .speed_mode = BL_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    esp_err_t err = ledc_timer_config(&tcfg);
    if (err == ESP_OK) {
        err = ledc_channel_config(&ccfg);
    }
    s_pwm_ok = (err == ESP_OK);
    if (!s_pwm_ok) {
        ESP_LOGW(TAG, "LEDC no disponible (%s): retroiluminacion encendido/apagado", esp_err_to_name(err));
    }
    s_percent = 0;
    return ESP_OK;
}

void adv_board_backlight_set(int percent)
{
    if (percent <= 0) {
        percent = 0;
    } else if (percent < 5) {
        percent = 5;
    } else if (percent > 100) {
        percent = 100;
    }
    s_percent = percent;
    if (!s_pwm_ok) {
        gpio_set_level(ADV_PIN_LCD_BL, percent > 0);
        return;
    }
    uint32_t duty = percent == 0 ? 0 : (uint32_t)(25 + (percent * (255 - 25)) / 100);
    ledc_set_duty(BL_MODE, BL_CHANNEL, duty);
    ledc_update_duty(BL_MODE, BL_CHANNEL);
}

int adv_board_backlight_get(void)
{
    return s_percent;
}
