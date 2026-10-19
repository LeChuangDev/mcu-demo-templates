/**
 * @file  main.c —— 寄存器版最小主程序（GD32F407VG）
 * 流程: 时钟初始化 -> 延时初始化 -> 主循环(延时 + 位带演示)
 */
#include "bsp.h"
#include "bsp_bitband.h"
#include "gd32f4xx.h"

int main(void)
{
    /* 第 1 步：时钟 168MHz
       注意：启动文件里的 SystemInit() 已经按 HXTAL 25MHz + PLL 配到 168MHz
       （和 ST 模板只停在 HSI 不同），bsp_clock_init() 是同参数的完整重配，
       同时设置好 FMC 等待周期并刷新 SystemCoreClock */
    bsp_clock_init();

    /* 第 2 步：SysTick + DWT 延时初始化 */
    bsp_delay_init();

    /* 第 3 步：位带演示用的 GPIO 准备（PB2 推挽输出）
       不需要这个演示的话，下面到 while 之前的 GPIO 代码都可以删。
       GD 寄存器风格：端口是地址常量 GPIOB，寄存器用"宏(端口)"访问，
       没有 ST 那种 GPIOB->MODER 结构体。 */
    RCU_AHB1EN |= RCU_AHB1EN_PBEN;             /* 开 GPIOB 时钟 */
    (void)RCU_AHB1EN;                          /* 读一次，等总线同步 */
    GPIO_CTL(GPIOB) &= ~GPIO_MODE_MASK(2);     /* 清 PB2 模式位 */
    GPIO_CTL(GPIOB) |=  GPIO_MODE_SET(2, GPIO_MODE_OUTPUT); /* 01 = 通用输出 */

    while (1)
    {
        /* 位带翻转 PB2：读位 -> 取反 -> 写回，原子操作。
           ST 写的是 GPIOB->ODR，GD 的输出寄存器叫 OCTL，
           但地址相同(端口基址+0x14)，位带别名地址完全通用。
           GD 还提供硬件翻转寄存器 GPIO_TG(GPIOB) = GPIO_TG_TG2，可替代之 */
        BB_REG(GPIO_OCTL(GPIOB), 2) = !BB_REG(GPIO_OCTL(GPIOB), 2);

        bsp_delay_ms(500);                     /* 延时 500ms */
    }
}
