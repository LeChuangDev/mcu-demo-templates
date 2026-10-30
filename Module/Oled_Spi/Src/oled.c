/**
 * @file  oled.c
 * @brief SSD1306 / SH1106 显存、绘图与文本渲染实现。
 */
#include "oled.h"

#include <string.h>

#include "oled_font.h"

#if OLED_ENABLE_PRINTF && OLED_ENABLE_TEXT
#include <stdarg.h>
#include <stdio.h>
#endif

#define OLED_TRY(expr)                                                       \
    do {                                                                     \
        oled_status_t status_ = (expr);                                      \
        if (status_ != OLED_OK) {                                            \
            return status_;                                                  \
        }                                                                    \
    } while (0)

/* ------------------------------------------------------------------------- */
/* 基础工具                                                                  */
/* ------------------------------------------------------------------------- */

static bool oled_is_ready(const oled_t *dev)
{
    return dev != NULL && dev->config.framebuffer != NULL &&
           dev->config.width != 0u && dev->config.height != 0u;
}

static oled_status_t oled_write_command(oled_t *dev, const uint8_t *data,
                                        uint16_t len)
{
    return dev->port.write_command(dev->port.user, data, len) ? OLED_OK
                                                              : OLED_ERR_IO;
}

static oled_status_t oled_write_data(oled_t *dev, const uint8_t *data,
                                     uint16_t len)
{
    return dev->port.write_data(dev->port.user, data, len) ? OLED_OK
                                                           : OLED_ERR_IO;
}

static oled_status_t oled_cmd(oled_t *dev, uint8_t command)
{
    return oled_write_command(dev, &command, 1u);
}

static oled_status_t oled_cmd2(oled_t *dev, uint8_t command, uint8_t argument)
{
    uint8_t buffer[2];

    buffer[0] = command;
    buffer[1] = argument;
    return oled_write_command(dev, buffer, 2u);
}

/**
 * @brief 切页并设置列地址（页寻址模式，SSD1306/SH1106 通用）。
 */
static oled_status_t oled_set_window(oled_t *dev, uint8_t page, uint16_t column)
{
    uint8_t commands[3];
    uint16_t hardware_column = (uint16_t)(column + dev->config.column_offset);

    if (page >= dev->pages || hardware_column > 131u) {
        return OLED_ERR_PARAM;
    }

    commands[0] = (uint8_t)(0xB0u | page);
    commands[1] = (uint8_t)(0x10u | ((hardware_column >> 4) & 0x0Fu));
    commands[2] = (uint8_t)(hardware_column & 0x0Fu);
    return oled_write_command(dev, commands, 3u);
}

/**
 * @brief 计算矩形与屏幕的交集。
 * @return false 表示交集为空。
 */
static bool oled_clip(const oled_t *dev, int16_t x, int16_t y, uint16_t width,
                      uint16_t height, int16_t *x0, int16_t *y0, int16_t *x1,
                      int16_t *y1)
{
    int32_t left = x;
    int32_t top = y;
    int32_t right = (int32_t)x + (int32_t)width;
    int32_t bottom = (int32_t)y + (int32_t)height;

    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right > (int32_t)dev->config.width) {
        right = (int32_t)dev->config.width;
    }
    if (bottom > (int32_t)dev->config.height) {
        bottom = (int32_t)dev->config.height;
    }

    if (left >= right || top >= bottom) {
        return false;
    }

    *x0 = (int16_t)left;
    *y0 = (int16_t)top;
    *x1 = (int16_t)right;
    *y1 = (int16_t)bottom;
    return true;
}

static void oled_pixel_raw(oled_t *dev, int16_t x, int16_t y, bool on)
{
    uint32_t page = (uint32_t)((uint16_t)y >> 3);
    uint32_t index = page * dev->config.width + (uint16_t)x;
    uint8_t mask = (uint8_t)(1u << ((uint16_t)y & 7u));

    if (on) {
        dev->config.framebuffer[index] |= mask;
    } else {
        dev->config.framebuffer[index] &= (uint8_t)~mask;
    }
}

