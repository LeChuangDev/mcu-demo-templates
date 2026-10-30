# SSD1306 / SH1106 OLED 驱动

平台无关的单色 OLED 驱动。核心负责显存、绘图、字模渲染和控制器命令，所有总线访问通过 `oled_port_t` 回调注入，因此同一份驱动可用于软件 SPI、硬件 SPI、I2C 或并口。

## 特性

- **实例化 + 依赖注入**：`oled_t` 由调用者静态分配，总线操作由 port 回调提供，不依赖任何 MCU 头文件。
- **零动态内存**：显存由调用者提供，大小由 `OLED_FRAMEBUFFER_SIZE(width, height)` 计算。
- **可选局部刷新**：`oled_display_area()` 只刷指定矩形所在的页，适合动画和低带宽场景。
- **几何可配置**：支持 128x64、128x32 等高度为 8 的倍数的面板，支持 X/Y 镜像、反色、对比度。
- **双控制器**：SSD1306 与 SH1106（含列偏移，通常为 2）。
- **完整绘图 API**：像素、线、矩形、位图、ASCII/UTF-8 文本、定点数字和 printf 风格格式化。
- **可裁剪**：字体和文本/数字/printf 功能都有独立编译开关。

## 目录结构

```
Oled_Spi/
├── Inc/
│   ├── oled.h              # 公开 API、port、配置、实例
│   ├── oled_config.h       # 字体与功能裁剪
│   └── oled_font.h         # 字模声明
├── Src/
│   ├── oled.c              # 驱动核心
│   └── oled_font.c         # ASCII / 16x16 汉字字模
├── Tests/
│   ├── test_oled.c         # 主机侧单元测试（假 port）
│   └── CMakeLists.txt
├── Examples/stm32f4/
│   ├── oled_spi_example.h  # 软件 SPI + GPIO 示例
│   └── oled_spi_example.c
└── CMakeLists.txt
```

## 快速开始

### 独立编译与测试（主机）

```bash
cmake -S Oled_Spi -B Oled_Spi/build-test -G Ninja
cmake --build Oled_Spi/build-test
ctest --test-dir Oled_Spi/build-test --output-on-failure
```

### 集成到 STM32 工程

```cmake
add_subdirectory(path/to/Oled_Spi)
target_link_libraries(bsp PUBLIC oled)
```

也可以只把 `Src/oled.c`、`Src/oled_font.c` 和 `Inc/` 加入现有工程。核心只依赖 C11 标准库；启用 `oled_printf` 时额外依赖 `vsnprintf`。

## 基本用法

```c
#include "oled.h"

static oled_t g_oled;
static uint8_t g_fb[OLED_FRAMEBUFFER_SIZE(128, 64)];

static bool my_write_command(void *user, const uint8_t *data, uint16_t len);
static bool my_write_data(void *user, const uint8_t *data, uint16_t len);
static void my_reset(void *user, bool asserted);
static void my_delay_ms(void *user, uint32_t ms);

void display_init(void)
{
    oled_config_t config;
    oled_port_t port = {
        .write_command = my_write_command,
        .write_data = my_write_data,
        .reset = my_reset,
        .delay_ms = my_delay_ms,
        .user = NULL,
    };

    oled_config_default(&config);
    config.framebuffer = g_fb;
    config.framebuffer_size = sizeof(g_fb);

    if (oled_init(&g_oled, &config, &port) != OLED_OK) {
        /* 处理错误 */
    }
}

void display_task(void)
{
    oled_clear(&g_oled);
    oled_show_text(&g_oled, 0, 0, "Hello", OLED_FONT_8X16);
    oled_show_number(&g_oled, 0, 16, 3.14, OLED_NUMBER_FLOAT, 2,
                     OLED_FONT_6X8);
    oled_display(&g_oled);          /* 只有这一步才真正刷屏 */
}
```

### Port 约定

