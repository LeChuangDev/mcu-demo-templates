/**
 * @file  zigbee_uart_example.h
 * @brief STM32F4 + Zigbee 透传模块集成示例（USART2，PA2/PA3，AF7）。
 *
 * 默认帧格式与 zb_config_default() 一致：
 *   0xFA | LEN | PAYLOAD | SUM8 | 0xFB
 * 实际模块（DL-LN3x、DRF、CC2530 透传等）的帧头/长度/校验请按手册修改
 * zigbee_example_init() 中的 zb_config_t；若模块完全透传，可改为 RAW 模式。
 */
#ifndef ZIGBEE_UART_EXAMPLE_H
#define ZIGBEE_UART_EXAMPLE_H

#include "zigbee.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化链路与 USART2。
 * @param baudrate 模块波特率（常见 9600 / 115200）。
 * @return true 成功。
 */
bool zigbee_example_init(uint32_t baudrate);

/** @brief 按当前协议组帧发送。 */
zb_status_t zigbee_example_send(const uint8_t *payload, size_t len);

/**
 * @brief 取出最近一帧。
 * @param out 输出缓冲区
 * @param capacity 缓冲区容量
 * @param out_len 实际负载长度
 * @return true 取到一帧；false 暂无新帧或容量不足（容量不足时帧保留）。
 */
bool zigbee_example_take_frame(uint8_t *out, size_t capacity,
                               size_t *out_len);

/** @brief 链路错误计数。 */
uint32_t zigbee_example_error_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ZIGBEE_UART_EXAMPLE_H */
