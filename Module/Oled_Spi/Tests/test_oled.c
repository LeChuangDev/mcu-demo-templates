/**
 * @file  test_oled.c
 * @brief OLED 驱动主机端单元测试（假端口，不依赖硬件）。
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "oled.h"
#include "oled_font.h"

#define FB_WIDTH 128u
#define FB_HEIGHT 64u
#define FB_SIZE (FB_WIDTH * FB_HEIGHT / 8u)

static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            failures++;                                                      \
        }                                                                    \
    } while (0)

/* ---- 假端口 ------------------------------------------------------------ */

typedef struct {
    uint8_t commands[2048];
    size_t command_len;
    unsigned command_calls;
    uint8_t data[8192];
    size_t data_len;
    unsigned data_calls;
    unsigned reset_calls;
    bool last_reset;
    unsigned delay_calls;
    bool fail_command;
    bool fail_data;
} fake_port_t;

static bool fake_write_command(void *user, const uint8_t *data, uint16_t len)
{
    fake_port_t *port = (fake_port_t *)user;

    port->command_calls++;
    if (port->fail_command) {
        return false;
    }
    if (port->command_len + len > sizeof(port->commands)) {
        return false;
    }
    memcpy(&port->commands[port->command_len], data, len);
    port->command_len += len;
    return true;
}

static bool fake_write_data(void *user, const uint8_t *data, uint16_t len)
{
    fake_port_t *port = (fake_port_t *)user;

    port->data_calls++;
    if (port->fail_data) {
        return false;
    }
    if (port->data_len + len > sizeof(port->data)) {
        return false;
    }
    memcpy(&port->data[port->data_len], data, len);
    port->data_len += len;
    return true;
}

static void fake_reset(void *user, bool asserted)
{
    fake_port_t *port = (fake_port_t *)user;

    port->reset_calls++;
    port->last_reset = asserted;
}

static void fake_delay(void *user, uint32_t ms)
{
    fake_port_t *port = (fake_port_t *)user;

    (void)ms;
    port->delay_calls++;
}

static uint8_t framebuffer[FB_SIZE];
static uint8_t reference_fb[FB_SIZE];
static oled_t display;
static fake_port_t fake;
static oled_port_t port;

static void fake_init(void)
{
    memset(&fake, 0, sizeof(fake));
    port.write_command = fake_write_command;
    port.write_data = fake_write_data;
    port.reset = fake_reset;
    port.delay_ms = fake_delay;
    port.user = &fake;
}

static bool setup_display(uint16_t width, uint16_t height, bool with_reset)
{
    oled_config_t config;

    memset(framebuffer, 0, sizeof(framebuffer));
    fake_init();
    if (!with_reset) {
        port.reset = NULL;
        port.delay_ms = NULL;
    }

    oled_config_default(&config);
    config.width = width;
    config.height = height;
    config.framebuffer = framebuffer;
    config.framebuffer_size = sizeof(framebuffer);

    return oled_init(&display, &config, &port) == OLED_OK;
}

static bool pixel_is(int16_t x, int16_t y, bool expected)
{
    bool value = false;

    if (oled_get_pixel(&display, x, y, &value) != OLED_OK) {
        return false;
    }
    return value == expected;
}

/* ---- 基础测试 ---------------------------------------------------------- */

static void test_config_default(void)
{
    oled_config_t config;

    oled_config_default(&config);
    CHECK(config.width == 128u);
    CHECK(config.height == 64u);
    CHECK(config.controller == OLED_CONTROLLER_SSD1306);
    CHECK(config.contrast == OLED_DEFAULT_CONTRAST);
    CHECK(config.framebuffer == NULL);
}

static void test_init_sequence(void)
{
    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));
    CHECK(display.initialized == true);
    CHECK(display.display_on == true);
    CHECK(display.pages == 8u);
    CHECK(fake.reset_calls == 2u);
    CHECK(fake.last_reset == false);
    CHECK(fake.delay_calls >= 2u);
    CHECK(fake.command_calls > 10u);
    CHECK(fake.command_len >= 2u);
    CHECK(fake.commands[0] == 0xAEu);

    bool saw_display_on = false;
    size_t i;
    for (i = 0u; i < fake.command_len; ++i) {
        if (fake.commands[i] == 0xAFu) {
            saw_display_on = true;
        }
    }
    CHECK(saw_display_on);
    CHECK(fake.data_len == 1024u);
}

