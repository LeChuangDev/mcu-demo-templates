/**
 * @file  bsp_clock.c —— 纯寄存器配置系统时钟（GD32E230 版）
 * 目标: HXTAL 8MHz -> PLL x9 -> SYSCLK 72MHz
 *        AHB=72MHz / APB1=72MHz / APB2=72MHz（官方 72MHz 配置）
 * 参考: GD32E23x 用户手册  RCU / FMC 章节
 *
 * ★ 与 GD32F4 的寄存器命名差异（移植时最容易踩的坑）：
 *      F4                     E23x
 *      RCU_CTL              -> RCU_CTL0
 *      RCU_CFG0             -> RCU_CFG0（PLL 参数也在这里，没有单独的 RCU_PLL）
 *      PLLPSC/PLLN/PLLP/Q   -> PLLSEL + PLLMF（只有倍频，没有 M/N/P 三段）
 *      RCU_AHB1EN           -> RCU_AHBEN
 *      PMU 高驱动/调压       -> 不需要（72MHz 不用 HDEN/LDOVS）
 *
 * ★ 与 SystemInit() 的关系 ★
 *   startup 里的 SystemInit()（system_gd32e23x.c）进场前已经把时钟配到 72MHz。
 *   本函数仍然完整重配一遍：先切回 IRC8M，再关 PLL，最后按 BSP 参数重配。
 *   所有等待都带超时，HXTAL/PLL 异常时不会永久卡死；
 *   返回值非 0 时调用者可继续运行（SystemCoreClock 已按实际时钟更新）。
 */
#include "bsp.h"
#include "gd32e23x.h"

/* 等待超时：即使还在 8MHz IRC8M 上，也足够覆盖晶振起振（典型 1~2ms） */
#define CLOCK_WAIT_TIMEOUT  500000u

/* 等 reg 的 mask 位变成 value；超时返回 -1。
   GD 的寄存器宏展开就是 volatile 解引用，&RCU_CTL0 可直接取到 volatile 指针。 */
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
    /* 第 1 步：确保 IRC8M(8MHz 内部 RC) 已开启并稳定，然后切到 IRC8M
       后面要关 PLL 改参数，而 PLL 在运行中时参数位不可写，
       所以先把系统时钟临时退到 IRC8M */
    RCU_CTL0 |= RCU_CTL0_IRC8MEN;
    if (clock_wait(&RCU_CTL0, RCU_CTL0_IRC8MSTB, RCU_CTL0_IRC8MSTB) != 0) {
        SystemCoreClockUpdate();
        return -1;
    }
    RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS) | RCU_CKSYSSRC_IRC8M;
    if (clock_wait(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_IRC8M) != 0) {
        SystemCoreClockUpdate();
        return -2;
    }

    /* 第 2 步：关 PLL，等 PLLSTB 回落到 0（PLL 完全停止后参数才可改写） */
    RCU_CTL0 &= ~RCU_CTL0_PLLEN;
    if (clock_wait(&RCU_CTL0, RCU_CTL0_PLLSTB, 0u) != 0) {
        SystemCoreClockUpdate();
        return -3;
    }

    /* 第 3 步：Flash 等待周期
       72MHz/3.3V 时按手册需要 2 个等待周期（WS_WSCNT_2）。
       必须在提升主频【之前】设置好，所以放在配 PLL 之前。
       PFEN 等其它位保持原值（只改 WSCNT 字段） */
    FMC_WS = (FMC_WS & ~FMC_WS_WSCNT) | WS_WSCNT_2;

    /* 第 4 步：开 HXTAL 外部晶振（立创 GD32E230C8T6 板载 8MHz），等稳定 */
    RCU_CTL0 |= RCU_CTL0_HXTALEN;
    if (clock_wait(&RCU_CTL0, RCU_CTL0_HXTALSTB, RCU_CTL0_HXTALSTB) != 0) {
        SystemCoreClockUpdate();            /* 保持 IRC8M，让延时按实际频率计算 */
        return -4;
    }

    /* 第 5 步：总线分频（此时系统时钟还是 IRC8M 8MHz，怎么分都安全）
       AHB  = SYSCLK / 1 -> HCLK  72MHz
       APB2 = AHB    / 1 -> PCLK2 72MHz
       APB1 = AHB    / 1 -> PCLK1 72MHz
       只改这三个分频字段，其余位保持不动 */
    RCU_CFG0 &= ~(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1
              | RCU_APB2_CKAHB_DIV1
              | RCU_APB1_CKAHB_DIV1;

    /* 第 6 步：PLL 参数
       GD32E23x 的 PLL 比 F4 简单：
         PLLSEL = 1        -> PLL 输入选 HXTAL（不是 IRC8M/2）
         PLLPREDV = 0      -> HXTAL 不分频（8MHz 直接进 PLL）
         PLLMF = 9         -> 8MHz x 9 = 72MHz
         PLLDV = 0         -> PLL 输出不再除 2
       PLL 输入频率手册要求 1~24MHz，8MHz 满足 */
    RCU_CFG0 &= ~(RCU_CFG0_PLLSEL | RCU_CFG0_PLLPREDV |
                  RCU_CFG0_PLLMF | RCU_CFG0_PLLDV);
    RCU_CFG0 |= (RCU_PLLSRC_HXTAL | RCU_PLL_MUL9);

    /* 第 7 步：开 PLL，等锁定（带超时） */
    RCU_CTL0 |= RCU_CTL0_PLLEN;
    if (clock_wait(&RCU_CTL0, RCU_CTL0_PLLSTB, RCU_CTL0_PLLSTB) != 0) {
        SystemCoreClockUpdate();
        return -5;
    }

    /* 第 8 步：把系统时钟源切到 PLL，等 SCSS 状态确认（带超时）
       SCS[1:0] 是"选择"位，SCSS[3:2] 是"当前实际使用源"的状态位 */
    RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS) | RCU_CKSYSSRC_PLL;
    if (clock_wait(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_PLL) != 0) {
        SystemCoreClockUpdate();
        return -6;
    }

    /* 第 9 步：更新 SystemCoreClock 全局变量
       system_gd32e23x.c 维护，bsp_delay 用它换算周期数；
       不调的话 TIMER5 微秒延时会按旧频率设置分频，时间全错 */
    SystemCoreClockUpdate();
    return 0;
}

/* ========================================================================== */
/* 系统启动 / 工具链链接胶水                                                    */
/* ========================================================================== */

/* SystemInit()（Core/System/system_gd32e23x.c）末尾要调用 nvic_vector_table_set()，
   GD 官方实现在 gd32e23x_misc.c 里；本工程只引用官方寄存器【头】、不编译官方 .c，
   所以在 BSP 层补一个寄存器实现——这个函数本质就是设置 SCB->VTOR（向量表基址）。
   声明在 gd32e23x_misc.h，签名必须与官方一致，链接器靠它解析 SystemInit 的引用 */
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
