/**
 * @file  oled.h
 * @brief SSD1306 / SH1106 单色 OLED 驱动（平台无关核心）。
 *
 * 架构
 * ----
 *   本模块只负责显存管理、字模渲染和控制器命令序列，所有总线访问都通过
 *   oled_port_t 回调完成。因此它既可以接软件 SPI、硬件 SPI，也可以接
 *   I2C（由 port 层实现），不依赖任何厂商头文件。
 *
 * 显存模型
 * --------
 *   - 调用者提供一块 width * height / 8 字节的显存；
 *   - 所有绘制函数只改显存，调用 oled_display()/oled_display_area() 才刷屏；
 *   - 坐标原点在左上角，X 向右、Y 向下，超出屏幕的绘制自动裁剪；
 *   - 支持 128x64、128x32 等高度为 8 的倍数的面板。
 *
 * 并发约定
 * --------
 *   - 驱动不包含锁；请保证同一实例的绘制与刷屏不会在中断和主循环中并发；
 *   - 显存由调用者提供，也由调用者决定是否放在 DMA 可访问的内存中。
 *
 * 编码
 * ----
 *   oled_show_text()/oled_printf() 接受 UTF-8。ASCII 使用内置字模；
 *   16x16 汉字从 oled_cjk_font 中查找，未收录字符显示为 '?'。
 */
#ifndef OLED_H
#define OLED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "oled_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 版本 ------------------------------------------------------------- */
#define OLED_VERSION_MAJOR 1
#define OLED_VERSION_MINOR 0
#define OLED_VERSION_PATCH 0

/* ---- 返回状态 --------------------------------------------------------- */
typedef enum {
    OLED_OK = 0,
    OLED_ERR_PARAM = -1,       /**< 参数非法 */
    OLED_ERR_IO = -2,          /**< port 回调报告总线错误 */
    OLED_ERR_UNSUPPORTED = -3, /**< 功能/字体被编译期关闭或控制器不支持 */
    OLED_ERR_BUFFER = -4       /**< 显存缓冲区过小 */
} oled_status_t;

/* ---- 控制器与字体 ----------------------------------------------------- */
typedef enum {
    OLED_CONTROLLER_SSD1306 = 0,
    OLED_CONTROLLER_SH1106 = 1
} oled_controller_t;

typedef enum {
    OLED_FONT_6X8 = 0,
    OLED_FONT_8X16 = 1
} oled_font_t;

typedef enum {
    OLED_NUMBER_UNSIGNED = 0, /**< 十进制无符号 */
    OLED_NUMBER_SIGNED = 1,   /**< 十进制有符号（负数带 '-'） */
    OLED_NUMBER_HEX = 2,      /**< 十六进制，固定 Length 位 */
    OLED_NUMBER_BIN = 3,      /**< 二进制，固定 Length 位 */
    OLED_NUMBER_FLOAT = 4     /**< 整数 Length 位 + 最多 3 位小数 */
} oled_number_format_t;

/** 计算显存大小（字节）。 */
#define OLED_FRAMEBUFFER_SIZE(width, height) \
    ((uint32_t)(width) * (uint32_t)(height) / 8u)

/* ---- 平台端口 --------------------------------------------------------- */

/**
 * @brief 硬件访问回调。
 *
 * write_command / write_data 必须以一次完整事务为单位：回调内部负责
 * 片选、DC 电平、SPI 发送/接收，并在成功时返回 true。len 为 0 时直接返回
 * true。reset 与 delay_ms 可为 NULL：
 *   - reset 为空表示板级已在别处完成复位；
 *   - delay_ms 为空表示初始化中的等待由调用者/板级保证。
 */
typedef struct {
    bool (*write_command)(void *user, const uint8_t *data, uint16_t len);
    bool (*write_data)(void *user, const uint8_t *data, uint16_t len);
    void (*reset)(void *user, bool asserted);
    void (*delay_ms)(void *user, uint32_t ms);
    void *user;
} oled_port_t;

/* ---- 配置与实例 ------------------------------------------------------- */

/**
 * @brief 初始化配置。
 *
 * 使用 oled_config_default() 获得安全默认值，再按面板修改。
 * framebuffer 必须由调用者提供，大小至少
 * OLED_FRAMEBUFFER_SIZE(width, height)。
 */
typedef struct {
    uint16_t width;               /**< 列数，1~128 */
    uint16_t height;              /**< 行数，8~64 且为 8 的倍数 */
    oled_controller_t controller; /**< SSD1306 或 SH1106 */
    uint8_t column_offset;        /**< SH1106 通常为 2；SSD1306 为 0 */
    bool mirror_x;                /**< true = 与默认方向左右镜像 */
    bool mirror_y;                /**< true = 与默认方向上下镜像 */
    bool invert;                  /**< 反色显示 */
    bool external_vcc;            /**< true = 外部 VCC；false = 使用内部电荷泵 */
    uint8_t contrast;             /**< 对比度 0x00~0xFF */
    uint8_t *framebuffer;         /**< 调用者提供的显存 */
    uint32_t framebuffer_size;    /**< 显存字节数 */
} oled_config_t;