static int16_t oled_clamp_i16(int32_t value)
{
    if (value > (int32_t)INT16_MAX) {
        return INT16_MAX;
    }
    if (value < (int32_t)INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

/* ------------------------------------------------------------------------- */
/* 生命周期                                                                  */
/* ------------------------------------------------------------------------- */

void oled_config_default(oled_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->width = 128u;
    config->height = 64u;
    config->controller = OLED_CONTROLLER_SSD1306;
    config->column_offset = 0u;
    config->mirror_x = false;
    config->mirror_y = false;
    config->invert = false;
    config->external_vcc = false;
    config->contrast = OLED_DEFAULT_CONTRAST;
}

static oled_status_t oled_send_init_sequence(oled_t *dev)
{
    const oled_config_t *cfg = &dev->config;

    OLED_TRY(oled_cmd(dev, 0xAEu)); /* 关闭显示 */
    OLED_TRY(oled_cmd2(dev, 0xD5u, 0x80u)); /* 时钟分频 */
    OLED_TRY(oled_cmd2(dev, 0xA8u, (uint8_t)(cfg->height - 1u)));
    OLED_TRY(oled_cmd2(dev, 0xD3u, 0x00u)); /* 显示偏移 */
    OLED_TRY(oled_cmd(dev, 0x40u));         /* 起始行 */
    OLED_TRY(oled_cmd(dev, cfg->mirror_x ? 0xA0u : 0xA1u));
    OLED_TRY(oled_cmd(dev, cfg->mirror_y ? 0xC0u : 0xC8u));
    OLED_TRY(oled_cmd2(dev, 0xDAu, cfg->height == 32u ? 0x02u : 0x12u));
    OLED_TRY(oled_cmd2(dev, 0x81u, cfg->contrast));
    OLED_TRY(oled_cmd2(dev, 0xD9u, 0xF1u)); /* 预充电周期 */

    if (cfg->controller == OLED_CONTROLLER_SH1106) {
        OLED_TRY(oled_cmd2(dev, 0xDBu, 0x35u));
    } else {
        OLED_TRY(oled_cmd2(dev, 0xDBu, 0x30u));
    }

    OLED_TRY(oled_cmd(dev, 0xA4u)); /* 显示跟随显存 */
    OLED_TRY(oled_cmd(dev, cfg->invert ? 0xA7u : 0xA6u)); /* 正常/反色 */

    if (cfg->controller == OLED_CONTROLLER_SH1106) {
        OLED_TRY(oled_cmd2(dev, 0xADu, cfg->external_vcc ? 0x8Au : 0x8Bu));
    } else {
        OLED_TRY(oled_cmd2(dev, 0x8Du, cfg->external_vcc ? 0x10u : 0x14u));
    }

    return oled_cmd(dev, 0xAFu); /* 开启显示 */
}

oled_status_t oled_init(oled_t *dev, const oled_config_t *config,
                        const oled_port_t *port)
{
    if (dev == NULL || config == NULL || port == NULL ||
        port->write_command == NULL || port->write_data == NULL) {
        return OLED_ERR_PARAM;
    }
    if (config->framebuffer == NULL) {
        return OLED_ERR_PARAM;
    }
    if (config->width == 0u || config->width > 128u) {
        return OLED_ERR_PARAM;
    }
    if (config->height < 8u || config->height > 64u ||
        (config->height % 8u) != 0u) {
        return OLED_ERR_PARAM;
    }
    if (config->controller != OLED_CONTROLLER_SSD1306 &&
        config->controller != OLED_CONTROLLER_SH1106) {
        return OLED_ERR_PARAM;
    }
    if (config->framebuffer_size <
        OLED_FRAMEBUFFER_SIZE(config->width, config->height)) {
        return OLED_ERR_BUFFER;
    }

    memset(dev, 0, sizeof(*dev));
    dev->config = *config;
    dev->port = *port;
    dev->pages = (uint16_t)(config->height / 8u);

    /* 硬件复位：RES# 低有效。 */
    if (port->reset != NULL) {
        port->reset(port->user, true);
        if (port->delay_ms != NULL) {
            port->delay_ms(port->user, 10u);
        }
        port->reset(port->user, false);
        if (port->delay_ms != NULL) {
            port->delay_ms(port->user, 120u);
        }
    } else if (port->delay_ms != NULL) {
        port->delay_ms(port->user, 120u);
    }

    memset(dev->config.framebuffer, 0,
           (size_t)dev->pages * dev->config.width);

    OLED_TRY(oled_send_init_sequence(dev));

    dev->initialized = true;
    dev->display_on = true;

    OLED_TRY(oled_display(dev));
    return OLED_OK;
}

/* ------------------------------------------------------------------------- */
/* 刷屏                                                                      */
/* ------------------------------------------------------------------------- */

oled_status_t oled_display_area(const oled_t *dev, int16_t x, int16_t y,
                                uint16_t width, uint16_t height)
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    oled_t *mutable_dev;
    uint16_t page;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    if (width == 0u || height == 0u) {
        return OLED_OK;
    }
    if (!oled_clip(dev, x, y, width, height, &x0, &y0, &x1, &y1)) {
        return OLED_OK;
    }

    mutable_dev = (oled_t *)dev; /* port 回调需要非 const 实例 */

    for (page = (uint16_t)((uint16_t)y0 >> 3);
         page < (uint16_t)(((uint16_t)y1 + 7u) >> 3); ++page) {
        OLED_TRY(oled_set_window(mutable_dev, (uint8_t)page, (uint16_t)x0));
        OLED_TRY(oled_write_data(mutable_dev,
                                 &dev->config.framebuffer[(size_t)page *
                                                              dev->config.width +
                                                          (uint16_t)x0],
                                 (uint16_t)(x1 - x0)));
    }

    return OLED_OK;
}

oled_status_t oled_display(const oled_t *dev)
{
    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    return oled_display_area(dev, 0, 0, dev->config.width, dev->config.height);
}

/* ------------------------------------------------------------------------- */
/* 面板控制                                                                  */
/* ------------------------------------------------------------------------- */

oled_status_t oled_set_contrast(oled_t *dev, uint8_t contrast)
{
    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }

    OLED_TRY(oled_cmd2(dev, 0x81u, contrast));
    dev->config.contrast = contrast;
    return OLED_OK;
}

