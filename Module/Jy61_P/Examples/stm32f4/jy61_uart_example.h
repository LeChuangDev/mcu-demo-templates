/**
 * @file  jy61_uart_example.h
 * @brief STM32F4 + JY61P 集成示例（USART3，PB10/PB11，AF7）。
 *
 * 本文件只演示如何把平台无关的 jy61 驱动接到 STM32F4 的 UART 中断上。
 * 移植到自己的工程时，把 .c 中的 GPIO/USART 初始化替换成项目的 BSP 即可。
 */
#ifndef JY61_UART_EXAMPLE_H
#define JY61_UART_EXAMPLE_H

#include "jy61.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 JY61P 实例与 USART3。
 * @param baudrate 传感器波特率（JY61P 出厂通常为 9600）。
 * @return true 成功；参数非法或驱动初始化失败时返回 false。
 *
 * @note 本例使用 PB10=TX、PB11=RX、USART3、AF7，PCLK1 默认按 42 MHz 计算。
 */
bool jy61_example_init(uint32_t baudrate);

/**
 * @brief 读取最新姿态快照（主循环调用）。
 * @return JY61_OK / JY61_ERR_NO_DATA / JY61_ERR_PARAM / JY61_ERR_BUSY。
 *
 * @note 示例配置了关中断临界区，因此在主循环中读取是安全的。
 */
jy61_status_t jy61_example_read(jy61_data_t *out);

/** @brief 获取链路统计。 */
void jy61_example_stats(jy61_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* JY61_UART_EXAMPLE_H */