static void test_init_validation(void)
{
    oled_config_t config;
    oled_t dev;

    memset(&config, 0, sizeof(config));
    fake_init();

    CHECK(oled_init(NULL, &config, &port) == OLED_ERR_PARAM);
    CHECK(oled_init(&dev, NULL, &port) == OLED_ERR_PARAM);
    CHECK(oled_init(&dev, &config, NULL) == OLED_ERR_PARAM);

    oled_config_default(&config);
    config.framebuffer = framebuffer;
    config.framebuffer_size = sizeof(framebuffer);

    config.width = 0u;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_PARAM);
    config.width = 129u;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_PARAM);
    config.width = 128u;

    config.height = 0u;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_PARAM);
    config.height = 60u;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_PARAM);
    config.height = 72u;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_PARAM);
    config.height = 64u;

    config.controller = (oled_controller_t)7;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_PARAM);
    config.controller = OLED_CONTROLLER_SSD1306;

    config.framebuffer = NULL;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_PARAM);
    config.framebuffer = framebuffer;

    config.framebuffer_size = 10u;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_BUFFER);
    config.framebuffer_size = sizeof(framebuffer);

    port.write_data = NULL;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_PARAM);
    fake_init();

    fake.fail_command = true;
    CHECK(oled_init(&dev, &config, &port) == OLED_ERR_IO);
    CHECK(dev.initialized == false);
}

static void test_pixel_and_clipping(void)
{
    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));

    CHECK(oled_draw_pixel(&display, 0, 0, true) == OLED_OK);
    CHECK(oled_draw_pixel(&display, 127, 63, true) == OLED_OK);
    CHECK(pixel_is(0, 0, true));
    CHECK(pixel_is(127, 63, true));
    CHECK(pixel_is(1, 0, false));

    CHECK(oled_draw_pixel(&display, 0, 0, false) == OLED_OK);
    CHECK(pixel_is(0, 0, false));

    /* 越界绘制应被裁剪且不报错；读取越界应报参数错误。 */
    CHECK(oled_draw_pixel(&display, -1, 0, true) == OLED_OK);
    CHECK(oled_draw_pixel(&display, 128, 0, true) == OLED_OK);

    bool value;
    CHECK(oled_get_pixel(&display, -1, 0, &value) == OLED_ERR_PARAM);
    CHECK(oled_get_pixel(&display, 128, 0, &value) == OLED_ERR_PARAM);
    CHECK(oled_get_pixel(&display, 0, 64, &value) == OLED_ERR_PARAM);
    CHECK(oled_get_pixel(&display, 0, 0, NULL) == OLED_ERR_PARAM);
}

static void test_area_operations(void)
{
    int16_t x;
    int16_t y;

    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));

    CHECK(oled_fill_rect(&display, 2, 2, 10, 10, true) == OLED_OK);
    CHECK(pixel_is(2, 2, true));
    CHECK(pixel_is(11, 11, true));
    CHECK(pixel_is(12, 11, false));

    CHECK(oled_clear_area(&display, 4, 4, 4, 4) == OLED_OK);
    for (y = 4; y < 8; ++y) {
        for (x = 4; x < 8; ++x) {
            CHECK(pixel_is(x, y, false));
        }
    }
    CHECK(pixel_is(2, 2, true));

    CHECK(oled_reverse_area(&display, 10, 10, 2, 2) == OLED_OK);
    CHECK(pixel_is(10, 10, false));
    CHECK(pixel_is(11, 11, false));

    CHECK(oled_fill_rect(&display, 0, 0, FB_WIDTH, FB_HEIGHT, true) ==
          OLED_OK);
    CHECK(oled_reverse(&display) == OLED_OK);
    CHECK(pixel_is(0, 0, false));
    CHECK(pixel_is(127, 63, false));
    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(pixel_is(0, 0, false));
}

