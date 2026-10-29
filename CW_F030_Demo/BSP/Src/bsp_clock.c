/**
 * @file  bsp_clock.c —— 纯寄存器配置系统时钟（CW32F030C8T6 版）
 *
 * 目标：HSE 8MHz -> PLL x8 -> SYSCLK 64MHz（CW32F030 最高主频）
 *       HCLK = PCLK = 64MHz（HCLK/PCLK 均不分频）
 *
 * 参考：武汉芯源 CW32F030 标准外设库 V2.2
 *       Libraries/src/cw32f030_rcc.c 中
 *       RCC_HSE_Enable / RCC_PLL_Enable / RCC_SysClk_Switch 的权威序列，
 *       本文件按寄存器逐条展开（不编译官方 .c）。
 *
 * ★ CW32 时钟配置的 4 个关键点（移植时最容易踩的坑）：
 *   1) SYSCTRL 的 CR0/CR1/CR2/IER 是【带 KEY 保护】的寄存器：
 *      写入时必须把 0x5A5A 放在高 16 位（SYSCTRL_BYPASS），否则写不进去。
 *      HSE / PLL / HSI 这些配置寄存器则不需要 KEY。
 *   2) 外设时钟使能寄存器 AHBEN/APBENx 不需要 KEY，直接按位写即可。
 *   3) Flash 等待周期要跟着主频走：<=24MHz 可 0 等待，<=48MHz 1 个等待，
 *      64MHz 需要 3 个等待周期（FLASH_Latency_3，WAIT 字段值 2）。
 *      等待周期非 0 时官方建议同时打开 FETCH + CACHE（本文件已开）。
 *   4) PLL 输入/输出档位由 FREQIN/FREQOUT 两个字段描述，不是简单的
 *      倍数关系：8MHz 属于 FREQIN=1 档，64MHz 属于 FREQOUT=4 档。
 *
 * 注意：官方 SystemInit()（Core/System/system_cw32f030.c）进 main 前
 *       只加载 HSI/LSI 的出厂 Trim 值、把 SystemCoreClock 设为 8MHz，
 *       【不配置 PLL】；真正的 64MHz 配置在这里完成。
 */
#include "bsp.h"
#include "cw32f030_conf.h"

/* 等待时钟稳定的超时计数：约几十毫秒，防止晶振不起振时死等 */
#define CLOCK_TIMEOUT   (0x000FFFFFUL)