/** 驱动实例；字段为内部实现，应用请使用公开 API。 */
typedef struct {
    oled_config_t config;
    oled_port_t port;
    uint16_t pages;
    bool initialized;
    bool display_on;
} oled_t;

/* ---- 生命周期 --------------------------------------------------------- */

/** @brief 填充默认配置（128x64、SSD1306、对比度 0xCF、显存为空）。 */
void oled_config_default(oled_config_t *config);

/**
 * @brief 初始化面板。
 *
 * 依次执行：校验参数 -> 硬件复位（若提供 reset）-> 发送初始化序列 ->
 * 清空显存并刷屏。失败时 initialized 保持 false。
 */
oled_status_t oled_init(oled_t *dev, const oled_config_t *config,
                        const oled_port_t *port);

/* ---- 刷屏 ------------------------------------------------------------- */

/** @brief 将整个显存刷到面板。 */
oled_status_t oled_display(const oled_t *dev);

/** @brief 将显存指定矩形区域刷到面板；区域自动裁剪。 */
oled_status_t oled_display_area(const oled_t *dev, int16_t x, int16_t y,
                                uint16_t width, uint16_t height);

/* ---- 面板控制 --------------------------------------------------------- */

oled_status_t oled_set_contrast(oled_t *dev, uint8_t contrast);
oled_status_t oled_set_invert(oled_t *dev, bool invert);
oled_status_t oled_set_power(oled_t *dev, bool on);

/* ---- 显存操作 --------------------------------------------------------- */

oled_status_t oled_clear(oled_t *dev);
oled_status_t oled_clear_area(oled_t *dev, int16_t x, int16_t y,
                              uint16_t width, uint16_t height);
oled_status_t oled_reverse(oled_t *dev);
oled_status_t oled_reverse_area(oled_t *dev, int16_t x, int16_t y,
                                uint16_t width, uint16_t height);

/* ---- 绘图基本图元 ----------------------------------------------------- */

/** @brief 画/擦除一个像素；越界坐标被忽略。 */
oled_status_t oled_draw_pixel(oled_t *dev, int16_t x, int16_t y, bool on);

/** @brief 读取一个像素；越界返回 OLED_ERR_PARAM。 */
oled_status_t oled_get_pixel(const oled_t *dev, int16_t x, int16_t y, bool *on);

oled_status_t oled_draw_hline(oled_t *dev, int16_t x, int16_t y,
                              uint16_t width, bool on);
oled_status_t oled_draw_vline(oled_t *dev, int16_t x, int16_t y,
                              uint16_t height, bool on);
oled_status_t oled_draw_rect(oled_t *dev, int16_t x, int16_t y,
                             uint16_t width, uint16_t height, bool on);
oled_status_t oled_fill_rect(oled_t *dev, int16_t x, int16_t y,
                             uint16_t width, uint16_t height, bool on);

/* ---- 文本与图像 ------------------------------------------------------- */

/** @brief 字体像素尺寸；字体被关闭时返回 OLED_ERR_UNSUPPORTED。 */
oled_status_t oled_font_metrics(oled_font_t font, uint8_t *width,
                                uint8_t *height);

/** @brief 显示一个 ASCII 字符（可打印字符范围内），不换行。 */
oled_status_t oled_show_char(oled_t *dev, int16_t x, int16_t y, char ch,
                             oled_font_t font);

/** @brief 显示 UTF-8 文本，不换行；越过右边界的内容被裁剪。 */
oled_status_t oled_show_text(oled_t *dev, int16_t x, int16_t y,
                             const char *text, oled_font_t font);

/**
 * @brief 显示一幅图像。
 *
 * 数据格式与字模一致：按页存储，每页 width 字节，字节 bit0 为页内顶部。
 * 高度可以是任意值，但每行像素仍按 8 行一页组织。
 */
oled_status_t oled_show_image(oled_t *dev, int16_t x, int16_t y,
                              uint16_t width, uint16_t height,
                              const uint8_t *image);

/** @brief 按指定格式显示数字；Length 为固定显示的整数位数。 */
oled_status_t oled_show_number(oled_t *dev, int16_t x, int16_t y,
                               double number, oled_number_format_t format,
                               uint8_t length, oled_font_t font);

#if OLED_ENABLE_PRINTF && OLED_ENABLE_TEXT
/**
 * @brief 格式化显示文本，自动换行，支持 '\n'。
 * @return 实际绘制的字符数；参数错误返回负的 oled_status_t。
 */
int oled_printf(oled_t *dev, int16_t x, int16_t y, oled_font_t font,
                const char *format, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif /* OLED_H */
