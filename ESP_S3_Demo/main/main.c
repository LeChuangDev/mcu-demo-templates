/**
 * @file  main.c —— App 层最小主程序
 * 流程: LED/PWM 初始化 -> LED 闪烁 + PWM 输出 + 周期打印运行状态
 *
 * App 层只依赖 bsp.h，不直接操作 GPIO，也不包含 ESP-IDF 的驱动头文件；
 * 换板子时改 BSP/Src，main.c 不动。
 */
#include "bsp.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_idf_version.h"

static const char *TAG = "app";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 demo started, IDF %s", esp_get_idf_version());

    /* 第 1 步：板载 LED（引脚与有效电平见 BSP/Src/bsp_led.c） */
    bsp_led_init();

    /* 第 2 步：PWM 测试输出（引脚/频率/占空比见 BSP/Src/bsp_config.h） */
    bsp_pwm_init();

    /* 第 3 步：主循环。bsp_delay_ms 内部走 FreeRTOS，CPU 不会空转 */
    uint32_t count = 0;
    while (1)
    {
        bsp_led_toggle();

        ESP_LOGI(TAG, "heartbeat %u, free heap %u bytes",
                 (unsigned)count++, (unsigned)esp_get_free_heap_size());

        bsp_delay_ms(1000);
    }
}