void bsp_clock_init(void)
{
    uint32_t timeout;

    /* -----------------------------------------------------------------
     * 第 1 步：打开 FLASH 时钟并设置 3 个等待周期
     *   CR2 带 KEY（0x5A5A 在高 16 位），WAIT=2 表示 3 个等待周期；
     *   FETCH/CACHE 在等待周期非 0 时建议一起打开（官方 FLASH_SetLatency）
     * ----------------------------------------------------------------- */
    CW_SYSCTRL->AHBEN_f.FLASH = 1U;
    CW_FLASH->CR2 = 0x5A5A0000UL
                  | FLASH_Latency_3
                  | FLASH_Prefetch_Enable
                  | FLASH_Cache_Enable;

    /* -----------------------------------------------------------------
     * 第 2 步：HSE 8MHz 晶振（板载 Y1）
     *   PF0/PF1 先切成模拟功能（晶振引脚），再配置 HSE 参数：
     *     DETCNT      = 8GHz / 8MHz = 1000（起振检测计数）
     *     MODE        = 0   晶振模式（1 = 外部时钟输入）
     *     WAITCYCLE   = 131072 个周期（官方默认）
     *     FREQRANGE   = 1   8~16MHz 档
     *     DRIVER      = 2   正常驱动
     *     FLT         = 0   关闭强干扰滤波
     *   然后 CR1 置 HSEEN（同时开 CLKCCS/HSECCS 时钟失效自动切换）
     * ----------------------------------------------------------------- */
    CW_SYSCTRL->AHBEN_f.GPIOF = 1U;
    CW_GPIOF->ANALOG |= (uint32_t)(bv0 | bv1);

    CW_SYSCTRL->HSE = ((8000000000UL / BOARD_HSE_VALUE) << 8)
                    | (0UL << SYSCTRL_HSE_MODE_Pos)
                    | (0x2UL << SYSCTRL_HSE_WAITCYCLE_Pos)
                    | (1UL << SYSCTRL_HSE_FREQRANGE_Pos)
                    | (RCC_HSE_DRIVER_NORMAL << 0)
                    | (0UL << 7);                                    /* FLT 关 */

    CW_SYSCTRL->CR1 |= SYSCTRL_BYPASS
                     | RCC_SYSCTRL_CLKCCS
                     | RCC_SYSCTRL_HSECCS
                     | RCC_SYSCTRL_HSEEN;

    timeout = CLOCK_TIMEOUT;
    while ((CW_SYSCTRL->HSE & SYSCTRL_HSE_STABLE_Msk) == 0U)
    {
        if (--timeout == 0U)
        {
            /* 晶振没起来：保持复位默认的 HSI 8MHz 运行，延时函数仍可用 */
            return;
        }
    }

    /* -----------------------------------------------------------------
     * 第 3 步：PLL：HSE 8MHz × 8 = 64MHz
     *   SOURCE   = 0     HSE 晶振
     *   FREQIN   = 1     8~12MHz 输入档
     *   MUL      = 8     倍频 x8
     *   FREQOUT  = 4     48~72MHz 输出档
     *   WAITCYCLE= 7     16384 周期（与官方 RCC_PLL_Enable 默认一致）
     *   然后 CR1 置 PLLEN，等 PLL STABLE
     * ----------------------------------------------------------------- */
    CW_SYSCTRL->PLL = (0x7UL << SYSCTRL_PLL_WAITCYCLE_Pos)
                    | (4UL << SYSCTRL_PLL_FREQOUT_Pos)
                    | (8UL << SYSCTRL_PLL_MUL_Pos)
                    | (1UL << SYSCTRL_PLL_FREQIN_Pos)
                    | (0UL << SYSCTRL_PLL_SOURCE_Pos);

    CW_SYSCTRL->CR1 |= SYSCTRL_BYPASS | RCC_SYSCTRL_PLLEN;

    timeout = CLOCK_TIMEOUT;
    while ((CW_SYSCTRL->PLL & SYSCTRL_PLL_STABLE_Msk) == 0U)
    {
        if (--timeout == 0U)
        {
            return;             /* PLL 没锁定：继续用 HSI 8MHz */
        }
    }

    /* -----------------------------------------------------------------
     * 第 4 步：HCLK/PCLK 不分频，SYSCLK 切到 PLL
     *   CR0 带 KEY；先清掉旧的 SYSCLK 选择位再写新值（官方 RCC_SysClk_Switch）
     * ----------------------------------------------------------------- */
    {
        uint32_t cr0 = CW_SYSCTRL->CR0;

        cr0 &= ~SYSCTRL_BYPASS_MASK;        /* 清高 16 位 KEY */
        cr0 &= ~SYSCTRL_CR0_SYSCLK_Msk;     /* 清旧时钟源选择 */
        cr0 |= SYSCTRL_BYPASS
             | RCC_HCLK_DIV1
             | RCC_PCLK_DIV1
             | RCC_SYSCLKSRC_PLL;           /* SYSCLK = PLL */
        CW_SYSCTRL->CR0 = cr0;
    }

    /* 切换成功后才更新 CMSIS 全局变量，bsp_delay 用它换算 1ms 计数值 */
    if ((CW_SYSCTRL->CR0 & SYSCTRL_CR0_SYSCLK_Msk) == RCC_SYSCLKSRC_PLL)
    {
        SystemCoreClock = BOARD_SYSCLK_HZ;
    }
}