oled_status_t oled_set_invert(oled_t *dev, bool invert)
{
    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }

    OLED_TRY(oled_cmd(dev, invert ? 0xA7u : 0xA6u));
    dev->config.invert = invert;
    return OLED_OK;
}

oled_status_t oled_set_power(oled_t *dev, bool on)
{
    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }

    OLED_TRY(oled_cmd(dev, on ? 0xAFu : 0xAEu));
    dev->display_on = on;
    return OLED_OK;
}

/* ------------------------------------------------------------------------- */
/* 显存操作                                                                  */
/* ------------------------------------------------------------------------- */

oled_status_t oled_clear(oled_t *dev)
{
    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }

    memset(dev->config.framebuffer, 0,
           (size_t)dev->pages * dev->config.width);
    return OLED_OK;
}

oled_status_t oled_clear_area(oled_t *dev, int16_t x, int16_t y,
                              uint16_t width, uint16_t height)
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    int16_t row;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    if (!oled_clip(dev, x, y, width, height, &x0, &y0, &x1, &y1)) {
        return OLED_OK;
    }

    for (row = y0; row < y1; ++row) {
        uint32_t page = (uint32_t)((uint16_t)row >> 3);
        uint8_t mask = (uint8_t)(1u << ((uint16_t)row & 7u));
        uint8_t *line = &dev->config.framebuffer[page * dev->config.width];
        int16_t column;

        for (column = x0; column < x1; ++column) {
            line[(uint16_t)column] &= (uint8_t)~mask;
        }
    }

    return OLED_OK;
}

oled_status_t oled_reverse(oled_t *dev)
{
    size_t count;
    size_t i;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }

    count = (size_t)dev->pages * dev->config.width;
    for (i = 0u; i < count; ++i) {
        dev->config.framebuffer[i] ^= 0xFFu;
    }
    return OLED_OK;
}

