/**
 * @file  bsp_clock.c —— 纯寄存器配置系统时钟（AT32F435CGT7 版）
 * 目标: HEXT 8MHz -> PLL -> SCLK 288MHz（F435 最高主频）
 *        AHB=288MHz / APB1=144MHz / APB2=144MHz（APB1/2 上限均为 144MHz）
 * 参考: 雅特力 AT32F435_437_Firmware_Library V2.2.x
 *        project/at_start_f435/templates/src/at32f435_437_clock.c
 *        （system_clock_config 的 288MHz 权威序列，本文件逐寄存器展开）
 *
 * ★ 与 GD32F407 的关键差异（移植时最容易踩的坑）：
 *   1) 时钟外设叫 CRM（GD 叫 RCU），寄存器是【结构体+位域】风格：
 *      CRM->ctrl / ->cfg / ->pllcfg / ->misc2，位域名 *_bit.xxx；
 *   2) 288MHz 必须先把 LDO 输出切到 1.3V（PWC->ldoov_bit.ldoovsel=1），
 *      且要求 VDD>3.0V、工作温度 -40~85℃（GD 168M 是开高驱动 HDEN/HDS）；
 *   3) Flash 这边不是直接写"等待周期 WS"，而是写 Flash 时钟分频
 *      FLASH->divr_bit.fdiv = FLASH_CLOCK_DIV_3（288M/3=96MHz，等效 9 WS）；
 *   4) 切 SCLK 到 PLL 前后要开/关 auto step mode（misc2.auto_step_en=0x03/0x00），
 *      硬件按"自动跨步"序列完成切换，不能像 GD 那样裸写 SCS 死等；
 *   5) PLL 公式 sclk = hext * pll_ns / (pll_ms * pll_fr)：
 *      8M × 144 / (1 × 4) = 288M（pllfr 字段填 2 = CRM_PLL_FR_4）。
 *
 * 注意：AT 官方 SystemInit()（Core/System/system_at32f435_437.c）进 main 前
 *       只把 CRM 复位到 HICK 默认状态并设置 VTOR，【不配置 PLL】，
 *       所以本函数是必须执行的正式时钟配置，不是 GD 工程那种"同参数重配"。
 */
#include "bsp.h"
#include "at32f435_437.h"

