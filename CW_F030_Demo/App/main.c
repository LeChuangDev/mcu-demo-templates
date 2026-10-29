/*
 * @Author: 樂創 dev@zhangwenle.com
 * @Date: 2026-10-06 22:42:36
 * @LastEditors: 樂創 dev@zhangwenle.com
 * @LastEditTime: 2026-10-29
 * @FilePath: \CW_F030_Demo\App\main.c
 * @Description: CW32F030C8T6（立创·地文星开发板）纯寄存器最小主程序
 */
/**
 * @file  main.c —— 寄存器版最小主程序（CW32F030C8T6）
 * 流程: 时钟初始化(HSE 8M -> PLL x8 = 64M) -> 延时初始化 -> 配 PC13 -> 500ms 闪灯
 *
 * 板载 LED：PC13，阳极经限流电阻接 VDD，输出【低电平】点亮。
 *
 * ★ Cortex-M0+ 没有位带(bit-band)，不能像 M3/M4 那样用 BB_REG 原子写单个位。
 *   CW32 的 GPIO 自带原子置位/清位/翻转寄存器，效果相同：
 *     BSRR 的 [15:0] 写 1 -> 置高   BSRR 的 [31:16] 写 1 -> 清低
 *     BRR           写 1 -> 清低
 *     TOG           写 1 -> 翻转
 *   这些寄存器硬件只改指定位，不影响同端口其他引脚，也不怕中断打断。
 */
#include "bsp.h"
#include "cw32f030_conf.h"

int main(void)
{
    /* 第 1 步：时钟 64MHz
       官方 SystemInit() 进 main 前只加载 HSI/LSI Trim、把 SystemCoreClock
       设为 8MHz（HSI/6），不配 PLL，所以必须在这里调 bsp_clock_init() 完成
       HSE 8MHz -> PLL x8 = 64MHz 的正式配置 */
    bsp_clock_init();

    /* 第 2 步：SysTick 1ms 中断 + 微秒延时初始化（依赖上一步的主频结果） */
    bsp_delay_init();

    /* 第 3 步：PC13 推挽输出，先拉低点亮 LED
       CW32 的 GPIO 配置寄存器是按位组织的（与 STM32F4 风格类似）：
         AHBEN  : 在 SYSCTRL 里开 GPIOC 外设时钟
         ANALOG : 0 = 数字功能
         DIR    : 0 = 输出
         OPENDRAIN: 0 = 推挽
         SPEED  : 1 = 高速
       下面移位序列与官方 gpio_init() 完全一致，见 cw32f030_gpio.c */
    CW_SYSCTRL->AHBEN_f.GPIOC = 1U;                 /* 开 GPIOC 时钟（AHBEN bit6） */
    (void)CW_SYSCTRL->AHBEN;                        /* 读一次，等总线同步 */

    CW_GPIOC->ANALOG    &= ~(uint32_t)BOARD_LED_PIN;  /* PC13 数字功能 */
    CW_GPIOC->DIR       &= ~(uint32_t)BOARD_LED_PIN;  /* PC13 输出 */
    CW_GPIOC->OPENDRAIN &= ~(uint32_t)BOARD_LED_PIN;  /* 推挽 */
    CW_GPIOC->SPEED     |=  (uint32_t)BOARD_LED_PIN;  /* 高速 */

    /* 第 4 步：拉低 PC13 点亮 LED（BRR 写 1 原子清位） */
    CW_GPIOC->BRR = (uint32_t)BOARD_LED_PIN;

    /* 第 5 步：主循环 500ms 翻转一次，验证 SysTick 延时是否准确 */
    while (1)
    {
        bsp_delay_ms(500);
        CW_GPIOC->TOG = (uint32_t)BOARD_LED_PIN;    /* TOG 写 1 原子翻转 */
    }
}
