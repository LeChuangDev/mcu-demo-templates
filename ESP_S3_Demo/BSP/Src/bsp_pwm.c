/**
 * @file  bsp_pwm.c —— PWM 测试输出（LEDC）
 *
 * 上电后输出固定频率和占空比的方波，用于示波器/逻辑分析仪确认固件
 * 确实烧录并运行。引脚、频率、占空比都在 BSP/Src/bsp_config.h 中配置。
 */
#include "bsp.h"
#include "bsp_config.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

#define BSP_PWM_RESOLUTION  LEDC_TIMER_10_BIT
#define BSP_PWM_TIMER       LEDC_TIMER_0
#define BSP_PWM_CHANNEL     LEDC_CHANNEL_0
#define BSP_PWM_DUTY_MAX    ((1U << 10) - 1U) /* 10 bit -> 1023 */

static const char *TAG = "bsp_pwm";

void bsp_pwm_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = BSP_PWM_TIMER,
        .duty_resolution = BSP_PWM_RESOLUTION,
        .freq_hz         = BSP_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    const uint32_t duty = (BSP_PWM_DUTY_MAX * BSP_PWM_DUTY_PERCENT) / 100U;
    const ledc_channel_config_t channel = {
        .gpio_num   = BSP_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BSP_PWM_CHANNEL,
        .timer_sel  = BSP_PWM_TIMER,
        .duty       = duty,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    ESP_LOGI(TAG, "PWM test output: GPIO%d, %d Hz, %d%% duty",
             (int)BSP_PWM_GPIO, (int)BSP_PWM_FREQ_HZ, (int)BSP_PWM_DUTY_PERCENT);
}

void bsp_pwm_set_duty(uint8_t percent)
{
    if (percent > 100) { percent = 100; }

    uint32_t duty = (BSP_PWM_DUTY_MAX * percent) / 100U;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, BSP_PWM_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, BSP_PWM_CHANNEL));
}
