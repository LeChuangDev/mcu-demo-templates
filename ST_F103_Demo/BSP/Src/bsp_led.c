/**
 * @file  bsp_led.c —— 板载 LED（模板默认 PC13 推挽输出，低电平点亮）
 *
 * 换板子时只改下面四个宏，App 层不需要动：
 *   LED_GPIO / LED_PIN / LED_RCC_EN / LED_ACTIVE_LEVEL
 *
 * 默认值对应最常见的 STM32F103C8T6 "Blue Pill" 最小系统板：
 *   板载 LED 接 PC13，低电平点亮（LED_ON_LEVEL = 0）。
 *   若你的板子 LED 在 PA5/PB2/PC6 等位置，改成对应端口和引脚即可，
 *   例如 PA5 高电平点亮：GPIOA / 5 / RCC_APB2ENR_IOPAEN / 1。
 *
 * ★ F1 的 GPIO 配置与 G4/F4 不同 ★
 *   F1 没有 MODER/OTYPER/OSPEEDR/PUPDR 四个寄存器，而是用
 *   CRL（Pin0~7）和 CRH（Pin8~15），每个引脚占 4 位：
 *     低 2 位 MODE = 00 输入 / 01 输出 10MHz / 10 输出 2MHz / 11 输出 50MHz
 *     高 2 位 CNF  = 00 通用推挽 / 01 通用开漏 / 10 复用推挽 / 11 复用开漏
 *   所以 2MHz 推挽输出 = 0b0010 = 0x2。
 *
 * ★ 关于位带(bit-band) ★
 *   F1 的 GPIO 位于 0x4001_0800 起，正好落在 Cortex-M3 外设位带区
 *   (0x4000_0000~0x400F_FFFF) 内，理论上可以用位带；但 BSRR 写 1 生效、
 *   写 0 无影响，本身就是原子操作且只影响目标引脚，所以这里继续用 BSRR。
 */
#include "bsp.h"
#include "stm32f10x.h"

#define LED_GPIO          GPIOC
#define LED_PIN           13u
#define LED_RCC_EN        RCC_APB2ENR_IOPCEN   /* F1 的 GPIO 挂在 APB2 上 */
#define LED_ACTIVE_LEVEL  0u        /* 0 = 低电平点亮；高电平点亮的板改成 1 */

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
    RCC->APB2ENR |= LED_RCC_EN;
    (void)RCC->APB2ENR;

    /* CRL/CRH：先清 4 位，再写 0x2 = 输出模式 2MHz + 通用推挽 */
    volatile uint32_t *cr = (LED_PIN < 8u) ? &LED_GPIO->CRL : &LED_GPIO->CRH;
    uint32_t shift = (LED_PIN % 8u) * 4u;
    *cr &= ~(0xFu << shift);
    *cr |=  (0x2u << shift);

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
    /* 先把"引脚电平"换算成"逻辑亮/灭"，再取反。
       ★ 不能直接对 ODR 取反后当逻辑值传给 led_write：
         那样在【低电平点亮】的板子上会变成空操作，灯永远不变。 */
    uint32_t pin_high  = (LED_GPIO->ODR & (1u << LED_PIN)) != 0u ? 1u : 0u;
    uint32_t led_on    = (pin_high == (LED_ACTIVE_LEVEL != 0u)) ? 1u : 0u;
    led_write(led_on ^ 1u);
}
