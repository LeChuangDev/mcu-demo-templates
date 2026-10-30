/**
 * @file  oled_spi_example.c
 * @brief STM32F4 + SSD1306 软件 SPI 集成示例。
 *
 * 这是一个“端口层”示例：oled 核心不碰任何寄存器，本项目用 GPIO 模拟
 * 4 线 SPI。换成硬件 SPI 时，只需替换 write_command/write_data 内部实现。
 */
#include "oled_spi_example.h"

#include "stm32f4xx.h"

#define OLED_CS_PIN 4u   /* PA4 */
#define OLED_CLK_PIN 5u  /* PA5 */
#define OLED_MOSI_PIN 7u /* PA7 */
#define OLED_RES_PIN 4u  /* PC4 */
#define OLED_DC_PIN 5u   /* PC5 */

static oled_t s_oled;
static uint8_t s_framebuffer[OLED_FRAMEBUFFER_SIZE(128u, 64u)];

static void oled_pin_set(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = pin;
}

static void oled_pin_reset(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = pin << 16;
}

static void oled_gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    __DSB();

    /* PA4/PA5/PA7、PC4/PC5 全部设为通用推挽输出。 */
    GPIOA->MODER &= ~((3u << (OLED_CS_PIN * 2u)) |
                      (3u << (OLED_CLK_PIN * 2u)) |
                      (3u << (OLED_MOSI_PIN * 2u)));
    GPIOA->MODER |= (1u << (OLED_CS_PIN * 2u)) |
                    (1u << (OLED_CLK_PIN * 2u)) |
                    (1u << (OLED_MOSI_PIN * 2u));
    GPIOA->OSPEEDR |= (3u << (OLED_CS_PIN * 2u)) |
                      (3u << (OLED_CLK_PIN * 2u)) |
                      (3u << (OLED_MOSI_PIN * 2u));

    GPIOC->MODER &= ~((3u << (OLED_RES_PIN * 2u)) |
                      (3u << (OLED_DC_PIN * 2u)));
    GPIOC->MODER |= (1u << (OLED_RES_PIN * 2u)) |
                    (1u << (OLED_DC_PIN * 2u));
    GPIOC->OSPEEDR |= (3u << (OLED_RES_PIN * 2u)) |
                      (3u << (OLED_DC_PIN * 2u));

    /* 空闲电平：CS=1、RES=1、DC=1、CLK=0。 */
    oled_pin_set(GPIOA, 1u << OLED_CS_PIN);
    oled_pin_set(GPIOC, (1u << OLED_RES_PIN) | (1u << OLED_DC_PIN));
    oled_pin_reset(GPIOA, 1u << OLED_CLK_PIN);
}

static void oled_spi_write_byte(uint8_t byte)
{
    int bit;

    for (bit = 7; bit >= 0; --bit) {
        if ((byte & (uint8_t)(1u << (unsigned)bit)) != 0u) {
            oled_pin_set(GPIOA, 1u << OLED_MOSI_PIN);
        } else {
            oled_pin_reset(GPIOA, 1u << OLED_MOSI_PIN);
        }
        oled_pin_set(GPIOA, 1u << OLED_CLK_PIN);
        __NOP();
        __NOP();
        oled_pin_reset(GPIOA, 1u << OLED_CLK_PIN);
    }
}

static bool oled_spi_write_command(void *user, const uint8_t *data,
                                   uint16_t len)
{
    uint16_t i;

    (void)user;
    if (len == 0u) {
        return true;
    }

    oled_pin_reset(GPIOA, 1u << OLED_CS_PIN);
    oled_pin_reset(GPIOC, 1u << OLED_DC_PIN);
    for (i = 0u; i < len; ++i) {
        oled_spi_write_byte(data[i]);
    }
    oled_pin_set(GPIOA, 1u << OLED_CS_PIN);
    return true;
}

static bool oled_spi_write_data(void *user, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    (void)user;
    if (len == 0u) {
        return true;
    }

    oled_pin_reset(GPIOA, 1u << OLED_CS_PIN);
    oled_pin_set(GPIOC, 1u << OLED_DC_PIN);
    for (i = 0u; i < len; ++i) {
        oled_spi_write_byte(data[i]);
    }
    oled_pin_set(GPIOA, 1u << OLED_CS_PIN);
    return true;
}

static void oled_spi_reset(void *user, bool asserted)
{
    (void)user;
    if (asserted) {
        oled_pin_reset(GPIOC, 1u << OLED_RES_PIN);
    } else {
        oled_pin_set(GPIOC, 1u << OLED_RES_PIN);
    }
}

/** 粗略毫秒延时，仅用于面板上电/复位时序；正式工程请换成 BSP 延时。 */
static void oled_spi_delay_ms(void *user, uint32_t ms)
{
    volatile uint32_t loops = (SystemCoreClock / 4000u) * ms;

    (void)user;
    while (loops != 0u) {
        __NOP();
        loops--;
    }
}

void oled_example_init(void)
{
    oled_config_t config;
    oled_port_t port;

    oled_gpio_init();

    oled_config_default(&config);
    config.framebuffer = s_framebuffer;
    config.framebuffer_size = (uint32_t)sizeof(s_framebuffer);

    port.write_command = oled_spi_write_command;
    port.write_data = oled_spi_write_data;
    port.reset = oled_spi_reset;
    port.delay_ms = oled_spi_delay_ms;
    port.user = NULL;

    if (oled_init(&s_oled, &config, &port) != OLED_OK) {
        return;
    }

    (void)oled_clear(&s_oled);
    (void)oled_show_text(&s_oled, 0, 0, "SSD1306", OLED_FONT_8X16);
    (void)oled_show_text(&s_oled, 0, 16, "OLED ready", OLED_FONT_8X16);
    (void)oled_show_number(&s_oled, 0, 40, 128.0, OLED_NUMBER_UNSIGNED, 3,
                           OLED_FONT_6X8);
    (void)oled_display(&s_oled);
}

oled_t *oled_example_device(void)
{
    return &s_oled;
}

oled_status_t oled_example_status(const char *text)
{
    oled_status_t status;

    if (text == NULL) {
        return OLED_ERR_PARAM;
    }

    status = oled_clear(&s_oled);
    if (status != OLED_OK) {
        return status;
    }
    status = oled_show_text(&s_oled, 0, 0, text, OLED_FONT_8X16);
    if (status != OLED_OK) {
        return status;
    }
    return oled_display(&s_oled);
}
