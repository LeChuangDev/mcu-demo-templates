/**
 * @file  bsp_delay.c —— 延时实现
 *  毫秒: SysTick 定时器，1ms 中断一次（HAL_Delay 就是干这个的）
 *  微秒: DWT 硬件计数器，计数频率 = CPU 主频，不占任何外设
 */
#include "bsp.h"
#include "stm32f4xx.h"

static volatile uint32_t g_ms_tick = 0;    /* 毫秒计数值，中断里累加 */

/* 启动文件 startup_stm32f407xx.s 里 SysTick_Handler 是弱符号，
   我们在这里定义强符号，链接时自动覆盖它 */
void SysTick_Handler(void)
{
    g_ms_tick++;
}

void bsp_delay_init(void)
{
    /* 第 1 步：SysTick 每 1ms 中断一次
       SystemCoreClock/1000 = 168MHz 下每个 1ms 计 168000 个数 */
    SysTick_Config(SystemCoreClock / 1000UL);

    /* 第 2 步：启动 DWT 周期计数器
       注意：Cortex-M4 的 DWT【没有 LAR 锁寄存器】（LAR=0xC5ACCE55 解锁是
       Cortex-M7 及以后内核才有的步骤，写在 M4 上会直接访问到不存在的寄存器
       而编译报错）。M4 上电后 DWT 默认可用，使能 TRCENA 后直接开计数器即可 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  /* 开调试跟踪单元 */
    DWT->CYCCNT = 0;                                 /* 计数清零 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             /* 开始计数 */
}

uint32_t bsp_tick_ms(void)
{
    return g_ms_tick;
}

void bsp_delay_ms(uint32_t ms)
{
    /* 记录起点，等差值 >= 目标值。
       用无符号减法做差值：即使 32 位计数翻转（49 天后）也不会出错 */
    uint32_t start = bsp_tick_ms();
    while ((bsp_tick_ms() - start) < ms) { }
}

void bsp_delay_us(uint32_t us)
{
    /* DWT->CYCCNT 每个 CPU 周期 +1，168MHz 下 1us = 168 个计数。
       同样用差值法，计数翻转（约 25.6 秒一次）也不受影响 */
    uint32_t start  = DWT->CYCCNT;
    uint32_t ticks  = us * (SystemCoreClock / 1000000UL);
    while ((DWT->CYCCNT - start) < ticks) { }
}