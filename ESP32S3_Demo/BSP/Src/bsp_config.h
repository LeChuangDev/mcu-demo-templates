/**
 * @file  bsp_config.h —— 板级配置（换板子只改这个文件）
 *
 * BSP 内部头文件，由 bsp_led.c / bsp_pwm.c 包含；App 层不需要修改。
 * 改完引脚或频率后重新编译即可。
 */
#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

#include "driver/gpio.h"

/* ===================== 板载 LED ===================== */
#define BSP_LED_GPIO          GPIO_NUM_2
#define BSP_LED_ACTIVE_LEVEL  1      /* 1 = 高电平点亮；低电平点亮的板改成 0 */

/* ===================== PWM 测试输出 ===================== */
#define BSP_PWM_GPIO          GPIO_NUM_4
#define BSP_PWM_FREQ_HZ       1000   /* 方波频率（Hz） */
#define BSP_PWM_DUTY_PERCENT  50     /* 初始占空比 0~100 */

#endif /* BSP_CONFIG_H */
