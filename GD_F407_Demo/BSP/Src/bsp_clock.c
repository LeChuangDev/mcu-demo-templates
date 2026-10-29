/**
 * @file  bsp_clock.c —— 纯寄存器配置系统时钟（GD32F407 版）
 * 目标: HXTAL 25MHz -> PLL -> SYSCLK 168MHz
 *        AHB=168MHz / APB1=42MHz / APB2=84MHz（F407 的三大极限值）
 * 参考: GD32F4xx 用户手册  RCU/PMU/FMC 章节
 *
 * ★ 与 STM32F4 的寄存器命名差异（移植时最容易踩的坑）：
 *      STM32            GD32
 *      RCC->CR/CFGR  -> RCU_CTL / RCU_CFG0 / RCU_PLL  (裸宏，非结构体)
 *      RCC->APB1ENR  -> RCU_APB1EN
 *      PWR->CR/CSR   -> PMU_CTL / PMU_CS
 *      FLASH->ACR    -> FMC_WS
 *      HSE/HSERDY    -> HXTAL/HXTALSTB，PLLRDY -> PLLSTB
 *   另外两点不同：
 *     1) 168MHz 时 GD 必须开"高驱动"(HDEN/HDS)，ST 只需要 VOS Scale1；
 *     2) GD 的 FMC 没有 ST 那种 PRFTEN/ICEN/DCEN 位——ART 加速器始终生效，
 *        只能设置等待周期 WSCNT。
 *
 * ★ 与 SystemInit() 的关系 ★
 *   startup 里的 SystemInit()（system_gd32f4xx.c）进场前已经把时钟配到 168MHz。
 *   本函数仍然完整重配一遍，但顺序是安全的：先切到 HXTAL，再关 PLL，最后改参数。
 *   所有等待都带超时，HXTAL/PLL/PMU 异常时不会永久卡死；
 *   返回值非 0 时调用者可继续运行（SystemCoreClock 已按实际时钟更新）。
 */
#include "bsp.h"
#include "gd32f4xx.h"

/* PLL 各字段的移位量（RCU_PLL 位布局与 ST 的 RCC_PLLCFGR 相同）：
   PLLPSC[5:0] / PLLN[14:6] / PLLP[17:16] / PLLSEL bit22 / PLLQ[27:24] */
#define RCU_PLL_PLLPSC_SHIFT   0u    /* 输入分频 M 字段位域: bit[5:0]  */
#define RCU_PLL_PLLN_SHIFT     6u    /* 倍频 N 字段位域:   bit[14:6]  */
#define RCU_PLL_PLLP_SHIFT    16u    /* P 字段位域:        bit[17:16]，00 = ÷2 */
#define RCU_PLL_PLLQ_SHIFT    24u    /* Q 字段位域:        bit[27:24] */

/* 等待超时：即使还在 16MHz IRC16M 上，也足够覆盖晶振起振（典型 1~2ms） */
#define CLOCK_WAIT_TIMEOUT  500000u

