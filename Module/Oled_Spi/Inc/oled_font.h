/**
 * @file  oled_font.h
 * @brief OLED 内置字模表声明。
 *
 * 字模采用“列优先、页内低位在上”的 SSD1306 原生格式：
 *   - ASCII 6x8  ：每字符 6 字节，字节 bit0 对应字符顶部像素；
 *   - ASCII 8x16 ：每字符 16 字节，前 8 字节为上半页，后 8 字节为下半页；
 *   - 汉字 16x16：每字 32 字节，前 16 字节为上半页，后 16 字节为下半页。
 */
#ifndef OLED_FONT_H
#define OLED_FONT_H

#include <stdint.h>

#include "oled_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if OLED_ENABLE_FONT_8X16
extern const uint8_t oled_font_8x16[][16];
#endif

#if OLED_ENABLE_FONT_6X8
extern const uint8_t oled_font_6x8[][6];
#endif

#if OLED_ENABLE_CJK
typedef struct {
    const char *index;      /**< UTF-8 字符序列；空字符串表示表结束 */
    const uint8_t data[32]; /**< 16x16 字模 */
} oled_cjk_glyph_t;

extern const oled_cjk_glyph_t oled_cjk_font[];
#endif

#ifdef __cplusplus
}
#endif

#endif /* OLED_FONT_H */
