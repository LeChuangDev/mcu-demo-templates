/**
 * @file  bsp_delay.c —— 延时实现（GD32E230 / Cortex-M23）
 *  毫秒: SysTick 定时器，1ms 中断一次（HAL_Delay 就是干这个的）
 *  微秒: TIMER5 基本定时器，1MHz 递增计数，不依赖内核调试单元
 *
 * ★ 为什么不用 DWT->CYCCNT ★
 *   Cortex-M23 属于 ARMv8-M Baseline，CMSIS 的 core_cm23.h 里 DWT 结构体
 *   【没有 CYCCNT 成员】（该计数器在 M23 上是可选项，CMSIS 按不存在处理），
 *   照抄 M4 的写法会直接编译报错。所以微秒延时改用片内 TIMER5：
 *   72MHz / 72 = 1MHz，16 位计数器最大 65535us，超长延时按 50ms 分块。
 */
#include "bsp.h"
#include "gd32e23x.h"

#define US_TIMER           TIMER5
#define US_TIMER_CHUNK_US  50000u   /* < 65536，保证 16 位计数差值不歧义 */

static volatile uint32_t g_ms_tick = 0;    /* 毫秒计数值，中断里累加 */

/* 启动文件 startup_gd32e23x.S 里 SysTick_Handler 是弱符号，
   我们在这里定义强符号，链接时自动覆盖它 */
void SysTick_Handler(void)
{
    g_ms_tick++;
}

void bsp_delay_init(void)
{
    /* 第 1 步：SysTick 每 1ms 中断一次
       SystemCoreClock/1000 = 72MHz 下每个 1ms 计 72000 个数
       （SysTick 是 24 位，最大 16777215，当前主频下不会溢出） */
    (void)SysTick_Config(SystemCoreClock / 1000UL);

    /* 第 2 步：开 TIMER5 时钟（挂在 APB1 上），读回一次等外设总线同步 */
    RCU_APB1EN |= RCU_APB1EN_TIMER5EN;
    (void)RCU_APB1EN;

    /* 第 3 步：TIMER5 配成 1MHz 自由递增计数
       分频值 = 定时器输入时钟 / 1MHz - 1
       APB1 分频为 1 或 2 时，定时器时钟都等于 HCLK(72MHz)：
         APB1 ÷1 -> CK_TIMER = 72MHz
         APB1 ÷2 -> CK_TIMER = 2 x 36MHz = 72MHz
       所以这里直接用 SystemCoreClock 换算 */
    uint32_t psc = SystemCoreClock / 1000000UL;
    if (psc == 0u) {
        psc = 1u;                       /* 主频低于 1MHz 时按最快档兜底 */
    }

    TIMER_CTL0(US_TIMER) = 0u;                          /* 先停计数 */
    TIMER_PSC(US_TIMER)  = psc - 1u;                    /* 1MHz */
    TIMER_CAR(US_TIMER)  = 0xFFFFu;                     /* 满量程自动重装 */
    TIMER_SWEVG(US_TIMER) = TIMER_SWEVG_UPG;            /* 立即装载分频值 */
    TIMER_CNT(US_TIMER)  = 0u;                          /* 计数清零 */
    TIMER_CTL0(US_TIMER) = TIMER_CTL0_CEN;              /* 开始计数 */
}

uint32_t bsp_tick_ms(void)
{
    return g_ms_tick;
}

void bsp_delay_ms(uint32_t ms)
{
    /* 记录起点，等差值 >= 目标值。
       用无符号减法做差值：即使 32 位计数翻转（49 天后）也不会出错。
       WFI 让 CPU 在等待期间睡眠，SysTick（或任意其它中断）到来时唤醒；
       醒来后重新判断，不会因为其它中断提前退出。 */
    uint32_t start = bsp_tick_ms();
    while ((bsp_tick_ms() - start) < ms) {
        __WFI();
    }
}

void bsp_delay_us(uint32_t us)
{
    /* TIMER5 每 1us 加 1，16 位计数器约 65.5ms 回绕一次。
       单段最多等 50ms，用 (uint16_t)(now - start) 的无符号差值计算，
       回绕也能算对；更长的延时拆成多段。 */
    while (us > 0u) {
        uint32_t chunk = (us > US_TIMER_CHUNK_US) ? US_TIMER_CHUNK_US : us;
        uint16_t start = (uint16_t)TIMER_CNT(US_TIMER);

        while ((uint16_t)((uint16_t)TIMER_CNT(US_TIMER) - start) < (uint16_t)chunk) {
        }
        us -= chunk;
    }
}
