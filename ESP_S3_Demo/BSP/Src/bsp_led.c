/**
 * @file  bsp_led.c —— 板载 LED
 *
 * 引脚与有效电平在 BSP/Src/bsp_config.h 中配置，App 层不需要动。
 *
 * 注意：部分 ESP32-S3 开发板（如 DevKitC-1 v1.1）的板载 RGB 灯是 WS2812，
 * 接在 GPIO48 上，不能直接用 GPIO 电平控制（需要 RMT/LED 驱动）。
 * 若你的板子是这种，请把 LED 换成外部普通 LED 或改用 RMT 驱动。
 */
#include "bsp.h"
#include "bsp_config.h"

#include "driver/gpio.h"
#include "esp_err.h"

void bsp_led_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BSP_LED_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&cfg));
    bsp_led_off();
}

void bsp_led_on(void)
{
    gpio_set_level(BSP_LED_GPIO, BSP_LED_ACTIVE_LEVEL);
}

void bsp_led_off(void)
{
    gpio_set_level(BSP_LED_GPIO, !BSP_LED_ACTIVE_LEVEL);
}

void bsp_led_toggle(void)
{
    /* 读当前电平再取反写回；LED 输出脚做输入读取是 ESP32 支持的 */
    gpio_set_level(BSP_LED_GPIO, gpio_get_level(BSP_LED_GPIO) ? 0 : 1);
}
