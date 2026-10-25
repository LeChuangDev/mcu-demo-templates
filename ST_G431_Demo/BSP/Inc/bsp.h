/**
 * @file  bsp.h —— 板级接口（时钟/延时/LED）
 * 规则：App 层只允许 include 这个文件
 */
#ifndef __BSP_H
#define __BSP_H

#include <stdint.h>

/* ---- 时钟 ---- */
int bsp_clock_init(void);           /* 0 = 170MHz 就绪；非 0 = 已回退 HSI（仍可运行） */

/* ---- 延时（HAL 时代 HAL_Delay 的替代品）---- */
void     bsp_delay_init(void);      /* 初始化 SysTick(1ms) 和 DWT(微秒) */
uint32_t bsp_tick_ms(void);         /* 获取开机以来的毫秒数 */
void     bsp_delay_ms(uint32_t ms); /* 毫秒延时（阻塞，等待时 CPU 进 WFI 睡眠） */
void     bsp_delay_us(uint32_t us); /* 微秒延时（阻塞，DWT 实现，支持任意长度） */

/* ---- 板载 LED（端口/引脚/有效电平在 bsp_led.c 顶部配置）---- */
void bsp_led_init(void);
void bsp_led_on(void);
void bsp_led_off(void);
void bsp_led_toggle(void);

#endif /* __BSP_H */
