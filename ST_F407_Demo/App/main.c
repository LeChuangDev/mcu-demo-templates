/**
 * @file  main.c —— 寄存器版最小主程序
 * 流程: 时钟初始化 -> 延时初始化 -> LED 闪烁
 *
 * App 层只依赖 bsp.h，不直接包含芯片头、也不操作任何寄存器；
 * 换板子时改 BSP，换芯片时改 Core/Drivers，main.c 不动。
 */
#include "bsp.h"

int main(void)
{
    /* 第 1 步：时钟 168MHz
       SystemInit 已先配过一遍；bsp_clock_init 会切回 HSI、关 PLL 后完整重配，
       保证 PLL 参数以 BSP 为准。返回非 0 表示 HSE/PLL 超时、已回退 HSI，
       程序仍可继续运行（延时按实际主频换算）。 */
    (void)bsp_clock_init();

    /* 第 2 步：SysTick + DWT 延时初始化 */
    bsp_delay_init();

    /* 第 3 步：板载 LED（引脚与有效电平见 BSP/Src/bsp_led.c） */
    bsp_led_init();

    /* 第 4 步：主循环。bsp_delay_ms 内部用 WFI 睡眠，CPU 不会空转发热 */
    while (1)
    {
        bsp_led_toggle();
        bsp_delay_ms(500);
    }
}