static void test_lines_and_rect(void)
{
    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));

    CHECK(oled_draw_hline(&display, 0, 0, FB_WIDTH, true) == OLED_OK);
    CHECK(oled_draw_vline(&display, 0, 0, FB_HEIGHT, true) == OLED_OK);
    CHECK(pixel_is(0, 0, true));
    CHECK(pixel_is(127, 0, true));
    CHECK(pixel_is(0, 63, true));
    CHECK(pixel_is(1, 1, false));

    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_draw_rect(&display, 2, 2, 10, 10, true) == OLED_OK);
    CHECK(pixel_is(2, 2, true));
    CHECK(pixel_is(11, 2, true));
    CHECK(pixel_is(2, 11, true));
    CHECK(pixel_is(11, 11, true));
    CHECK(pixel_is(3, 3, false));
}

static void test_images(void)
{
    /* 8x8 空心菱形，按页存储。 */
    static const uint8_t image[8] = {0x81, 0x42, 0x24, 0x18,
                                     0x18, 0x24, 0x42, 0x81};
    int16_t row;
    int16_t column;

    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));
    CHECK(oled_show_image(&display, 0, 0, 8, 8, image) == OLED_OK);

    for (row = 0; row < 8; ++row) {
        for (column = 0; column < 8; ++column) {
            bool expected = ((image[column] >> row) & 1u) != 0u;
            CHECK(pixel_is(column, row, expected));
        }
    }

    /* 负坐标裁剪：屏幕 (0,0) 应对应图像 (3,3)。 */
    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));
    CHECK(oled_show_image(&display, -3, -3, 8, 8, image) == OLED_OK);
    CHECK(pixel_is(0, 0, ((image[3] >> 3) & 1u) != 0u));
    CHECK(pixel_is(4, 4, ((image[7] >> 7) & 1u) != 0u));

    CHECK(oled_show_image(&display, 0, 0, 8, 8, NULL) == OLED_ERR_PARAM);
}

static const uint8_t *ascii_glyph(char character, oled_font_t font,
                                  uint8_t *width, uint8_t *height)
{
    if (font == OLED_FONT_6X8) {
        *width = 6u;
        *height = 8u;
        return oled_font_6x8[(uint8_t)character - 0x20u];
    }
    *width = 8u;
    *height = 16u;
    return oled_font_8x16[(uint8_t)character - 0x20u];
}

static void check_ascii_at(int16_t x, int16_t y, char character,
                           oled_font_t font)
{
    uint8_t width;
    uint8_t height;
    const uint8_t *data = ascii_glyph(character, font, &width, &height);
    int16_t row;
    int16_t column;

    for (row = 0; row < (int16_t)height; ++row) {
        for (column = 0; column < (int16_t)width; ++column) {
            bool expected =
                ((data[((size_t)row >> 3) * width + (size_t)column] >>
                  (row & 7)) &
                 1u) != 0u;
            CHECK(pixel_is((int16_t)(x + column), (int16_t)(y + row),
                           expected));
        }
    }
}

static void test_ascii_text(void)
{
    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));

    CHECK(oled_show_char(&display, 10, 3, 'A', OLED_FONT_6X8) == OLED_OK);
    check_ascii_at(10, 3, 'A', OLED_FONT_6X8);

    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_show_text(&display, 0, 5, "AB", OLED_FONT_6X8) == OLED_OK);
    check_ascii_at(0, 5, 'A', OLED_FONT_6X8);
    check_ascii_at(6, 5, 'B', OLED_FONT_6X8);

    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_show_text(&display, 4, 2, "Z", OLED_FONT_8X16) == OLED_OK);
    check_ascii_at(4, 2, 'Z', OLED_FONT_8X16);

    /* 右边界裁剪不应越界。 */
    CHECK(oled_show_text(&display, 120, 30, "XYZ", OLED_FONT_8X16) ==
          OLED_OK);
}