oled_status_t oled_reverse_area(oled_t *dev, int16_t x, int16_t y,
                                uint16_t width, uint16_t height)
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    int16_t row;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    if (!oled_clip(dev, x, y, width, height, &x0, &y0, &x1, &y1)) {
        return OLED_OK;
    }

    for (row = y0; row < y1; ++row) {
        uint32_t page = (uint32_t)((uint16_t)row >> 3);
        uint8_t mask = (uint8_t)(1u << ((uint16_t)row & 7u));
        uint8_t *line = &dev->config.framebuffer[page * dev->config.width];
        int16_t column;

        for (column = x0; column < x1; ++column) {
            line[(uint16_t)column] ^= mask;
        }
    }

    return OLED_OK;
}

/* ------------------------------------------------------------------------- */
/* 基本图元                                                                  */
/* ------------------------------------------------------------------------- */

oled_status_t oled_draw_pixel(oled_t *dev, int16_t x, int16_t y, bool on)
{
    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    if (x < 0 || y < 0 || x >= (int16_t)dev->config.width ||
        y >= (int16_t)dev->config.height) {
        return OLED_OK; /* 裁剪 */
    }

    oled_pixel_raw(dev, x, y, on);
    return OLED_OK;
}

oled_status_t oled_get_pixel(const oled_t *dev, int16_t x, int16_t y, bool *on)
{
    uint32_t page;
    uint8_t mask;

    if (!oled_is_ready(dev) || on == NULL) {
        return OLED_ERR_PARAM;
    }
    if (x < 0 || y < 0 || x >= (int16_t)dev->config.width ||
        y >= (int16_t)dev->config.height) {
        return OLED_ERR_PARAM;
    }

    page = (uint32_t)((uint16_t)y >> 3);
    mask = (uint8_t)(1u << ((uint16_t)y & 7u));
    *on = (dev->config.framebuffer[page * dev->config.width + (uint16_t)x] &
           mask) != 0u;
    return OLED_OK;
}

oled_status_t oled_draw_hline(oled_t *dev, int16_t x, int16_t y,
                              uint16_t width, bool on)
{
    uint16_t i;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    if (y < 0 || y >= (int16_t)dev->config.height) {
        return OLED_OK;
    }

    for (i = 0u; i < width; ++i) {
        int32_t column = (int32_t)x + (int32_t)i;

        if (column < 0 || column >= (int32_t)dev->config.width) {
            continue;
        }
        oled_pixel_raw(dev, (int16_t)column, y, on);
    }
    return OLED_OK;
}

oled_status_t oled_draw_vline(oled_t *dev, int16_t x, int16_t y,
                              uint16_t height, bool on)
{
    uint16_t i;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    if (x < 0 || x >= (int16_t)dev->config.width) {
        return OLED_OK;
    }

    for (i = 0u; i < height; ++i) {
        int32_t row = (int32_t)y + (int32_t)i;

        if (row < 0 || row >= (int32_t)dev->config.height) {
            continue;
        }
        oled_pixel_raw(dev, x, (int16_t)row, on);
    }
    return OLED_OK;
}

oled_status_t oled_draw_rect(oled_t *dev, int16_t x, int16_t y,
                             uint16_t width, uint16_t height, bool on)
{
    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    if (width == 0u || height == 0u) {
        return OLED_OK;
    }

    OLED_TRY(oled_draw_hline(dev, x, y, width, on));
    OLED_TRY(oled_draw_hline(dev, x,
                             oled_clamp_i16((int32_t)y + (int32_t)height - 1),
                             width, on));
    OLED_TRY(oled_draw_vline(dev, x, y, height, on));
    OLED_TRY(oled_draw_vline(dev,
                             oled_clamp_i16((int32_t)x + (int32_t)width - 1), y,
                             height, on));
    return OLED_OK;
}

oled_status_t oled_fill_rect(oled_t *dev, int16_t x, int16_t y,
                             uint16_t width, uint16_t height, bool on)
{
    uint16_t row;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }

    for (row = 0u; row < height; ++row) {
        int32_t target = (int32_t)y + (int32_t)row;

        if (target < 0 || target >= (int32_t)dev->config.height) {
            continue;
        }
        OLED_TRY(oled_draw_hline(dev, x, (int16_t)target, width, on));
    }
    return OLED_OK;
}

