/**
 * @file  cw32f030_conf.h —— 本工程裁剪版器件/板级配置头
 *
 * 背景：CW32 官方头文件不像 AT32/ST 那样在 cw32f030.h 末尾自动 include
 * 一个 conf.h，所以这个文件由 BSP / App 主动 include，作用是把本工程
 * 实际用到的【寄存器结构体头】聚合到一起，并写清板级常量。
 *
 * 本工程是纯寄存器工程（不编译官方 StdPeriph 的 .c，只借寄存器结构体
 * 和位域），目前只开 3 个外设：
 *   RCC    时钟复位（SYSCTRL）
 *   GPIO   通用 IO
 *   FLASH  Flash 等待周期（64MHz 需要 3 个等待周期）
 * SysTick 直接用 CMSIS core_cm0plus.h 里的 SysTick_Config()，无需额外头。
 *
 * 以后用到新外设时，把官方 Libraries/inc/cw32f030_xxx.h 放进
 * Drivers/CW32F0xx_StdPeriph/Include/ 并在这里 include 即可。
 */
#ifndef __CW32F030_CONF_H
#define __CW32F030_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cw32f030.h"
#include "cw32f030_gpio.h"
#include "cw32f030_rcc.h"
#include "cw32f030_flash.h"

/* =====================================================================
 *  板级常量：立创·地文星 CW32F030C8T6 开发板
 * ===================================================================== */

/* 板载高速晶振 8MHz（HSE）。HSE_VALUE 由 BSP/CMakeLists.txt 传
 * -DHSE_VALUE=8000000，cw32f030_rcc.h 里已加 #if 保护允许覆盖。 */
#define BOARD_HSE_VALUE         ((uint32_t)8000000)

/* 两处数值必须一致：CMake 的 BOARD_HSE_VALUE 生成 -DHSE_VALUE，
 * C 代码用 BOARD_HSE_VALUE 配 HSE 寄存器。改晶振时两处一起改，
 * 不一致就在编译期报错，而不是运行后才表现为串口波特率不对。 */
#if defined(HSE_VALUE) && (HSE_VALUE != 8000000U)
#error "HSE_VALUE 与 BOARD_HSE_VALUE 不一致：请同时修改 chip.cmake 和 cw32f030_conf.h"
#endif

/* 系统时钟目标：HSE 8MHz × 8 = 64MHz（CW32F030 最高主频） */
#define BOARD_SYSCLK_HZ         ((uint32_t)64000000)

/* 用户 LED：PC13，阳极经限流电阻接 VDD，输出低电平点亮 */
#define BOARD_LED_PORT          CW_GPIOC
#define BOARD_LED_PIN           GPIO_PIN_13

/* 板载 3 个用户按键 K1/K2/K3 的具体引脚随批次略有差异，用到时查原理图；
 * 调试串口 UART3 接板载 CH340：PA9=TX, PA10=RX。 */

#ifdef __cplusplus
}
#endif

#endif /* __CW32F030_CONF_H */