| 回调 | 要求 |
|---|---|
| `write_command(user, data, len)` | 一次完整事务：拉低 CS、DC=0、发送 `len` 字节、释放 CS；成功返回 `true` |
| `write_data(user, data, len)` | 同上，DC=1 |
| `reset(user, asserted)` | `asserted == true` 时拉低 RES#；可为 `NULL` |
| `delay_ms(user, ms)` | 初始化复位时序；可为 `NULL` |

`len == 0` 时 port 应直接返回 `true`。回调内部不要改变 DC/CS 的最终状态假设，驱动会在每次事务开始时重新设置。

### API 一览

| 分组 | 函数 |
|---|---|
| 生命周期 | `oled_config_default`、`oled_init` |
| 刷屏 | `oled_display`、`oled_display_area` |
| 面板 | `oled_set_contrast`、`oled_set_invert`、`oled_set_power` |
| 显存 | `oled_clear`、`oled_clear_area`、`oled_reverse`、`oled_reverse_area` |
| 图元 | `oled_draw_pixel`、`oled_get_pixel`、`oled_draw_hline`、`oled_draw_vline`、`oled_draw_rect`、`oled_fill_rect` |
| 文本 | `oled_font_metrics`、`oled_show_char`、`oled_show_text`、`oled_printf` |
| 数字/图像 | `oled_show_number`、`oled_show_image` |

所有函数返回 `oled_status_t`（`oled_printf` 返回绘制字符数或负状态码）：

- `OLED_OK`
- `OLED_ERR_PARAM`：参数非法或实例未初始化
- `OLED_ERR_IO`：port 报告总线失败
- `OLED_ERR_UNSUPPORTED`：功能/字体被编译期关闭
- `OLED_ERR_BUFFER`：显存不足

## 显存与坐标

- 显存按页存储：每页 8 行，每页 `width` 字节，字节 bit0 是页内最上面一行。
- 坐标原点在左上角，X 向右、Y 向下；所有绘制自动裁剪到面板范围。
- 绘制只改显存，**不会自动刷屏**；调用 `oled_display()` 或 `oled_display_area()` 才写面板。
- `oled_display_area()` 的 Y 方向按页对齐：传入 `y=3,height=2` 会刷新包含这两行的整个页。X 方向按列精确裁剪。
- 显存完全由调用者管理，可放在 CCM、DMA 缓冲或外部 RAM 中；驱动不假设其位置。

## 面板配置

```c
oled_config_t config;
oled_config_default(&config);

config.width  = 128;                 /* 1..128 */
config.height = 64;                  /* 8..64，且为 8 的倍数 */
config.controller = OLED_CONTROLLER_SH1106;
config.column_offset = 2;            /* SH1106 常见值；SSD1306 用 0 */
config.mirror_x = false;
config.mirror_y = false;
config.invert = false;
config.external_vcc = false;         /* false = 使用内部电荷泵 */
config.contrast = OLED_DEFAULT_CONTRAST;
config.framebuffer = g_fb;
config.framebuffer_size = sizeof(g_fb);
```

- `mirror_x` / `mirror_y` 会改变段重映射和 COM 扫描方向。
- `external_vcc = true` 时关闭内部电荷泵，适用于模块自带升压的板子。
- 128x32 面板会自动选择对应的 COM 配置。

## 字体与文本

- 内置 6x8 和 8x16 ASCII 字模；`oled_show_text()` 接受 UTF-8。
- 16x16 汉字从 `Src/oled_font.c` 的 `oled_cjk_font[]` 查找；未收录字符显示为 `?`。
- 文本默认不换行，超出右边界的部分被裁剪；`oled_printf()` 会自动换行并处理 `\n`。
- `oled_printf()` 使用固定大小栈缓冲区（`OLED_PRINTF_BUFFER_SIZE`），超长内容安全截断。

### 编译期裁剪