static void test_cjk_text(void)
{
    static const char cjk_angle[] = "\xE8\xA7\x92"; /* “角” */
    const oled_cjk_glyph_t *glyph = NULL;
    const oled_cjk_glyph_t *item;
    int16_t row;
    int16_t column;

    for (item = oled_cjk_font; item->index[0] != '\0'; ++item) {
        if (strcmp(item->index, cjk_angle) == 0) {
            glyph = item;
            break;
        }
    }
    CHECK(glyph != NULL);
    if (glyph == NULL) {
        return;
    }

    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));
    CHECK(oled_show_text(&display, 0, 0, cjk_angle, OLED_FONT_8X16) ==
          OLED_OK);

    for (row = 0; row < 16; ++row) {
        for (column = 0; column < 16; ++column) {
            bool expected =
                ((glyph->data[((size_t)row >> 3) * 16u + (size_t)column] >>
                  (row & 7)) &
                 1u) != 0u;
            CHECK(pixel_is(column, row, expected));
        }
    }

    /* 6x8 字体下不支持汉字，应以 '?' 代替且不崩溃。 */
    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_show_text(&display, 0, 0, cjk_angle, OLED_FONT_6X8) ==
          OLED_OK);
}

static void expect_number(double number, oled_number_format_t format,
                          uint8_t length, oled_font_t font,
                          const char *expected)
{
    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_show_number(&display, 0, 0, number, format, length, font) ==
          OLED_OK);
    memcpy(reference_fb, framebuffer, sizeof(reference_fb));

    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_show_text(&display, 0, 0, expected, font) == OLED_OK);
    CHECK(memcmp(reference_fb, framebuffer, sizeof(framebuffer)) == 0);
}

static void test_numbers(void)
{
    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));

    expect_number(42.0, OLED_NUMBER_UNSIGNED, 2, OLED_FONT_6X8, "42");
    expect_number(42.0, OLED_NUMBER_UNSIGNED, 4, OLED_FONT_6X8, "0042");
    expect_number(-42.0, OLED_NUMBER_SIGNED, 2, OLED_FONT_6X8, "-42");
    expect_number(0x1A, OLED_NUMBER_HEX, 2, OLED_FONT_6X8, "1A");
    expect_number(5.0, OLED_NUMBER_BIN, 4, OLED_FONT_6X8, "0101");
    expect_number(1.5, OLED_NUMBER_FLOAT, 1, OLED_FONT_6X8, "1.5");
    expect_number(12.3456, OLED_NUMBER_FLOAT, 2, OLED_FONT_6X8, "12.346");
    expect_number(-1.25, OLED_NUMBER_FLOAT, 1, OLED_FONT_6X8, "-1.25");

    CHECK(oled_show_number(&display, 0, 0, 1.0, OLED_NUMBER_FLOAT, 0,
                           OLED_FONT_6X8) == OLED_ERR_PARAM);
}

static void test_printf(void)
{
    char long_text[18];
    int i;

    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));

    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_printf(&display, 0, 0, OLED_FONT_6X8, "%d", 123) == 3);
    memcpy(reference_fb, framebuffer, sizeof(framebuffer));
    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_show_text(&display, 0, 0, "123", OLED_FONT_6X8) == OLED_OK);
    CHECK(memcmp(reference_fb, framebuffer, sizeof(framebuffer)) == 0);

    /* 17 个 'A'，8x16 字体下应在第 17 个字符处换行。 */
    for (i = 0; i < 17; ++i) {
        long_text[i] = 'A';
    }
    long_text[17] = '\0';

    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_printf(&display, 0, 0, OLED_FONT_8X16, "%s", long_text) == 17);
    memcpy(reference_fb, framebuffer, sizeof(framebuffer));

    CHECK(oled_clear(&display) == OLED_OK);
    CHECK(oled_show_text(&display, 0, 0, "AAAAAAAAAAAAAAAA",
                         OLED_FONT_8X16) == OLED_OK);
    CHECK(oled_show_text(&display, 0, 16, "A", OLED_FONT_8X16) == OLED_OK);
    CHECK(memcmp(reference_fb, framebuffer, sizeof(framebuffer)) == 0);

    CHECK(oled_printf(&display, 0, 0, OLED_FONT_6X8, NULL) < 0);
}

