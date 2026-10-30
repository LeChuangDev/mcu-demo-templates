/**
 * @file  oled_config.h
 * @brief OLED 驱动编译期裁剪开关。
 *
 * 所有宏都可被 -D 覆盖。关闭不用的字体/功能可以显著减小 Flash 占用；
 * 字体数据位于 Src/oled_font.c，关闭后对应表不会被链接进来。
 */
#ifndef OLED_CONFIG_H
#define OLED_CONFIG_H

/* ---- 字体 ------------------------------------------------------------- */
#ifndef OLED_ENABLE_FONT_6X8
#define OLED_ENABLE_FONT_6X8 1
#endif

#ifndef OLED_ENABLE_FONT_8X16
#define OLED_ENABLE_FONT_8X16 1
#endif

/* 16x16 汉字字模。关闭后非 ASCII 字符统一显示为 '?'。 */
#ifndef OLED_ENABLE_CJK
#define OLED_ENABLE_CJK 1
#endif

/* ---- 功能模块 --------------------------------------------------------- */
/* 文本渲染（show_char/show_text）。关闭后这两个 API 仍存在但返回不支持。 */
#ifndef OLED_ENABLE_TEXT
#define OLED_ENABLE_TEXT 1
#endif

/* 数字格式化显示。 */
#ifndef OLED_ENABLE_NUMBER
#define OLED_ENABLE_NUMBER 1
#endif

/* printf 风格格式化显示；依赖 C 库 vsnprintf，可关闭以省 Flash。 */
#ifndef OLED_ENABLE_PRINTF
#define OLED_ENABLE_PRINTF 1
#endif

/* ---- 参数 ------------------------------------------------------------- */
/* printf 输出缓冲区大小。超长内容会被截断，不会溢出栈。 */
#ifndef OLED_PRINTF_BUFFER_SIZE
#define OLED_PRINTF_BUFFER_SIZE 96
#endif

/* 默认对比度（SSD1306/SH1106 的有效范围 0x00~0xFF）。 */
#ifndef OLED_DEFAULT_CONTRAST
#define OLED_DEFAULT_CONTRAST 0xCF
#endif

#endif /* OLED_CONFIG_H */