void bsp_clock_init(void)
{
    /* 第 1 步：复位 CRM 到默认状态（序列同官方 crm_reset()）
       先让 HICK 稳定并切过去，之后才能安全关 PLL/HEXT 重配 */
    CRM->ctrl_bit.hicken = TRUE;
    while (CRM->ctrl_bit.hickstbl != SET) { }
    CRM->cfg_bit.sclksel = CRM_SCLK_HICK;
    while (CRM->cfg_bit.sclksts != CRM_SCLK_HICK) { }

    CRM->ctrl &= ~(0x010D0000U);      /* 清 hexten/hextbyps/cfden/pllen */
    CRM->cfg = 0U;                    /* 复位 sclk 选择与全部总线分频 */
    CRM->pllcfg = 0x00033002U;        /* 复位 pllms/pllns/pllfr/pllrcs */

    /* 复位 misc1（hickdiv/clkout 等），官方序列里每次写后来一条 ISB */
    CRM->misc1_bit.hick_to_sclk = 1U;
    __ISB();
    CRM->misc1_bit.hickdiv = 0U;
    __ISB();
    CRM->misc1 = 0U;
    __ISB();

    CRM->clkint = 0x009F0000U;        /* 关全部时钟中断并清挂起标志 */

    /* 第 2 步：开 PWC 外设时钟（APB1EN bit28），读一次等总线同步 */
    CRM->apb1en_bit.pwcen = TRUE;
    (void)CRM->apb1en;

    /* 第 3 步：LDO 输出切 1.3V —— 288MHz 的硬性前提
       （1.2V 档最高只能跑 216MHz；1.3V 要求 VDD>3.0V、-40~85℃） */
    PWC->ldoov_bit.ldoovsel = PWC_LDO_OUTPUT_1V3;

    /* 第 4 步：Flash 时钟分频 ÷3
       必须在提高主频【之前】设置；288M/3=96MHz，对应 9 个等待周期 */
    FLASH->divr_bit.fdiv = FLASH_CLOCK_DIV_3;

    /* 第 5 步：开 HEXT 外部 8MHz 晶振，等稳定（ctrl bit16/状态 bit17） */
    CRM->ctrl_bit.hexten = TRUE;
    while (CRM->ctrl_bit.hextstbl != SET) { }

    /* 第 6 步：配 PLL：HEXT ×144 ÷(1×4) = 288MHz
       pllrcs=HEXT(1)  pllns=144  pllms=1  pllfr=CRM_PLL_FR_4(字段值 2) */
    CRM->pllcfg_bit.pllrcs = CRM_PLL_SOURCE_HEXT;
    CRM->pllcfg_bit.pllns  = 144U;
    CRM->pllcfg_bit.pllms  = 1U;
    CRM->pllcfg_bit.pllfr  = CRM_PLL_FR_4;

    /* 第 7 步：开 PLL，等锁定（ctrl bit24/状态 bit25） */
    CRM->ctrl_bit.pllen = TRUE;
    while (CRM->ctrl_bit.pllstbl != SET) { }

    /* 第 8 步：总线分频
       AHB  /1 -> HCLK  288MHz
       APB2 /2 -> PCLK2 144MHz（上限 144MHz）
       APB1 /2 -> PCLK1 144MHz（上限 144MHz） */
    CRM->cfg_bit.ahbdiv  = CRM_AHB_DIV_1;
    CRM->cfg_bit.apb2div = CRM_APB2_DIV_2;
    CRM->cfg_bit.apb1div = CRM_APB1_DIV_2;

    /* 第 9 步：auto step mode 下把系统时钟切到 PLL
       自动跨步模式让硬件按 HICK->HEXT->PLL 安全序列自动切换，
       切完确认 sclksts=2(PLL) 后再关掉该模式（序列同官方模板） */
    CRM->misc2_bit.auto_step_en = CRM_AUTO_STEP_MODE_ENABLE;
    CRM->cfg_bit.sclksel = CRM_SCLK_PLL;
    DUMMY_NOP();
    while (CRM->cfg_bit.sclksts != CRM_SCLK_PLL) { }
    CRM->misc2_bit.auto_step_en = CRM_AUTO_STEP_MODE_DISABLE;

    /* 第 10 步：刷新 system_core_clock 全局变量
       system_at32f435_437.c 维护（全小写，AT 风格，非 GD/ST 的 SystemCoreClock），
       bsp_delay 用它换算 SysTick/DWT 周期数；不调则延时时间全错 */
    system_core_clock_update();
}

/* ---- SPL CRM 函数最小实现 -------------------------------------------------
   本工程是纯寄存器工程，不链接雅特力官方 SPL 库（Drivers/AT32F4xx_StdPeriph/
   下只有 Include/，没有 Src/）。但 CMSIS 系统文件 system_at32f435_437.c 的
   system_core_clock_update() 依赖 crm_sysclk_switch_status_get()，故在此补
   最小实现。CRM 的 cfg 寄存器 cfg_bit.sclksts 位域在 [3:2]，硬件编码正好
   对应 crm_sclk_type 枚举（HICK=0 / HEXT=1 / PLL=2），直接强转即可。 */
crm_sclk_type crm_sysclk_switch_status_get(void)
{
    return (crm_sclk_type)(CRM->cfg_bit.sclksts);
}

/* ========================================================================== */
/* 工具链链接胶水                                                               */
/* ========================================================================== */

/* 当前 newlib（arm-none-eabi-gcc 15 自带）的 __libc_init_array() 仍会调用
   _init()，老式 crti.o/crtn.o 才提供它；裸机工程用了 -nostartfiles 没有
   crti，这里给空桩。C++ 全局对象构造实际走 .init_array 段（启动文件已遍历
   调用，ld 脚本里也有 .init_array 段），不经过 _init，空桩无副作用。
   _fini 同理（程序退出路径，裸机不会走到，给桩只为防链接报错）。 */
void _init(void)
{
}

void _fini(void)
{
}