/* ------------------------------------------------------------------------- */
/* 字体与文本                                                                */
/* ------------------------------------------------------------------------- */

oled_status_t oled_font_metrics(oled_font_t font, uint8_t *width,
                                uint8_t *height)
{
    if (width == NULL || height == NULL) {
        return OLED_ERR_PARAM;
    }

    switch (font) {
#if OLED_ENABLE_FONT_6X8
    case OLED_FONT_6X8:
        *width = 6u;
        *height = 8u;
        return OLED_OK;
#endif
#if OLED_ENABLE_FONT_8X16
    case OLED_FONT_8X16:
        *width = 8u;
        *height = 16u;
        return OLED_OK;
#endif
    default:
        break;
    }

    return OLED_ERR_UNSUPPORTED;
}

#if OLED_ENABLE_TEXT

typedef struct {
    const uint8_t *data;
    uint8_t width;
    uint8_t height;
} oled_glyph_t;

static oled_status_t oled_ascii_glyph(oled_font_t font, uint8_t character,
                                      oled_glyph_t *glyph)
{
    if (character < 0x20u || character > 0x7Eu) {
        character = (uint8_t)' ';
    }

    switch (font) {
#if OLED_ENABLE_FONT_6X8
    case OLED_FONT_6X8:
        glyph->data = oled_font_6x8[character - 0x20u];
        glyph->width = 6u;
        glyph->height = 8u;
        return OLED_OK;
#endif
#if OLED_ENABLE_FONT_8X16
    case OLED_FONT_8X16:
        glyph->data = oled_font_8x16[character - 0x20u];
        glyph->width = 8u;
        glyph->height = 16u;
        return OLED_OK;
#endif
    default:
        break;
    }

    return OLED_ERR_UNSUPPORTED;
}

#if OLED_ENABLE_CJK
static const oled_cjk_glyph_t *oled_cjk_lookup(const uint8_t *text,
                                               size_t length)
{
    const oled_cjk_glyph_t *glyph;

    for (glyph = oled_cjk_font; glyph->index[0] != '\0'; ++glyph) {
        if (strlen(glyph->index) == length &&
            memcmp(glyph->index, text, length) == 0) {
            return glyph;
        }
    }
    return NULL;
}
#endif

/**
 * @brief 解码一个 UTF-8 码点。
 * @return 消耗的字节数（至少 1）；非法序列返回 U+FFFD 并只消耗 1 字节。
 */
static size_t oled_utf8_decode(const uint8_t *text, size_t remaining,
                               uint32_t *codepoint)
{
    uint32_t value;
    uint32_t minimum;
    size_t length;
    size_t i;

    if (remaining == 0u) {
        *codepoint = 0xFFFDu;
        return 0u;
    }

    if (text[0] < 0x80u) {
        *codepoint = text[0];
        return 1u;
    }

    if ((text[0] & 0xE0u) == 0xC0u) {
        length = 2u;
        value = (uint32_t)(text[0] & 0x1Fu);
        minimum = 0x80u;
    } else if ((text[0] & 0xF0u) == 0xE0u) {
        length = 3u;
        value = (uint32_t)(text[0] & 0x0Fu);
        minimum = 0x800u;
    } else if ((text[0] & 0xF8u) == 0xF0u) {
        length = 4u;
        value = (uint32_t)(text[0] & 0x07u);
        minimum = 0x10000u;
    } else {
        *codepoint = 0xFFFDu;
        return 1u;
    }

    if (remaining < length) {
        *codepoint = 0xFFFDu;
        return 1u;
    }

    for (i = 1u; i < length; ++i) {
        if ((text[i] & 0xC0u) != 0x80u) {
            *codepoint = 0xFFFDu;
            return 1u;
        }
        value = (value << 6) | (uint32_t)(text[i] & 0x3Fu);
    }

    if (value < minimum || value > 0x10FFFFu ||
        (value >= 0xD800u && value <= 0xDFFFu)) {
        *codepoint = 0xFFFDu;
        return 1u;
    }

    *codepoint = value;
    return length;
}

/**
 * @brief 文本绘制核心。
 *
 * @param wrap    true 时超出右边界自动换行；false 时只做裁剪。
 * @param drawn   可为 NULL；输出处理的字符数（不含 '\n'）。
 */
