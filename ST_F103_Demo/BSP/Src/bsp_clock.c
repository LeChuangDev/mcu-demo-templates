/**
 * @file  bsp_clock.c —— 纯寄存器配置系统时钟 (STM32F103)
 * 目标: HSE 8MHz -> PLL x9 -> SYSCLK 72MHz
 *        AHB=72MHz / APB1=36MHz / APB2=72MHz
 * 参考: RM0008 参考手册 "Reset and clock control (RCC)" 章节
 *
 * ★ F1 与 G4/F4 时钟树的关键差异（换芯片时最容易踩的坑）★
 *   1) PLL 只有一级倍频：SYSCLK = PLL输入 × PLLMUL，没有 G4 的 M/N/R 分频，
 *      也没有 F4 的 P/Q 输出。F103 的 PLL 最高 72MHz。
 *   2) APB1 最高只有 36MHz！72MHz 主频下 PPRE1 必须 ÷2，
 *      而 APB2 可以跑满 72MHz（PPRE2 ÷1）。写错轻则外设超频，重则死机。
 *   3) Flash 等待周期：72MHz / 3.3V 需要 2WS（FLASH_ACR_LATENCY_2），
 *      并打开预取缓冲（PRFTBE）；F1 没有 G4 的 ICEN/DCEN 缓存位。
 *   4) ADC 时钟上限 14MHz，ADCPRE 必须取 PCLK2/6 = 12MHz（默认 /2 会超频）。
 *   5) USB 需要 48MHz：72MHz ÷ 1.5 正好，置 USBPRE=1。
 *
 * ★ 与 SystemInit() 的关系 ★
 *   StdPeriph 的 system_stm32f10x.c 默认已经配置到 72MHz（SYSCLK_FREQ_72MHz）。
 *   本函数仍然不假设当前时钟源，先切回 HSI、关闭 PLL，再完整重配：
 *     1) RM0008 规定 PLL 相关位只有在 PLL 关闭时才可写；
 *     2) 先降回 HSI 再改分频，避免高频下出现瞬时超频；
 *     3) 所有等待都带超时，HSE/PLL 异常时回退 HSI，不会卡死在 main 里。
 */
#include "bsp.h"
#include "stm32f10x.h"          /* 全部寄存器定义（StdPeriph CMSIS 器件头） */

/* 等待超时：即使还在 8MHz HSI 上，也足够覆盖晶振起振（典型 1~2ms）。
   超时不是为了精确计时，只是避免晶振缺失/参数错误时永久卡死。 */
#define CLOCK_WAIT_TIMEOUT  500000u

/* PLL 倍频（按 8MHz HSE 计算，换晶振只改这里：目标 72MHz 除以晶振频率）
   8MHz × 9 = 72MHz。12MHz 晶振就改成 RCC_CFGR_PLLMULL6。 */
#define CLOCK_PLL_MUL       RCC_CFGR_PLLMULL9

/* 等 reg 的 mask 位变成 value；超时返回 -1。
   reg 必须是 volatile 寄存器，编译器不会把循环优化掉。 */
static int clock_wait(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    uint32_t timeout = CLOCK_WAIT_TIMEOUT;
    while ((*reg & mask) != value) {
        if (--timeout == 0u) {
            return -1;
        }
    }
    return 0;
}

int bsp_clock_init(void)
{
    /* 第 1 步：打开 HSI。F1 的 RCC 在 AHB 上，不需要像 G4 那样先开 PWR 时钟 */
    RCC->CR |= RCC_CR_HSION;
    if (clock_wait(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY) != 0) {
        return -1;
    }

    /* 第 2 步：如果当前不在 HSI 上，先切回 HSI。
       切时钟源只改 SW 字段，必须先清再写，避免旧值和新值按位或出保留值。 */
    if ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
        RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
        if (clock_wait(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI) != 0) {
            SystemCoreClockUpdate();
            return -2;
        }
    }

    /* 第 3 步：关闭 PLL，等 PLLRDY 真正回落。
       RM0008：PLL 使能时 PLLSRC/PLLMUL 等配置位不可写，必须先关。 */
    RCC->CR &= ~RCC_CR_PLLON;
    if (clock_wait(&RCC->CR, RCC_CR_PLLRDY, 0u) != 0) {
        SystemCoreClockUpdate();
        return -3;
    }

    /* 第 4 步：Flash 等待周期
       72MHz 需要 2WS；顺便打开预取缓冲。
       在 8MHz HSI 下多等几拍没有副作用，所以先设好再升频。 */
    FLASH->ACR = (uint32_t)((FLASH->ACR & ~(uint32_t)FLASH_ACR_LATENCY)
                            | FLASH_ACR_LATENCY_2
                            | FLASH_ACR_PRFTBE);

    /* 第 5 步：总线分频 + PLL 参数（此刻跑在 HSI，改分频不会造成超频）
       HPRE=÷1  -> HCLK  72MHz
       PPRE1=÷2 -> APB1  36MHz（上限！）
       PPRE2=÷1 -> APB2  72MHz
       ADCPRE=÷6 -> ADC  12MHz（PCLK2 6 分频，上限 14MHz）
       PLLSRC=HSE（8MHz 不分频），PLLMUL=×9 -> PLL 输出 72MHz
       USBPRE=1.5 -> USB 48MHz
       这些字段复位值不是 0，必须清空再赋值。 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_SW | RCC_CFGR_HPRE | RCC_CFGR_PPRE1
                               | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE
                               | RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE
                               | RCC_CFGR_PLLMULL | RCC_CFGR_USBPRE))
              | RCC_CFGR_HPRE_DIV1
              | RCC_CFGR_PPRE1_DIV2
              | RCC_CFGR_PPRE2_DIV1
              | RCC_CFGR_ADCPRE_DIV6
              | RCC_CFGR_PLLSRC_HSE        /* PLLXTPRE=0，HSE 不预分频 */
              | CLOCK_PLL_MUL
              | RCC_CFGR_USBPRE;

    /* 第 6 步：打开 HSE 晶振，等它稳定（带超时） */
    RCC->CR |= RCC_CR_HSEON;
    if (clock_wait(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY) != 0) {
        SystemCoreClockUpdate();            /* 留在 HSI，让延时按 8MHz 计算 */
        return -4;
    }

    /* 第 7 步：打开 PLL，等它锁定（带超时） */
    RCC->CR |= RCC_CR_PLLON;
    if (clock_wait(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY) != 0) {
        SystemCoreClockUpdate();
        return -5;
    }

    /* 第 8 步：把系统时钟源切换到 PLL，等切换完成（带超时）
       SWS 是"当前实际在用的时钟源"状态位，必须等它变成 PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    if (clock_wait(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL) != 0) {
        SystemCoreClockUpdate();
        return -6;
    }

    /* 第 9 步：更新 SystemCoreClock 全局变量
       system_stm32f10x.c 维护的变量，bsp_delay_us 用它换算周期数 */
    SystemCoreClockUpdate();
    return 0;
}
