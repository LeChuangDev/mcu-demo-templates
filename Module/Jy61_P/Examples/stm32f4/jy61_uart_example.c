/**
 * @file  jy61_uart_example.c
 * @brief STM32F4 + JY61P 集成示例。
 *
 * 接线（与旧版 Jy61_P.c 一致）：
 *   PB10 -> USART3_TX（接传感器 RX，可选）
 *   PB11 <- USART3_RX
 *
 * 说明：
 *   - 中断只做“取字节 + 喂给解析器”，不做浮点运算、不刷屏；
 *   - 主循环通过 jy61_example_read() 取快照；
 *   - 若工程里已有 USART3_IRQHandler，请把本文件中的处理逻辑合并进去，
 *     避免重复定义。
 */
#include "jy61_uart_example.h"

#include "stm32f4xx.h"

#ifndef JY61_EXAMPLE_PCLK1_HZ
#define JY61_EXAMPLE_PCLK1_HZ 42000000u
#endif

static jy61_t s_jy61;
static uint32_t s_critical_state;

static void jy61_example_critical_enter(void *user)
{
    (void)user;
    s_critical_state = __get_PRIMASK();
    __disable_irq();
}

static void jy61_example_critical_exit(void *user)
{
    (void)user;
    __set_PRIMASK(s_critical_state);
}

static void jy61_example_uart_init(uint32_t baudrate)
{
    /* PB10/PB11 复用为 USART3_TX/RX（AF7）。 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    __DSB();

    GPIOB->MODER &= ~((3u << (10u * 2u)) | (3u << (11u * 2u)));
    GPIOB->MODER |= (2u << (10u * 2u)) | (2u << (11u * 2u));
    GPIOB->OSPEEDR |= (2u << (10u * 2u)) | (2u << (11u * 2u));

    /* RX 上拉，避免悬空产生噪声。 */
    GPIOB->PUPDR &= ~(3u << (11u * 2u));
    GPIOB->PUPDR |= (1u << (11u * 2u));

    GPIOB->AFR[1] &= ~((0xFu << 8) | (0xFu << 12));
    GPIOB->AFR[1] |= (7u << 8) | (7u << 12);

    USART3->CR1 = 0u;
    USART3->CR2 = 0u;
    USART3->CR3 = 0u;
    /* OVER8 = 0，16 倍过采样。 */
    USART3->BRR = (JY61_EXAMPLE_PCLK1_HZ + baudrate / 2u) / baudrate;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE |
                  USART_CR1_UE;

    NVIC_SetPriority(USART3_IRQn, 6u);
    NVIC_EnableIRQ(USART3_IRQn);
}

bool jy61_example_init(uint32_t baudrate)
{
    jy61_config_t config;

    if (baudrate == 0u) {
        return false;
    }

    jy61_config_default(&config);
    config.critical.enter = jy61_example_critical_enter;
    config.critical.exit = jy61_example_critical_exit;
    config.critical.user = NULL;

    if (jy61_init(&s_jy61, &config) != JY61_OK) {
        return false;
    }

    jy61_example_uart_init(baudrate);
    return true;
}

jy61_status_t jy61_example_read(jy61_data_t *out)
{
    return jy61_get_data(&s_jy61, out);
}

void jy61_example_stats(jy61_stats_t *out)
{
    jy61_get_stats(&s_jy61, out);
}

/** UART 中断：把每个字节交给流式解析器。 */
void USART3_IRQHandler(void)
{
    uint32_t status = USART3->SR;

    if ((status & (USART_SR_RXNE | USART_SR_ORE)) != 0u) {
        /* 读 SR 后再读 DR 可同时清除 RXNE 与 ORE。 */
        uint8_t byte = (uint8_t)(USART3->DR & 0xFFu);
        (void)jy61_feed_byte(&s_jy61, byte);
    }
}