static oled_status_t oled_draw_text(oled_t *dev, int16_t x, int16_t y,
                                    const char *text, oled_font_t font,
                                    bool wrap, int *drawn)
{
    uint8_t ascii_width;
    uint8_t ascii_height;
    uint8_t line_height;
    size_t text_length;
    size_t index = 0u;
    int32_t cursor_x = x;
    int32_t cursor_y = y;
    int count = 0;
    oled_status_t status;

    if (!oled_is_ready(dev) || text == NULL) {
        return OLED_ERR_PARAM;
    }

    status = oled_font_metrics(font, &ascii_width, &ascii_height);
    if (status != OLED_OK) {
        return status;
    }
    line_height = ascii_height;

    text_length = strlen(text);
    while (index < text_length) {
        uint32_t codepoint;
        size_t consumed;
        oled_glyph_t glyph;
        bool found = false;

        if (text[index] == '\n') {
            cursor_x = x;
            cursor_y += (int32_t)line_height;
            index++;
            continue;
        }

        consumed = oled_utf8_decode((const uint8_t *)&text[index],
                                    text_length - index, &codepoint);
        if (consumed == 0u) {
            break;
        }

        if (codepoint < 0x80u) {
            if (codepoint < 0x20u) {
                index += consumed;
                continue; /* 其他控制字符不绘制 */
            }
            status = oled_ascii_glyph(font, (uint8_t)codepoint, &glyph);
            if (status != OLED_OK) {
                return status;
            }
            found = true;
        } else if (font == OLED_FONT_8X16 && OLED_ENABLE_CJK) {
#if OLED_ENABLE_CJK
            const oled_cjk_glyph_t *cjk =
                oled_cjk_lookup((const uint8_t *)&text[index], consumed);

            if (cjk != NULL) {
                glyph.data = cjk->data;
                glyph.width = 16u;
                glyph.height = 16u;
                line_height = 16u;
                found = true;
            }
#endif
        }

        if (!found) {
            /* 未收录的字符统一显示为 '?'。 */
            status = oled_ascii_glyph(font, (uint8_t)'?', &glyph);
            if (status != OLED_OK) {
                return status;
            }
        }

        if (wrap && (cursor_x + (int32_t)glyph.width) >
                        (int32_t)dev->config.width) {
            cursor_x = x;
            cursor_y += (int32_t)line_height;
        }

        if (cursor_y >= (int32_t)dev->config.height) {
            break; /* 屏幕下方已无空间 */
        }

        if (cursor_x < (int32_t)dev->config.width &&
            cursor_y + (int32_t)glyph.height > 0) {
            status = oled_show_image(dev, (int16_t)cursor_x, (int16_t)cursor_y,
                                     glyph.width, glyph.height, glyph.data);
            if (status != OLED_OK) {
                return status;
            }
            count++;
        }

        cursor_x += (int32_t)glyph.width;
        index += consumed;
    }

    if (drawn != NULL) {
        *drawn = count;
    }
    return OLED_OK;
}

#endif /* OLED_ENABLE_TEXT */

oled_status_t oled_show_char(oled_t *dev, int16_t x, int16_t y, char character,
                             oled_font_t font)
{
#if OLED_ENABLE_TEXT
    oled_glyph_t glyph;
    oled_status_t status;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }

    status = oled_ascii_glyph(font, (uint8_t)character, &glyph);
    if (status != OLED_OK) {
        return status;
    }
    return oled_show_image(dev, x, y, glyph.width, glyph.height, glyph.data);
#else
    (void)dev;
    (void)x;
    (void)y;
    (void)character;
    (void)font;
    return OLED_ERR_UNSUPPORTED;
#endif
}

oled_status_t oled_show_text(oled_t *dev, int16_t x, int16_t y,
                             const char *text, oled_font_t font)
{
#if OLED_ENABLE_TEXT
    return oled_draw_text(dev, x, y, text, font, false, NULL);
#else
    (void)dev;
    (void)x;
    (void)y;
    (void)text;
    (void)font;
    return OLED_ERR_UNSUPPORTED;
#endif
}

