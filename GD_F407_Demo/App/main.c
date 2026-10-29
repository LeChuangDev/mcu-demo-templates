/**
 * @file  main.c —— 寄存器版最小主程序（GD32F407VG）
 * 流程: 时钟初始化 -> 延时初始化 -> LED 闪烁
 *
 * App 层只依赖 bsp.h，不直接包含芯片头、也不操作任何寄存器；
 * 换板子时改 BSP，换芯片时改 Core/Drivers，main.c 不动。
 */
#include "bsp.h"

int main(void)
{
    /* 第 1 步：时钟 168MHz
       SystemInit 已按 25MHz HXTAL 配过一遍；bsp_clock_init 会先切到 HXTAL、
       关 PLL，再完整重配，保证 PLL 参数以 BSP 为准。
       返回非 0 表示 HXTAL/PLL/PMU 某一步超时，SystemCoreClock 已按实际
       时钟更新，程序仍可继续运行。 */
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
