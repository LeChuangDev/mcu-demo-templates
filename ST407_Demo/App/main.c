/**
 * @file  main.c —— 寄存器版最小主程序
 * 流程: 时钟初始化 -> 延时初始化 -> 主循环(延时 + 位带演示)
 */
#include "bsp.h"
#include "bsp_bitband.h"
#include "stm32f4xx.h"

int main(void)
{
    /* 第 1 步：时钟 168MHz（SystemInit 已在启动文件里先跑过，
       它把系统置于 HSI 16MHz 并开了 FPU，这里再切到 HSE+PLL） */
    bsp_clock_init();

    /* 第 2 步：SysTick + DWT 延时初始化 */
    bsp_delay_init();

    /* 第 3 步：位带演示用的 GPIO 准备（PB2 推挽输出）
       不需要这个演示的话，下面到 while 之前的 GPIO 代码都可以删 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;            /* 开 GPIOB 时钟 */ 
    (void)RCC->AHB1ENR;
    GPIOB->MODER &= ~(3u << (2u * 2u));            /* 清模式位 */
    GPIOB->MODER |=  (1u << (2u * 2u));            /* 01 = 通用输出 */

    while (1)
    {
        /* 位带翻转 PB2：读位 -> 取反 -> 写回，原子操作 */
        BB_REG(GPIOB->ODR, 2) = !BB_REG(GPIOB->ODR, 2);

        bsp_delay_ms(500);                          /* 延时 500ms */
    }
}