/* 等 reg 的 mask 位变成 value；超时返回 -1。
   GD 的寄存器宏展开就是 volatile 解引用，&RCU_CTL 可直接取到 volatile 指针。 */
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
    /* 第 1 步：开 PMU 时钟，LDO 输出切到高档，等就绪
       GD32F407 跑高主频前 PMU_CTL_LDOVS 要写 11b（LDO 高输出档），
       对应 ST 的 VOS Scale1；之后还要开高驱动，见第 8 步 */
    RCU_APB1EN |= RCU_APB1EN_PMUEN;
    (void)RCU_APB1EN;                       /* 读一次，等 APB 总线同步 */
    PMU_CTL |= PMU_CTL_LDOVS;
    if (clock_wait(&PMU_CS, PMU_CS_LDOVSRF, PMU_CS_LDOVSRF) != 0) {
        SystemCoreClockUpdate();
        return -1;
    }

    /* 第 2 步：Flash 等待周期
       168MHz/3.3V 时按手册需要 5 个等待周期（WS_WSCNT_5）。
       必须在提升主频【之前】设置好，所以放在配 PLL 之前。
       GD 没有预取/缓存使能位：ART 指令加速器上电即工作。 */
    FMC_WS = (FMC_WS & ~FMC_WC_WSCNT) | WS_WSCNT_5;

    /* 第 3 步：开 HXTAL 外部晶振，等稳定（带超时） */
    RCU_CTL |= RCU_CTL_HXTALEN;
    if (clock_wait(&RCU_CTL, RCU_CTL_HXTALSTB, RCU_CTL_HXTALSTB) != 0) {
        SystemCoreClockUpdate();            /* 保持原时钟，让延时按实际频率计算 */
        return -2;
    }

    /* 第 4 步：若系统时钟正来自 PLL，先切到 HXTAL
       PLL 使能时 RCU_PLL 的配置位不可写，重配 PLL 前必须先关掉它，
       而关 PLL 前系统时钟又不能还选着 PLL，故先临时切到 HXTAL */
    if ((RCU_CFG0 & RCU_CFG0_SCSS) == RCU_SCSS_PLLP) {
        RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS) | RCU_CKSYSSRC_HXTAL;
        if (clock_wait(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_HXTAL) != 0) {
            SystemCoreClockUpdate();
            return -3;
        }
    }

    /* 第 5 步：关 PLL，等 PLLSTB 回落到 0（带超时；PLL 完全停止后参数才可改写） */
    RCU_CTL &= ~RCU_CTL_PLLEN;
    if (clock_wait(&RCU_CTL, RCU_CTL_PLLSTB, 0u) != 0) {
        SystemCoreClockUpdate();
        return -4;
    }

    /* 第 6 步：总线分频
       AHB  = SYSCLK / 1 -> HCLK  168MHz
       APB2 = AHB    / 2 -> PCLK2  84MHz（APB2 外设最大 84MHz）
       APB1 = AHB    / 4 -> PCLK1  42MHz（APB1 外设最大 42MHz）
       只改这三个分频字段，其余位保持不动 */
    RCU_CFG0 &= ~(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1
              | RCU_APB2_CKAHB_DIV2
              | RCU_APB1_CKAHB_DIV4;

    /* 第 7 步：PLL 参数（三段式：M 分频 -> N 倍频 -> P 分频）
       25MHz ÷ PLLPSC(25) = 1MHz     输入级，要求落进 1~2MHz
       1MHz × PLLN(336)  = 336MHz   中间级 VCO，要求落进 100~432MHz
       336MHz ÷ PLLP(2)  = 168MHz   系统时钟，P 只能取 2/4/6/8
       PLLQ = 7 -> 336/7 = 48MHz    给 USB OTG 预留（不用 USB 也建议按规范填）
       时钟源选 HXTAL（RCU_PLLSRC_HXTAL = bit22） */
    RCU_PLL = (25u   << RCU_PLL_PLLPSC_SHIFT)
            | (336u  << RCU_PLL_PLLN_SHIFT)
            | (0u    << RCU_PLL_PLLP_SHIFT)    /* 字段值 00 = ÷2 */
            |  RCU_PLLSRC_HXTAL
            | (7u    << RCU_PLL_PLLQ_SHIFT);

    /* 第 8 步：开 PLL，等锁定；然后使能高驱动并等待切换完成
       GD32F407 主频超过 ~120MHz 时必须进高驱动模式，顺序固定：
       HDEN=1 -> 等 HDRF -> HDS=1 -> 等 HDSRF */
    RCU_CTL |= RCU_CTL_PLLEN;
    if (clock_wait(&RCU_CTL, RCU_CTL_PLLSTB, RCU_CTL_PLLSTB) != 0) {
        SystemCoreClockUpdate();
        return -5;
    }
    PMU_CTL |= PMU_CTL_HDEN;
    if (clock_wait(&PMU_CS, PMU_CS_HDRF, PMU_CS_HDRF) != 0) {
        SystemCoreClockUpdate();
        return -6;
    }
    PMU_CTL |= PMU_CTL_HDS;
    if (clock_wait(&PMU_CS, PMU_CS_HDSRF, PMU_CS_HDSRF) != 0) {
        SystemCoreClockUpdate();
        return -7;
    }

    /* 第 9 步：把系统时钟源切到 PLLP，等 SCSS 状态确认（带超时）
       SCS[1:0] 是"选择"位，SCSS[3:2] 是"当前实际使用源"的状态位 */
    RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS) | RCU_CKSYSSRC_PLLP;
    if (clock_wait(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_PLLP) != 0) {
        SystemCoreClockUpdate();
        return -8;
    }

    /* 第 10 步：更新 SystemCoreClock 全局变量
       system_gd32f4xx.c 维护，bsp_delay 用它换算周期数；
       不调的话 DWT 微秒延时会按旧频率算，时间全错 */
    SystemCoreClockUpdate();
    return 0;
}

/* ========================================================================== */
/* 系统启动 / 工具链链接胶水                                                    */
/* ========================================================================== */

/* SystemInit()（Core/System/system_gd32f4xx.c）末尾要调用 nvic_vector_table_set()，
   GD 官方实现在 gd32f4xx_misc.c 里；本工程只引用官方寄存器【头】、不编译官方 .c，
   所以在 BSP 层补一个寄存器实现——这个函数本质就是设置 SCB->VTOR（向量表基址）。
   声明在 gd32f4xx_misc.h，签名必须与官方一致，链接器靠它解析 SystemInit 的引用 */
void nvic_vector_table_set(uint32_t nvic_vict_tab, uint32_t offset)
{
    /* 低 7 位保留必须为 0（向量表要求 128 字节以上对齐，offset 同样如此）；
       写完做一次数据同步屏障，保证后续取向量时新基址已生效 */
    SCB->VTOR = nvic_vict_tab | (offset & NVIC_VECTTAB_OFFSET_MASK);
    __DSB();
}

/* 当前 newlib（arm-none-eabi-gcc 15 自带）的 __libc_init_array() 仍会调用 _init()，
   老式 crti.o/crtn.o 才提供它；裸机工程用了 -nostartfiles 没有 crti，这里给空桩。
   注意：C++ 全局对象构造实际走 .init_array 段（由 __libc_init_array 遍历调用，
   ld 脚本里已有 .init_array 段），不经过 _init，空桩不影响构造函数执行。
   _fini 同理（程序退出路径，裸机根本不会走到，给桩只为防链接报错） */
void _init(void)
{
}

void _fini(void)
{
}
