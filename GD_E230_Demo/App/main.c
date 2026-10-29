/**
 * @file  main.c —— 寄存器版最小主程序（GD32E230C8 / 立创开发板）
 * 流程: 时钟初始化 -> 延时初始化 -> LED 闪烁
 *
 * App 层只依赖 bsp.h，不直接包含芯片头、也不操作任何寄存器；
 * 换板子时改 BSP，换芯片时改 Core/Drivers，main.c 不动。
 */
#include "bsp.h"

int main(void)
{
    /* 第 1 步：时钟 72MHz
       SystemInit 已按 8MHz HXTAL 配过一遍；bsp_clock_init 会先切回 IRC8M、
       关 PLL，再完整重配，保证 PLL 参数以 BSP 为准。
       返回非 0 表示 HXTAL/PLL 某一步超时，SystemCoreClock 已按实际时钟更新，
       程序仍可继续运行。 */
    (void)bsp_clock_init();

    /* 第 2 步：SysTick(1ms) + TIMER5(微秒) 延时初始化 */
    bsp_delay_init();

    /* 第 3 步：板载 LED（PC13，高电平点亮；见 BSP/Src/bsp_led.c） */
    bsp_led_init();

    /* 第 4 步：主循环。bsp_delay_ms 内部用 WFI 睡眠，CPU 不会空转发热 */
    while (1)
    {
        bsp_led_toggle();
        bsp_delay_ms(500);
    }
}
