/**
 * @file  bsp_bitband.h —— Cortex-M0+ 没有位带，改用 GPIO 原子寄存器
 *
 * ★ 与 Cortex-M3/M4 模板的重要区别 ★
 *   位带(bit-band)是 Cortex-M3/M4 的可选特性，Cortex-M0/M0+ 内核
 *   【不实现】它：0x22000000 / 0x42000000 别名区在 CW32F030 上不存在，
 *   按老模板的 BB_REG() 写下去不会报错，但访问的是无效地址（HardFault）。
 *   所以本文件不再提供 BITBAND_xxx / BB_REG 宏。
 *
 * CW32 的 GPIO 自带原子置位/清位/翻转寄存器，效果与位带相同：
 *   BSRR 低 16 位写 1 -> 置高；BSRR 高 16 位写 1 -> 清低
 *   BRR  写 1 -> 清低
 *   TOG  写 1 -> 翻转
 * 硬件只改指定位，不影响同端口其他引脚，也不会被中断打断。
 *
 * 用法：
 *   BSP_GPIO_SET(CW_GPIOC, BOARD_LED_PIN);
 *   BSP_GPIO_CLR(CW_GPIOC, BOARD_LED_PIN);
 *   BSP_GPIO_TOG(CW_GPIOC, BOARD_LED_PIN);
 *   if (BSP_GPIO_READ(CW_GPIOC, BOARD_LED_PIN)) { ... }
 */
#ifndef __BSP_BITBAND_H
#define __BSP_BITBAND_H

#include <stdint.h>

/* port 必须是 CW_GPIOx 指针；pin 是 GPIO_PIN_x 位掩码（可为多位组合） */
#define BSP_GPIO_SET(port, pin)   ((port)->BSRR = (uint32_t)(pin))
#define BSP_GPIO_CLR(port, pin)   ((port)->BRR  = (uint32_t)(pin))
#define BSP_GPIO_TOG(port, pin)   ((port)->TOG  = (uint32_t)(pin))
#define BSP_GPIO_READ(port, pin)  (((port)->IDR & (uint32_t)(pin)) != 0U)

#endif /* __BSP_BITBAND_H */
