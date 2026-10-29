/**
 * @file  bsp_led.c —— 板载 LED（模板默认 PB2 推挽输出）
 *
 * 换板子时只改下面四个宏，App 层不需要动：
 *   LED_GPIO / LED_PIN / LED_RCC_EN / LED_ACTIVE_LEVEL
 *
 * 注意：PB2 在 STM32F407 上同时是 BOOT1，若目标板的 PB2 接了启动模式
 * 上下拉，请换成板上实际 LED 所用的引脚。
 */
#include "bsp.h"
#include "bsp_bitband.h"
#include "stm32f4xx.h"

#define LED_GPIO          GPIOB
#define LED_PIN           2u
#define LED_RCC_EN        RCC_AHB1ENR_GPIOBEN
#define LED_ACTIVE_LEVEL  1u        /* 1 = 高电平点亮；低电平点亮的板改成 0 */

void bsp_led_init(void)
{
    /* 开 GPIO 时钟；读回一次寄存器，等外设总线时钟稳定后再访问 GPIO */
    RCC->AHB1ENR |= LED_RCC_EN;
    (void)RCC->AHB1ENR;

    /* MODER：先清 2 位，再写 01 = 通用推挽输出。
       OTYPER/OSPEEDR/PUPDR 保持复位值即可（推挽、低速、无上下拉） */
    LED_GPIO->MODER &= ~(3u << (LED_PIN * 2u));
    LED_GPIO->MODER |=  (1u << (LED_PIN * 2u));

    bsp_led_off();
}

void bsp_led_on(void)
{
    /* 位带写：硬件原子地只改这一位，不影响同端口的其它引脚 */
    BB_REG(LED_GPIO->ODR, LED_PIN) = LED_ACTIVE_LEVEL;
}

void bsp_led_off(void)
{
    BB_REG(LED_GPIO->ODR, LED_PIN) = !LED_ACTIVE_LEVEL;
}

void bsp_led_toggle(void)
{
    /* 读当前位 -> 取反 -> 原子写回 */
    BB_REG(LED_GPIO->ODR, LED_PIN) = !BB_REG(LED_GPIO->ODR, LED_PIN);
}
