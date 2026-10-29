/**
 * @file  bsp_clock.c —— 纯寄存器配置系统时钟 (STM32H743)
 * 目标: HSE 25MHz -> PLL1 -> SYSCLK 400MHz
 *        CPU = 400MHz，AXI/HCLK = 200MHz，APB1/2/3/4 = 100MHz
 * 参考: RM0433 参考手册 "Reset and clock control (RCC)" 章节
 *
 * ★ H7 与 G4/F4 时钟树的关键差异（换芯片时最容易踩的坑）★
 *   1) PLL 分成 PLL1/PLL2/PLL3 三套独立配置，各有一套 M/N/P/Q/R；
 *      SYSCLK 固定从 PLL1 的 P 输出（PLL1P）来。
 *   2) PLL 输入分频 DIVM 写在 RCC_PLLCKSELR，倍频/分频写在 RCC_PLL1DIVR，
 *      输入频率范围（PLL1RGE）和 VCO 范围（PLL1VCOSEL）写在 RCC_PLLCFGR。
 *   3) 供电电压 VOS 在 PWR->D3CR（不在 PWR->CR1），400MHz 用 VOS1；
 *      480MHz 才需要 VOS0，本模板不碰。
 *   4) Flash 等待周期按 AXI 频率（不是 CPU 频率）计算：
 *      400MHz 经 HPRE=÷2 后 AXI=200MHz，VOS1 下 4WS 一定安全
 *      （RM0433 表：VOS1 最大 225MHz 仍可用 4WS）。
 *   5) 0x0000_0000 是 ITCM 而不是 Flash 别名，所以 SystemInit 里
 *      必须显式设置 VTOR（见 Core/System/system_stm32h7xx.c）。
 *
 * ★ 与 SystemInit() 的关系 ★
 *   CMSIS 的 SystemInit() 把 RCC 恢复成复位状态（HSI 64MHz）并设置 VTOR，
 *   但不配置 PLL。本函数不假设当前时钟源，先切回 HSI、关闭 PLL1，再完整重配：
 *     1) PLL 使能时 PLLCKSELR/PLL1DIVR/PLLCFGR 不可写，必须先关；
 *     2) 先降回 HSI 再改分频，避免高频下出现瞬时超频；
 *     3) 所有等待都带超时，HSE/PLL 异常时回退 HSI，不会卡死在 main 里。
 */
#include "bsp.h"
#include "stm32h7xx.h"          /* 全部寄存器定义（STM32CubeH7 CMSIS 器件头） */

/* 等待超时：即使还在 64MHz HSI 上，也足够覆盖晶振起振（典型 1~2ms）。
   超时不是为了精确计时，只是避免晶振缺失/参数错误时永久卡死。 */
#define CLOCK_WAIT_TIMEOUT  500000u

/* PLL1 参数（按 25MHz HSE 计算，换晶振时改这里）
   5MHz 输入 -> 800MHz VCO -> 400MHz SYSCLK
   约束：PLL 输入 1~16MHz，VCO 宽范围 192~836MHz，VCO 倍频 N 4~512 */
#define CLOCK_PLLM          5u      /* 25MHz ÷ 5 = 5MHz  (PLL 输入，4~8MHz 档) */
#define CLOCK_PLLN          160u    /* 5MHz × 160 = 800MHz (VCO)                  */
#define CLOCK_PLLP          2u      /* 800MHz ÷ 2 = 400MHz (SYSCLK)               */
#define CLOCK_PLLQ          4u      /* 800MHz ÷ 4 = 200MHz（本模板不使能 Q 输出） */
#define CLOCK_PLLR          2u      /* 800MHz ÷ 2 = 400MHz（本模板不使能 R 输出） */

/* Flash 等待周期 + 写高频档位
   AXI=200MHz、VOS1：4WS 覆盖到 225MHz，取保守值，宁可慢一点也不能少等；
   WRHIGHFREQ=0b10 对应 185~285MHz 的写访问档位（RM0433 FLASH_ACR 表）。 */
#define CLOCK_FLASH_LATENCY      FLASH_ACR_LATENCY_4WS
/* 头文件里 _0/_1 是【位】不是【数值】：WRHIGHFREQ=0b10 对应 FLASH_ACR_WRHIGHFREQ_1 */
#define CLOCK_FLASH_WRHIGHFREQ   FLASH_ACR_WRHIGHFREQ_1

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

/* H7 的 L1 Cache：I-Cache 零副作用；D-Cache 打开后 DMA 缓冲区需要
   做 cache 维护（或把缓冲区放到 MPU 配置的 non-cacheable 区域）。
   本模板没有 DMA，先开 I/D Cache 并强制 D-Cache 透写，兼顾性能和安全。 */
static void clock_cache_init(void)
{
#if (__ICACHE_PRESENT == 1)
    SCB_EnableICache();
#endif
#if (__DCACHE_PRESENT == 1)
    SCB_EnableDCache();
    SCB->CACR |= SCB_CACR_FORCEWT_Msk;      /* D-Cache 写透：写操作同时更新 RAM */
#endif
}

