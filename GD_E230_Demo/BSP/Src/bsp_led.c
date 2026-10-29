/**
 * @file  bsp_led.c —— 板载 LED（立创 GD32E230C8T6 开发板：PC13，高电平点亮）
 *
 * 换板子时只改下面四个宏，App 层不需要动：
 *   LED_GPIO / LED_PIN / LED_RCC_EN / LED_ACTIVE_LEVEL
 *
 * ★ Cortex-M23 没有位带(bit-band) ★
 *   M23 属于 ARMv8-M Baseline，不提供 M3/M4 的位带别名区，
 *   所以这里改用 GD32 自带的 GPIO_BOP / GPIO_BC / GPIO_TG 寄存器：
 *   写 1 生效、写 0 不影响其它位，同样是硬件原子操作。
 */
#include "bsp.h"
#include "gd32e23x.h"

#define LED_GPIO          GPIOC
#define LED_PIN           13u
#define LED_RCC_EN        RCU_AHBEN_PCEN
#define LED_ACTIVE_LEVEL  1u        /* 1 = 高电平点亮；低电平点亮的板改成 0 */

#define LED_MASK          (1u << LED_PIN)

void bsp_led_init(void)
{
    /* 开 GPIO 时钟（E23x 的 GPIO 挂在 AHB 上，使能位在 RCU_AHBEN）；
       读回一次寄存器，等外设总线时钟稳定后再访问 GPIO */
    RCU_AHBEN |= LED_RCC_EN;
    (void)RCU_AHBEN;

    /* GPIO_CTL：每引脚 2 位，01 = 通用输出。
       其余属性保持复位值即可（推挽、2MHz、无上下拉） */
    GPIO_CTL(LED_GPIO) = (GPIO_CTL(LED_GPIO) & ~GPIO_MODE_MASK(LED_PIN))
                       | GPIO_MODE_SET(LED_PIN, GPIO_MODE_OUTPUT);
    GPIO_OMODE(LED_GPIO) &= ~(1u << LED_PIN);                 /* 0 = 推挽 */
    GPIO_OSPD(LED_GPIO)  = (GPIO_OSPD(LED_GPIO) & ~GPIO_OSPEED_MASK(LED_PIN))
                         | GPIO_OSPEED_SET(LED_PIN, GPIO_OSPEED_2MHZ);
    GPIO_PUD(LED_GPIO)   &= ~GPIO_PUPD_MASK(LED_PIN);         /* 00 = 浮空 */

    bsp_led_off();
}

void bsp_led_on(void)
{
#if LED_ACTIVE_LEVEL
    GPIO_BOP(LED_GPIO) = LED_MASK;      /* 置位寄存器：写 1 置高 */
#else
    GPIO_BC(LED_GPIO) = LED_MASK;       /* 清位寄存器：写 1 置低 */
#endif
}

void bsp_led_off(void)
{
#if LED_ACTIVE_LEVEL
    GPIO_BC(LED_GPIO) = LED_MASK;
#else
    GPIO_BOP(LED_GPIO) = LED_MASK;
#endif
}

void bsp_led_toggle(void)
{
    GPIO_TG(LED_GPIO) = LED_MASK;       /* 翻转寄存器：写 1 取反 */
}
