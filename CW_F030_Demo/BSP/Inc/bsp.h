/**
 * @file  bsp.h —— 板级接口（时钟 / 延时）
 * 规则：App 层只允许 include 这个文件（需要寄存器时再 include cw32f030_conf.h）
 *
 * 目标板：立创·地文星 CW32F030C8T6（Cortex-M0+，64K Flash / 8K SRAM）
 */
#ifndef __BSP_H
#define __BSP_H

#include <stdint.h>

/* ---- 时钟 ---- */
void bsp_clock_init(void);          /* HSE 8MHz -> PLL x8 -> 64MHz */

/* ---- 延时（HAL 时代 HAL_Delay 的替代品）---- */
void     bsp_delay_init(void);      /* 初始化 SysTick(1ms)，需在 bsp_clock_init 之后调用 */
uint32_t bsp_tick_ms(void);         /* 获取开机以来的毫秒数 */
void     bsp_delay_ms(uint32_t ms); /* 毫秒延时（阻塞） */
void     bsp_delay_us(uint32_t us); /* 微秒延时（阻塞，基于 SysTick 计数器） */

#endif /* __BSP_H */
