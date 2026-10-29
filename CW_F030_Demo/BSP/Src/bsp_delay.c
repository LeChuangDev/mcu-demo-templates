/**
 * @file  bsp_delay.c —— 延时实现（CW32F030 / Cortex-M0+ 版）
 *
 *  毫秒: SysTick 定时器，1ms 中断一次（HAL_Delay 就是干这个的）
 *  微秒: SysTick 的 24 位递减计数器直接读数，按主频换算
 *
 * ★ 为什么微秒不用 DWT->CYCCNT？
 *   Cortex-M0+ 的 DWT 是精简版，很多实现里没有 CYCCNT 周期计数器
 *   （CW32F030 就不带），写 DWT->CYCCNT 既编译不过也读不到数。
 *   所以这里复用 SysTick：1ms 中断保证毫秒计时，同时读 VAL 做微秒级
 *   轮询。SysTick 由 HCLK 驱动，64MHz 下分辨率约 15.6ns。
 */
#include "bsp.h"
#include "cw32f030_conf.h"

static volatile uint32_t g_ms_tick = 0;    /* 毫秒计数值，中断里累加 */

/* 启动文件 startup_cw32f030.s 里 SysTick_Handler 是弱符号，
   这里定义强符号，链接时自动覆盖它 */
void SysTick_Handler(void)
{
    g_ms_tick++;
}

void bsp_delay_init(void)
{
    /* SysTick 每 1ms 中断一次：
       SystemCoreClock/1000 = 64MHz 下每 1ms 计 64000 个数。
       必须在 bsp_clock_init() 之后调用，此时 SystemCoreClock 已更新为 64MHz */
    (void)SysTick_Config(SystemCoreClock / 1000UL);
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
    /* SysTick->VAL 从 LOAD 递减到 0 后自动重装，循环往复。
       1us 的计数值 = SystemCoreClock / 1MHz = 64（64MHz 时）。
       这里按"相邻两次读数走了多少个时钟"累加，跨过重装点也能正确计算，
       因此不受 24 位计数器翻转影响。 */
    uint32_t load    = SysTick->LOAD;
    uint32_t last    = SysTick->VAL;
    uint32_t elapsed = 0;
    uint32_t ticks   = us * (SystemCoreClock / 1000000UL);

    while (elapsed < ticks)
    {
        uint32_t now = SysTick->VAL;

        if (now <= last)
        {
            /* 正常递减（含 now == last 的零步进） */
            elapsed += (last - now);
        }
        else
        {
            /* 跨过 0 并重装：last -> 0 需要 last+1 个时钟，
               重装到 LOAD 后再到 now 需要 LOAD-now 个时钟 */
            elapsed += last + (load - now) + 1UL;
        }
        last = now;
    }
}