oled_status_t oled_show_image(oled_t *dev, int16_t x, int16_t y,
                              uint16_t width, uint16_t height,
                              const uint8_t *image)
{
    uint16_t row;
    int32_t first_column;
    int32_t last_column;

    if (!oled_is_ready(dev)) {
        return OLED_ERR_PARAM;
    }
    if (image == NULL) {
        return OLED_ERR_PARAM;
    }
    if (width == 0u || height == 0u) {
        return OLED_OK;
    }

    /* 预计算可见列区间，避免逐像素判断。 */
    first_column = (x < 0) ? -(int32_t)x : 0;
    last_column = (int32_t)width;
    if ((int32_t)x + last_column > (int32_t)dev->config.width) {
        last_column = (int32_t)dev->config.width - x;
    }
    if (first_column >= last_column) {
        return OLED_OK;
    }

    for (row = 0u; row < height; ++row) {
        int32_t target_row = (int32_t)y + (int32_t)row;
        const uint8_t *source;
        uint8_t source_mask;
        uint8_t *line;
        uint8_t target_mask;
        int32_t column;

        if (target_row < 0 || target_row >= (int32_t)dev->config.height) {
            continue;
        }

        source = &image[((size_t)row >> 3) * width];
        source_mask = (uint8_t)(1u << (row & 7u));
        line = &dev->config.framebuffer[((size_t)target_row >> 3) *
                                            dev->config.width];
        target_mask = (uint8_t)(1u << ((uint16_t)target_row & 7u));

        for (column = first_column; column < last_column; ++column) {
            int32_t target_column = (int32_t)x + column;

            if ((source[column] & source_mask) != 0u) {
                line[target_column] |= target_mask;
            } else {
                line[target_column] &= (uint8_t)~target_mask;
            }
        }
    }

    return OLED_OK;
}

#if OLED_ENABLE_NUMBER && OLED_ENABLE_TEXT
static uint32_t oled_pow_u32(uint32_t base, uint8_t exponent)
{
    uint32_t result = 1u;

    while (exponent-- > 0u) {
        result *= base;
    }
    return result;
}

/** 将 double 安全转换为 uint32_t：负数/NaN 视为 0，超上界饱和。 */
static uint32_t oled_double_to_u32(double value)
{
    if (!(value > 0.0)) {
        return 0u;
    }
    if (value >= 4294967295.0) {
        return 0xFFFFFFFFu;
    }
    return (uint32_t)value;
}

/** 将 double 转换为 32 位位模式（负数按 int32 截断后转 uint32）。 */
static uint32_t oled_double_to_bits(double value)
{
    if (value < 0.0) {
        if (value <= -2147483648.0) {
            return 0x80000000u;
        }
        return (uint32_t)(int32_t)value;
    }
    return oled_double_to_u32(value);
}
#endif