int bsp_clock_init(void)
{
    /* 第 1 步：内核电压 Scale 1（VOS=0b11），并等 VOSRDY
       必须先调压再升频；电压没就绪就切高频会直接跑飞。 */
    PWR->D3CR = (PWR->D3CR & ~PWR_D3CR_VOS)
              | PWR_D3CR_VOS_0 | PWR_D3CR_VOS_1;
    if (clock_wait(&PWR->D3CR, PWR_D3CR_VOSRDY, PWR_D3CR_VOSRDY) != 0) {
        SystemCoreClockUpdate();            /* 调压器未就绪，保持当前时钟 */
        return -1;
    }

    /* 第 2 步：打开 HSI，并把系统时钟切回 HSI
       切时钟源只改 SW 字段，必须先清再写，避免旧值和新值按位或出保留值。 */
    RCC->CR |= RCC_CR_HSION;
    if (clock_wait(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY) != 0) {
        return -2;
    }
    if ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
        RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
        if (clock_wait(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI) != 0) {
            SystemCoreClockUpdate();
            return -2;
        }
    }

    /* 第 3 步：关闭 PLL1，等 PLLRDY 真正回落（PLL 关闭时配置寄存器才可写） */
    RCC->CR &= ~RCC_CR_PLLON;
    if (clock_wait(&RCC->CR, RCC_CR_PLLRDY, 0u) != 0) {
        SystemCoreClockUpdate();
        return -3;
    }

    /* 第 4 步：总线分频（此刻跑在 HSI，改分频不会造成超频）
       D1CPRE=÷1 -> CPU 400MHz
       HPRE  =÷2 -> AXI/HCLK 200MHz
       APBx  =÷2 -> APB1/2/3/4 100MHz（都在各自上限内）
       这几个字段复位值不是 0，必须清空再赋值。 */
    RCC->D1CFGR = (RCC->D1CFGR & ~(RCC_D1CFGR_D1CPRE |
                                   RCC_D1CFGR_HPRE   |
                                   RCC_D1CFGR_D1PPRE))
                | RCC_D1CFGR_D1CPRE_DIV1
                | RCC_D1CFGR_HPRE_DIV2
                | RCC_D1CFGR_D1PPRE_DIV2;
    RCC->D2CFGR = (RCC->D2CFGR & ~(RCC_D2CFGR_D2PPRE1 |
                                   RCC_D2CFGR_D2PPRE2))
                | RCC_D2CFGR_D2PPRE1_DIV2
                | RCC_D2CFGR_D2PPRE2_DIV2;
    RCC->D3CFGR = (RCC->D3CFGR & ~RCC_D3CFGR_D3PPRE)
                | RCC_D3CFGR_D3PPRE_DIV2;

    /* 第 5 步：Flash 等待周期（升频前设好；HSI 下多等几拍没有副作用） */
    FLASH->ACR = (FLASH->ACR & ~(FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ))
               | CLOCK_FLASH_LATENCY
               | CLOCK_FLASH_WRHIGHFREQ;

    /* 第 6 步：打开 HSE 晶振，等它稳定（带超时） */
    RCC->CR |= RCC_CR_HSEON;
    if (clock_wait(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY) != 0) {
        SystemCoreClockUpdate();            /* 留在 HSI，让延时按实际频率计算 */
        return -4;
    }

    /* 第 7 步：PLL1 参数
       PLLSRC=HSE，DIVM1=5（5MHz 输入，选 4~8MHz 档），
       DIVN1=160（×160），DIVP1=2（÷2 -> 400MHz），
       PLL1VCOSEL=0（宽范围 VCO），DIVP1EN=1（必须使能 P 输出）。 */
    RCC->PLLCKSELR = (RCC->PLLCKSELR &
                      ~(RCC_PLLCKSELR_PLLSRC | RCC_PLLCKSELR_DIVM1))
                   | RCC_PLLCKSELR_PLLSRC_HSE
                   | ((uint32_t)CLOCK_PLLM << RCC_PLLCKSELR_DIVM1_Pos);

    RCC->PLL1DIVR = (((uint32_t)CLOCK_PLLN - 1u) << RCC_PLL1DIVR_N1_Pos)
                  | (((uint32_t)CLOCK_PLLP - 1u) << RCC_PLL1DIVR_P1_Pos)
                  | (((uint32_t)CLOCK_PLLQ - 1u) << RCC_PLL1DIVR_Q1_Pos)
                  | (((uint32_t)CLOCK_PLLR - 1u) << RCC_PLL1DIVR_R1_Pos);

    RCC->PLLCFGR = RCC_PLLCFGR_PLL1RGE_2      /* 输入 4~8MHz            */
                 | RCC_PLLCFGR_DIVP1EN;       /* 使能 P 输出 -> SYSCLK  */

    /* 第 8 步：打开 PLL1，等它锁定（带超时） */
    RCC->CR |= RCC_CR_PLLON;
    if (clock_wait(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY) != 0) {
        SystemCoreClockUpdate();
        return -5;
    }

    /* 第 9 步：把系统时钟源切换到 PLL1，等切换完成（带超时）
       SWS 是"当前实际在用的时钟源"状态位，必须等它变成 PLL1 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL1;
    if (clock_wait(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL1) != 0) {
        SystemCoreClockUpdate();
        return -6;
    }

    /* 第 10 步：使能 IO 补偿单元（H7 高速 IO 需要，低频时也可开）
       SYSCFG 挂在 APB4 上，先开时钟再写 CCCSR。 */
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;
    SYSCFG->CCCSR |= SYSCFG_CCCSR_EN;

    /* 第 11 步：更新 SystemCoreClock 全局变量（400MHz），再开 L1 Cache */
    SystemCoreClockUpdate();
    clock_cache_init();
    return 0;
}
