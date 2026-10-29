/**
 * @file  bsp.h —— 板级接口（本次只有时钟/延时/位带）
 * 规则：App 层只允许 include 这个文件
 */
#ifndef __BSP_H
#define __BSP_H

#include <stdint.h>

/* ---- 时钟 ---- */
void bsp_clock_init(void);          /* HEXT 8MHz -> PLL -> 288MHz */

/* ---- 延时（HAL 时代 HAL_Delay 的替代品）---- */
void     bsp_delay_init(void);      /* 初始化 SysTick(1ms) 和 DWT(微秒) */
uint32_t bsp_tick_ms(void);         /* 获取开机以来的毫秒数 */
void     bsp_delay_ms(uint32_t ms); /* 毫秒延时（阻塞） */
void     bsp_delay_us(uint32_t us); /* 微秒延时（阻塞，DWT 实现） */

#endif /* __BSP_H */