/**
 * @file  bsp_clock.c —— 纯寄存器配置系统时钟
 * 目标: HSE 8MHz -> PLL -> SYSCLK 168MHz
 *        AHB=168MHz / APB1=42MHz / APB2=84MHz（F407 的三大极限值）
 * 参考: RM0090 参考手册 6.3 节时钟树图
 */
#include "bsp.h"
#include "stm32f4xx.h"          /* 全部寄存器定义 */

/* 老版 SPL V1.8.1 的头文件只给了 RCC_PLLCFGR_PLLM 这种【掩码】，
   没有新版 Cube 头里的 RCC_PLLCFGR_PLLM_Pos 【位移】宏。
   这里按 RM0090 7.4.2 节 RCC_PLLCFGR 的位布局自行定义移位量。
   （以后若换成 Cube 新版器件头，可以删掉本块、直接用 *_Pos 宏） */
#define RCC_PLLCFGR_PLLM_SHIFT   0u    /* PLLM 字段位域: bit[5:0]  */
#define RCC_PLLCFGR_PLLN_SHIFT   6u    /* PLLN 字段位域: bit[14:6] */
#define RCC_PLLCFGR_PLLP_SHIFT  16u    /* PLLP 字段位域: bit[17:16]，00 = ÷2 */
#define RCC_PLLCFGR_PLLQ_SHIFT  24u    /* PLLQ 字段位域: bit[27:24] */

void bsp_clock_init(void)
{
    /* 第 1 步：打开 PWR 时钟，调压器切到 Scale1
       F407 的电压调压器分 3 档：Scale1 才能跑到 168MHz（Scale2 上限 144M） */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;      /* PWR 挂在 APB1 总线上，先给时钟 */
    (void)RCC->APB1ENR;                     /* 读一次，等总线同步 */
    PWR->CR |= PWR_CR_VOS;                  /* VOS=0b11 即 Scale1 */
    while (!(PWR->CSR & PWR_CSR_VOSRDY)) { } /* 等调压器就绪 */

    /* 第 2 步：Flash 等待周期
       168MHz 且 3.3V 供电时需要 5 个等待周期（RM0090 表 3）
       顺便开预取、指令缓存、数据缓存（运行速度明显提升） */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS
               | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN
               | FLASH_ACR_DCEN;

    /* 第 3 步：总线分频
       HPRE=÷1  -> HCLK  168MHz
       PPRE2=÷2 -> APB2   84MHz（外设最大 84MHz）
       PPRE1=÷4 -> APB1   42MHz（外设最大 42MHz）
       先清空再赋值：这几个字段复位值不是 0，必须清 */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1
               | RCC_CFGR_PPRE2_DIV2
               | RCC_CFGR_PPRE1_DIV4;

    /* 第 4 步：PLL 参数（F407 的 PLL 是三段式：M 分频 -> N 倍频 -> P 分频）
       8MHz ÷ PLLM(8) = 1MHz         输入级，要求落进 1~2MHz
       1MHz × PLLN(336) = 336MHz     中间级 VCO，要求落进 100~432MHz
       336MHz ÷ PLLP(2) = 168MHz     输出级，只能是 2/4/6/8
       PLLQ=7 -> 48MHz，给 USB 预留（不用 USB 也建议按规范填） */
    RCC->PLLCFGR = (8u   << RCC_PLLCFGR_PLLM_SHIFT) /* PLLM 复位值是 16，必须显式写 */
                 | (336u << RCC_PLLCFGR_PLLN_SHIFT)
                 | (0u   << RCC_PLLCFGR_PLLP_SHIFT) /* 字段值 00 = ÷2 */
                 | (7u   << RCC_PLLCFGR_PLLQ_SHIFT)
                 | RCC_PLLCFGR_PLLSRC_HSE;          /* PLL 时钟源 = HSE */

    /* 第 5 步：打开 HSE 晶振，等它稳定 */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) { }

    /* 第 6 步：打开 PLL，等它锁定 */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { }

    /* 第 7 步：把系统时钟源切换到 PLL，等切换完成
       SWS 是"当前实际在用的时钟源"状态位，必须等它变成 PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }

    /* 第 8 步：更新 SystemCoreClock 全局变量
       system_stm32f4xx.c 维护的变量，bsp_delay_us 用它换算周期数
       不调这句的话，DWT 微秒延时会按旧频率算，时间全错 */
    SystemCoreClockUpdate();
}