/**
 * @file  zigbee_uart_example.c
 * @brief STM32F4 + Zigbee 串口模块集成示例。
 *
 * 接线：PA2 -> USART2_TX，PA3 <- USART2_RX。
 *
 * 中断里只做解析和拷贝；帧处理在 zigbee_example_take_frame() 的主循环
 * 上下文中完成。若工程已有 USART2_IRQHandler，请合并这里的两个分支。
 */
#include "zigbee_uart_example.h"

#include <string.h>

#include "stm32f4xx.h"

#ifndef ZIGBEE_EXAMPLE_PCLK1_HZ
#define ZIGBEE_EXAMPLE_PCLK1_HZ 42000000u
#endif

static zb_link_t s_link;
static uint8_t s_frame[ZB_MAX_PAYLOAD];
static volatile size_t s_frame_len;
static volatile bool s_frame_ready;
static volatile uint32_t s_error_count;

static void zigbee_example_uart_init(uint32_t baudrate)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    __DSB();

    GPIOA->MODER &= ~((3u << (2u * 2u)) | (3u << (3u * 2u)));
    GPIOA->MODER |= (2u << (2u * 2u)) | (2u << (3u * 2u));
    GPIOA->OSPEEDR |= (2u << (2u * 2u)) | (2u << (3u * 2u));

    /* RX 上拉，模块未上电时避免浮空噪声。 */
    GPIOA->PUPDR &= ~(3u << (3u * 2u));
    GPIOA->PUPDR |= (1u << (3u * 2u));

    GPIOA->AFR[0] &= ~((0xFu << 8) | (0xFu << 12));
    GPIOA->AFR[0] |= (7u << 8) | (7u << 12);

    USART2->CR1 = 0u;
    USART2->CR2 = 0u;
    USART2->CR3 = 0u;
    USART2->BRR = (ZIGBEE_EXAMPLE_PCLK1_HZ + baudrate / 2u) / baudrate;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE |
                  USART_CR1_UE;

    NVIC_SetPriority(USART2_IRQn, 6u);
    NVIC_EnableIRQ(USART2_IRQn);
}

static bool zigbee_example_uart_write(void *user, const uint8_t *data,
                                      size_t len)
{
    size_t i;

    (void)user;
    for (i = 0u; i < len; ++i) {
        uint32_t guard = 1000000u;

        while ((USART2->SR & USART_SR_TXE) == 0u) {
            if (--guard == 0u) {
                return false;
            }
        }
        USART2->DR = data[i];
    }
    return true;
}

static void zigbee_example_on_frame(void *user, const uint8_t *payload,
                                    size_t len)
{
    (void)user;

    if (len > sizeof(s_frame)) {
        len = sizeof(s_frame);
    }
    if (len != 0u) {
        memcpy(s_frame, payload, len);
    }
    s_frame_len = len;
    s_frame_ready = true;
}

static void zigbee_example_on_error(void *user, zb_error_t error)
{
    (void)user;
    (void)error;
    s_error_count++;
}

bool zigbee_example_init(uint32_t baudrate)
{
    zb_config_t config;
    zb_port_t port;

    if (baudrate == 0u) {
        return false;
    }

    zb_config_default(&config);
    /* 按模块手册修改以下字段：
     *   config.sop[] / sop_len / eop[] / eop_len
     *   config.length_bytes / checksum
     *   config.escape_enabled / escape_byte / escape_xor
     * 若模块是纯透传，改为 config.mode = ZB_MODE_RAW。
     */
    config.on_frame = zigbee_example_on_frame;
    config.on_error = zigbee_example_on_error;
    config.user = NULL;

    port.write = zigbee_example_uart_write;
    port.user = NULL;

    s_frame_ready = false;
    s_frame_len = 0u;
    s_error_count = 0u;

    if (zb_init(&s_link, &config, &port) != ZB_OK) {
        return false;
    }

    zigbee_example_uart_init(baudrate);
    return true;
}

zb_status_t zigbee_example_send(const uint8_t *payload, size_t len)
{
    return zb_send(&s_link, payload, len);
}

bool zigbee_example_take_frame(uint8_t *out, size_t capacity,
                               size_t *out_len)
{
    uint32_t primask;
    size_t len;

    if (out == NULL || out_len == NULL) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if (!s_frame_ready) {
        __set_PRIMASK(primask);
        return false;
    }

    len = s_frame_len;
    if (len > capacity) {
        /* 容量不足时不消费，避免静默丢帧。 */
        __set_PRIMASK(primask);
        return false;
    }

    if (len != 0u) {
        memcpy(out, s_frame, len);
    }
    s_frame_ready = false;
    *out_len = len;

    __set_PRIMASK(primask);
    return true;
}

uint32_t zigbee_example_error_count(void)
{
    return s_error_count;
}

/** UART 中断：向链路实例喂字节。 */
void USART2_IRQHandler(void)
{
    uint32_t status = USART2->SR;

    if ((status & (USART_SR_RXNE | USART_SR_ORE)) != 0u) {
        uint8_t byte = (uint8_t)(USART2->DR & 0xFFu);
        (void)zb_feed_byte(&s_link, byte);
    }
}
