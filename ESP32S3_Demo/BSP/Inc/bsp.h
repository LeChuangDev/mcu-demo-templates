/**
 * @file  bsp.h —— 板级接口（延时 / LED）
 * 规则：App 层（main/）只允许 include 这个文件
 */
#ifndef __BSP_H
#define __BSP_H

#include <stdint.h>

/* ---- 延时（ESP-IDF 里靠 FreeRTOS，bsp 只做二次封装）---- */
void     bsp_delay_ms(uint32_t ms); /* 毫秒延时（阻塞，等待期间 CPU 让给其它任务） */
uint32_t bsp_tick_ms(void);         /* 获取开机以来的毫秒数 */

/* ---- 板载 LED（引脚与有效电平在 BSP/Src/bsp_led.c 顶部配置）---- */
void bsp_led_init(void);
void bsp_led_on(void);
void bsp_led_off(void);
void bsp_led_toggle(void);

#endif /* __BSP_H */
