/**
 * @file  bsp_delay.c —— 延时与计时实现
 *  ESP32 的时钟/节拍由 ESP-IDF 与 FreeRTOS 管好，这里只做薄封装，
 *  对应 ST407_Demo 里 bsp_delay.c 的位置。
 */
#include "bsp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

void bsp_delay_ms(uint32_t ms)
{
    /* 阻塞延时期间 FreeRTOS 会调度其它任务，CPU 不空转 */
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t bsp_tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}
