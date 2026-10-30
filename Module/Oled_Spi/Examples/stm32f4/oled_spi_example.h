/**
 * @file  oled_spi_example.h
 * @brief STM32F4 + SSD1306 软件 SPI 集成示例。
 *
 * 接线（与旧版 Oled.c 一致）：
 *   PA4 -> CS
 *   PA5 -> SCK
 *   PA7 -> MOSI
 *   PC4 -> RES（低有效）
 *   PC5 -> DC（0=命令，1=数据）
 */
#ifndef OLED_SPI_EXAMPLE_H
#define OLED_SPI_EXAMPLE_H

#include "oled.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化 GPIO、OLED 实例并显示一屏示例文字。 */
void oled_example_init(void);

/** @brief 返回驱动实例，供应用继续绘制。 */
oled_t *oled_example_device(void);

/** @brief 清屏并显示一行状态文本。 */
oled_status_t oled_example_status(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* OLED_SPI_EXAMPLE_H */