| 宏 | 默认 | 作用 |
|---|---|---|
| `OLED_ENABLE_FONT_6X8` | 1 | 6x8 ASCII 字模 |
| `OLED_ENABLE_FONT_8X16` | 1 | 8x16 ASCII 字模 |
| `OLED_ENABLE_CJK` | 1 | 16x16 汉字字模 |
| `OLED_ENABLE_TEXT` | 1 | `oled_show_char/text` |
| `OLED_ENABLE_NUMBER` | 1 | `oled_show_number` |
| `OLED_ENABLE_PRINTF` | 1 | `oled_printf`（依赖 vsnprintf） |
| `OLED_PRINTF_BUFFER_SIZE` | 96 | printf 缓冲区字节数 |
| `OLED_DEFAULT_CONTRAST` | `0xCF` | 默认对比度 |

关闭字体会让对应表不参与链接，明显减小 Flash。关闭文本/数字功能后相关 API 仍存在，但返回 `OLED_ERR_UNSUPPORTED`。

## 移植

`Examples/stm32f4/` 提供与旧版引脚一致的软件 SPI 示例：

| 信号 | 引脚 |
|---|---|
| CS | PA4 |
| SCK | PA5 |
| MOSI | PA7 |
| RES | PC4 |
| DC | PC5 |

换成硬件 SPI 时：

1. 在 port 的 `write_command` 中拉低 DC，调用 HAL/LL/寄存器发送，等待完成。
2. 在 `write_data` 中拉高 DC，同样发送。
3. CS 可由硬件 NSS 或软件控制；只要保证每次事务原子即可。
4. `reset` 和 `delay_ms` 直接接板级实现。

若使用 I2C 屏，把两个写回调实现为 I2C 传输，命令/数据通过控制字节区分即可；驱动核心无需修改。

## 并发约定

驱动内部没有锁。请保证同一实例的绘制、刷屏不会在中断和主循环中并发；典型做法是主循环绘制，刷屏也在主循环完成。若必须在中断中刷屏，请由应用层加临界区。

## 从旧 API 迁移

旧文件 `Oled.c/.h`、`Oled_data.c/.h` 已移除，对应关系：

| 旧接口 | 新接口 |
|---|---|
| `OLED_Init()` | `oled_init(&dev, &config, &port)`，显存由调用者提供 |
| `OLED_Update()` | `oled_display(&dev)` |
| `OLED_UpdateArea(x,y,w,h)` | `oled_display_area(&dev,x,y,w,h)` |
| `OLED_Clear()` / `OLED_ClearArea()` | `oled_clear()` / `oled_clear_area()` |
| `OLED_Reverse()` / `OLED_ReverseArea()` | `oled_reverse()` / `oled_reverse_area()` |
| `OLED_SetContrast()` | `oled_set_contrast()` |
| `OLED_ShowChar(x,y,ch,size)` | `oled_show_char(&dev,x,y,ch,font)` |
| `OLED_ShowText(x,y,text,size)` | `oled_show_text(&dev,x,y,text,font)` |
| `OLED_ShowImage()` | `oled_show_image(&dev,x,y,w,h,image)` |
| `OLED_ShowNumber(..., type, len, size)` | `oled_show_number(&dev,...,format,len,font)` |
| `OLED_Printf()` | `oled_printf(&dev,...)` |
| `OLED_6X8` / `OLED_8X16` | `OLED_FONT_6X8` / `OLED_FONT_8X16` |
| `OLED_DEC_UNSIGNED/SIGNED/HEX/BIN/FLOAT` | `OLED_NUMBER_*` |
| 全局 `OLED_DisplayBuf` | 调用者提供的 `config.framebuffer` |
| 全局 GPIO 宏 | `oled_port_t` 回调 |

旧版的 `Diode[]` 演示图片未保留；需要时用 `oled_show_image()` 传入自己的位图数据。

## 测试

`Tests/test_oled.c` 使用假 port 捕获命令/数据流，覆盖：

- 初始化序列、复位时序、参数校验；
- 像素、裁剪、区域清屏/反色/局部刷新；
- 线、矩形、位图（含负坐标）；
- ASCII 字模逐像素校验、UTF-8 汉字查找；
- 数字各格式与 `oled_printf` 换行；
- 128x32、SH1106、镜像、反色、外部 VCC；
- 总线失败与字体裁剪错误。

主机测试以 `-Wall -Wextra -Werror` 编译，可在 CI 中直接运行。
