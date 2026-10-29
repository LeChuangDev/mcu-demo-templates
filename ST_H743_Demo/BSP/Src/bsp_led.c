/**
 * @file  bsp_led.c —— 板载 LED（模板默认 PB0 推挽输出）
 *
 * 换板子时只改下面四个宏，App 层不需要动：
 *   LED_GPIO / LED_PIN / LED_RCC_EN / LED_ACTIVE_LEVEL
 *
 * 默认 PB0 对应 NUCLEO-H743ZI 的 LD1（绿色）。若使用其它 H743 核心板，
 * 按原理图改成实际引脚即可，例如 WeAct H743 核心板可自行确认 LED 网络；
 * 只改这四个宏，不要动下面的初始化逻辑。
 *
 * ★ 为什么不用位带(bit-band) ★
 *   H7 的 GPIO 寄存器位于 0x5802_0000 之后的 AHB4 外设区，而 Cortex-M7
 *   的外设位带区只覆盖 0x4000_0000~0x400F_FFFF —— GPIO 完全不在其中，
 *   位带别名地址写下去是无效的。因此这里使用 GPIOx->BSRR：
 *   写 1 生效、写 0 无影响，天然是原子操作，不会破坏同端口其它引脚。
 */
#include "bsp.h"
#include "stm32h7xx.h"

#define LED_GPIO          GPIOB
#define LED_PIN           0u
#define LED_RCC_EN        RCC_AHB4ENR_GPIOBEN   /* H7 的 GPIO 挂在 AHB4 上 */
#define LED_ACTIVE_LEVEL  1u        /* 1 = 高电平点亮；低电平点亮的板改成 0 */

static inline void led_write(uint32_t level)
{
    if ((level != 0u) == (LED_ACTIVE_LEVEL != 0u)) {
        LED_GPIO->BSRR = (1u << LED_PIN);          /* 置 1 */
    } else {
        LED_GPIO->BSRR = (1u << (LED_PIN + 16u));  /* 清 0 */
    }
}

void bsp_led_init(void)
{
    /* 开 GPIO 时钟；读回一次寄存器，等外设总线时钟稳定后再访问 GPIO */
    RCC->AHB4ENR |= LED_RCC_EN;
    (void)RCC->AHB4ENR;

    /* MODER：先清 2 位，再写 01 = 通用推挽输出。
       OTYPER/OSPEEDR/PUPDR 保持复位值即可（推挽、低速、无上下拉） */
    LED_GPIO->MODER &= ~(3u << (LED_PIN * 2u));
    LED_GPIO->MODER |=  (1u << (LED_PIN * 2u));

    bsp_led_off();
}

void bsp_led_on(void)
{
    led_write(1u);
}

void bsp_led_off(void)
{
    led_write(0u);
}

void bsp_led_toggle(void)
{
    /* 读当前输出位 -> 用 BSRR 原子地写反，不碰同端口其它位 */
    led_write((LED_GPIO->ODR & (1u << LED_PIN)) != 0u ? 0u : 1u);
}