oled_status_t oled_show_number(oled_t *dev, int16_t x, int16_t y, double number,
                               oled_number_format_t format, uint8_t length,
                               oled_font_t font)
{
#if !OLED_ENABLE_NUMBER
    (void)dev;
    (void)x;
    (void)y;
    (void)number;
    (void)format;
    (void)length;
    (void)font;
    return OLED_ERR_UNSUPPORTED;
#elif !OLED_ENABLE_TEXT
    (void)dev;
    (void)x;
    (void)y;
    (void)number;
    (void)format;
    (void)length;
    (void)font;
    return OLED_ERR_UNSUPPORTED;
#else
    char output[48];
    size_t used = 0u;
    uint8_t width;
    uint8_t height;
    oled_status_t status;
    uint8_t effective_length = 1u;
    bool negative = number < 0.0;

    if (!oled_is_ready(dev) || length == 0u) {
        return OLED_ERR_PARAM;
    }
    if (format < OLED_NUMBER_UNSIGNED || format > OLED_NUMBER_FLOAT) {
        return OLED_ERR_PARAM;
    }

    status = oled_font_metrics(font, &width, &height);
    if (status != OLED_OK) {
        return status;
    }
    (void)width;
    (void)height;

    switch (format) {
    case OLED_NUMBER_UNSIGNED:
    case OLED_NUMBER_SIGNED: {
        uint32_t value;
        uint8_t digit;

        effective_length = (length > 10u) ? 10u : length;
        if (format == OLED_NUMBER_SIGNED && negative) {
            output[used++] = '-';
        }
        value = oled_double_to_u32(negative ? -number : number);
        for (digit = 0u; digit < effective_length; ++digit) {
            uint32_t divisor =
                oled_pow_u32(10u, (uint8_t)(effective_length - digit - 1u));
            output[used++] =
                (char)('0' + (char)((value / divisor) % 10u));
        }
        break;
    }
    case OLED_NUMBER_HEX: {
        uint8_t digit;
        uint32_t value = oled_double_to_bits(number);

        effective_length = (length > 8u) ? 8u : length;
        for (digit = 0u; digit < effective_length; ++digit) {
            uint32_t divisor =
                oled_pow_u32(16u, (uint8_t)(effective_length - digit - 1u));
            uint8_t nibble = (uint8_t)((value / divisor) % 16u);
            output[used++] =
                (char)((nibble < 10u) ? (char)('0' + nibble)
                                      : (char)('A' + (int)nibble - 10));
        }
        break;
    }
    case OLED_NUMBER_BIN: {
        uint8_t digit;
        uint32_t value = oled_double_to_bits(number);

        effective_length = (length > 32u) ? 32u : length;
        for (digit = 0u; digit < effective_length; ++digit) {
            uint32_t divisor =
                oled_pow_u32(2u, (uint8_t)(effective_length - digit - 1u));
            output[used++] = (char)('0' + (char)((value / divisor) % 2u));
        }
        break;
    }
    case OLED_NUMBER_FLOAT: {
        uint32_t integer_part;
        uint32_t fraction;
        uint8_t fraction_digit[3];
        uint8_t fraction_length = 3u;
        uint8_t digit;
        double magnitude = negative ? -number : number;

        effective_length = (length > 10u) ? 10u : length;
        if (magnitude >= 4294967295.0) {
            integer_part = 0xFFFFFFFFu;
            fraction = 0u;
        } else {
            integer_part = (uint32_t)magnitude;
            fraction = (uint32_t)((magnitude - (double)integer_part) * 1000.0 +
                                  0.5);
            if (fraction >= 1000u) {
                fraction -= 1000u;
                integer_part++;
            }
        }

        fraction_digit[0] = (uint8_t)(fraction / 100u);
        fraction_digit[1] = (uint8_t)((fraction / 10u) % 10u);
        fraction_digit[2] = (uint8_t)(fraction % 10u);
        while (fraction_length > 1u &&
               fraction_digit[fraction_length - 1u] == 0u) {
            fraction_length--;
        }

        if (negative) {
            output[used++] = '-';
        }
        for (digit = 0u; digit < effective_length; ++digit) {
            uint32_t divisor =
                oled_pow_u32(10u, (uint8_t)(effective_length - digit - 1u));
            output[used++] =
                (char)('0' + (char)((integer_part / divisor) % 10u));
        }
        output[used++] = '.';
        for (digit = 0u; digit < fraction_length; ++digit) {
            output[used++] = (char)('0' + fraction_digit[digit]);
        }
        break;
    }
    default:
        return OLED_ERR_PARAM;
    }

    output[used] = '\0';
    return oled_show_text(dev, x, y, output, font);
#endif
}

#if OLED_ENABLE_PRINTF && OLED_ENABLE_TEXT
int oled_printf(oled_t *dev, int16_t x, int16_t y, oled_font_t font,
                const char *format, ...)
{
    char buffer[OLED_PRINTF_BUFFER_SIZE];
    va_list arguments;
    int result;
    int drawn = 0;
    oled_status_t status;

    if (!oled_is_ready(dev) || format == NULL) {
        return (int)OLED_ERR_PARAM;
    }

    va_start(arguments, format);
    result = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (result < 0) {
        return (int)OLED_ERR_PARAM;
    }

    status = oled_draw_text(dev, x, y, buffer, font, true, &drawn);
    if (status != OLED_OK) {
        return (int)status;
    }
    return drawn;
}
#endif

#undef OLED_TRY
