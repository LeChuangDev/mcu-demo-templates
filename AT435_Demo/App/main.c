/*
 * @Author: 樂創 dev@zhangwenle.com
 * @Date: 2026-10-06 22:42:36
 * @LastEditors: 樂創 dev@zhangwenle.com
 * @LastEditTime: 2026-10-25 19:58:24
 * @FilePath: \AT435_Demo\App\main.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
 * @file  main.c —— 寄存器版最小主程序（AT32F435CGT7）
 * 流程: 时钟初始化(HEXT 8M->PLL 288M) -> 延时初始化 -> 配 PA9/PA10 -> 拉低点亮 LED
 */
#include "bsp.h"
#include "bsp_bitband.h"
#include "at32f435_437.h"

int main(void)
{
    /* 第 1 步：时钟 288MHz
       注意：AT 的 SystemInit() 进 main 前只把时钟复位到 HICK 8MHz 默认态
       （不配 PLL），所以必须在这里调 bsp_clock_init() 完成正式时钟配置 */
    bsp_clock_init();

    /* 第 2 步：SysTick + DWT 延时初始化（依赖上一步的主频结果） */
    bsp_delay_init();

    /* 第 3 步：PA9 / PA10 推挽输出，拉低点亮 LED
       LED 接法：阳极接 VDD -> 限流电阻 -> LED -> GPIO，输出低电平点亮。
       AT32 的 GPIO 是 F4 风格的【分立寄存器】，不是 F1 的 CFGHR 一锅端：
         cfgr  每 pin 2 位模式(00输入/01输出/10复用/11模拟)
         omode 每 pin 1 位输出类型(0推挽/1开漏)
         odrvr 每 pin 2 位驱动能力(0正常/1更强)
         pull  每 pin 2 位上下拉(0无/1上拉/2下拉)
       下面的移位序列与官方 gpio_init() 完全一致，见 at32f435_437_gpio.c */
    CRM->ahben1_bit.gpioaen = TRUE;            /* 开 GPIOA 时钟（AHB1EN bit0） */
    (void)CRM->ahben1;                         /* 读一次，等总线同步 */

    /* PA9 推挽输出 */
    GPIOA->cfgr  &= ~(0x03UL << (9U * 2U));    /* 清 PA9 模式位 */
    GPIOA->cfgr  |=  GPIO_MODE_OUTPUT << (9U * 2U);  /* 01 = 通用输出 */
    GPIOA->omode &= ~(0x01UL << 9U);           /* PA9 推挽（清 = 推挽） */
    GPIOA->odrvr &= ~(0x03UL << (9U * 2U));    /* 清 PA9 驱动能力位 */
    GPIOA->odrvr |=  GPIO_DRIVE_STRENGTH_STRONGER << (9U * 2U); /* 01 = 更强驱动 */
    GPIOA->pull  &= ~(0x03UL << (9U * 2U));    /* PA9 无上下拉 */

    /* PA10 推挽输出 */
    GPIOA->cfgr  &= ~(0x03UL << (10U * 2U));   /* 清 PA10 模式位 */
    GPIOA->cfgr  |=  GPIO_MODE_OUTPUT << (10U * 2U); /* 01 = 通用输出 */
    GPIOA->omode &= ~(0x01UL << 10U);          /* PA10 推挽 */
    GPIOA->odrvr &= ~(0x03UL << (10U * 2U));   /* 清 PA10 驱动能力位 */
    GPIOA->odrvr |=  GPIO_DRIVE_STRENGTH_STRONGER << (10U * 2U); /* 01 = 更强驱动 */
    GPIOA->pull  &= ~(0x03UL << (10U * 2U));   /* PA10 无上下拉 */

    /* 拉低 PA9 / PA10 输出低电平，点亮 LED
       位带写 0 = 原子地只清该位，不影响同端口其他引脚。
       也可用 GPIOA->clr（0x28）原子清位寄存器：GPIOA->clr = (1u<<9)|(1u<<10); */
    BB_REG(GPIOA->odt, 9)  = 0;
    BB_REG(GPIOA->odt, 10) = 0;
    while (1)
    {
        /* LED 已点亮，主循环空转保持 */
    }
}