static void test_display_area(void)
{
    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));

    fake.data_len = 0;
    fake.command_calls = 0;
    CHECK(oled_display_area(&display, 2, 2, 10, 10) == OLED_OK);
    CHECK(fake.command_calls == 2u); /* y=2..11 跨两页 */
    CHECK(fake.data_len == 20u);

    fake.data_len = 0;
    CHECK(oled_display_area(&display, -10, -10, 5, 5) == OLED_OK);
    CHECK(fake.data_len == 0u);

    fake.data_len = 0;
    CHECK(oled_display(&display) == OLED_OK);
    CHECK(fake.data_len == 1024u);
}

static void test_128x32(void)
{
    CHECK(setup_display(128u, 32u, true));
    CHECK(display.pages == 4u);

    fake.data_len = 0;
    CHECK(oled_display(&display) == OLED_OK);
    CHECK(fake.data_len == 512u);
}

static void test_io_errors(void)
{
    CHECK(setup_display(FB_WIDTH, FB_HEIGHT, true));

    fake.fail_command = true;
    CHECK(oled_set_contrast(&display, 0x10u) == OLED_ERR_IO);
    CHECK(oled_set_invert(&display, true) == OLED_ERR_IO);
    CHECK(oled_set_power(&display, false) == OLED_ERR_IO);

    fake.fail_command = false;

    fake.fail_data = true;
    CHECK(oled_display(&display) == OLED_ERR_IO);
}

static void test_controller_options(void)
{
    oled_config_t config;

    memset(framebuffer, 0, sizeof(framebuffer));
    fake_init();
    oled_config_default(&config);
    config.controller = OLED_CONTROLLER_SH1106;
    config.column_offset = 2u;
    config.mirror_x = true;
    config.mirror_y = true;
    config.invert = true;
    config.external_vcc = true;
    config.framebuffer = framebuffer;
    config.framebuffer_size = sizeof(framebuffer);

    CHECK(oled_init(&display, &config, &port) == OLED_OK);

    /* 初始化序列中应出现 SH1106 的段/COM 重映射与 DC-DC 命令。 */
    bool saw_a0 = false;
    bool saw_c0 = false;
    bool saw_ad = false;
    size_t i;

    for (i = 0; i + 1u < fake.command_len; ++i) {
        if (fake.commands[i] == 0xA0u) {
            saw_a0 = true;
        }
        if (fake.commands[i] == 0xC0u) {
            saw_c0 = true;
        }
        if (fake.commands[i] == 0xADu) {
            saw_ad = true;
        }
    }
    CHECK(saw_a0);
    CHECK(saw_c0);
    CHECK(saw_ad);
}

static void test_font_metrics_and_params(void)
{
    uint8_t width = 0u;
    uint8_t height = 0u;

    CHECK(oled_font_metrics(OLED_FONT_6X8, &width, &height) == OLED_OK);
    CHECK(width == 6u && height == 8u);
    CHECK(oled_font_metrics(OLED_FONT_8X16, &width, &height) == OLED_OK);
    CHECK(width == 8u && height == 16u);
    CHECK(oled_font_metrics(OLED_FONT_6X8, NULL, &height) == OLED_ERR_PARAM);
}

int main(void)
{
    test_config_default();
    test_init_sequence();
    test_init_validation();
    test_pixel_and_clipping();
    test_area_operations();
    test_lines_and_rect();
    test_images();
    test_ascii_text();
    test_cjk_text();
    test_numbers();
    test_printf();
    test_display_area();
    test_128x32();
    test_io_errors();
    test_controller_options();
    test_font_metrics_and_params();

    if (failures != 0) {
        printf("oled: %d check(s) failed\n", failures);
        return 1;
    }

    printf("oled: all tests passed\n");
    return 0;
}
