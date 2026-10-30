# JY61P / WitMotion 姿态传感器驱动

平台无关的 JY61P（WitMotion 0x55 协议）串口驱动。核心只做**流式解析、量程换算和数据发布**，不依赖任何 MCU 厂商头文件，也不操作 UART/GPIO。

## 特性

- **实例化**：每个 `jy61_t` 独立，可同时接多路传感器；无全局状态。
- **零动态内存**：实例与接收缓冲都由调用者静态分配。
- **流式解析**：任意位置插入噪声后可自动重新同步。
- **逐字节接口**：天然适配 UART 中断、DMA 完成回调或轮询。
- **数据快照**：`jy61_get_data()` 返回一致性副本；可选临界区回调，未配置时使用序号检查（seqlock）做无锁重试。
- **可裁剪**：校验、默认量程换算均可配置。
- **可测试**：主机侧单元测试覆盖协议、噪声、校验、并发回调和参数校验。

## 目录结构

```
Jy61_P/
├── Inc/
│   ├── jy61.h              # 公开 API、类型、实例
│   └── jy61_config.h       # 编译期默认量程与校验开关
├── Src/
│   └── jy61.c              # 平台无关实现
├── Tests/
│   ├── test_jy61.c         # 主机侧单元测试
│   └── CMakeLists.txt
├── Examples/stm32f4/
│   ├── jy61_uart_example.h # USART3 + 中断集成示例
│   └── jy61_uart_example.c
└── CMakeLists.txt
```

## 快速开始

### 独立编译与测试（主机）

需要 CMake ≥ 3.20、Ninja 和主机 C 编译器：

```bash
cmake -S Jy61_P -B Jy61_P/build-test -G Ninja
cmake --build Jy61_P/build-test
ctest --test-dir Jy61_P/build-test --output-on-failure
```

### 集成到 STM32 工程

方式一：作为 CMake 子目录

```cmake
add_subdirectory(path/to/Jy61_P)
target_link_libraries(bsp PUBLIC jy61)
```

方式二：只复制源码

把 `Inc/` 加入头文件路径，把 `Src/jy61.c` 加入源文件列表即可。驱动只依赖 C11 标准库的 `<stdint.h>/<stdbool.h>/<stddef.h>/<string.h>`。

方式三：沿用模板的 `stm32_add_lib`

把整个模块目录放到 `Drivers/External/jy61/`，在该目录的 CMakeLists 中调用 `stm32_add_lib(jy61)`（函数会自动收集 `Src/*.c` 并导出 `Inc/`）。

## 基本用法

```c
#include "jy61.h"

static jy61_t g_imu;

/* UART 中断：只喂字节，不做浮点运算 */
void USART3_IRQHandler(void)
{
    if ((USART3->SR & USART_SR_RXNE) != 0u) {
        uint8_t byte = (uint8_t)USART3->DR;
        (void)jy61_feed_byte(&g_imu, byte);
    }
}

void imu_init(void)
{
    jy61_config_t config;
    jy61_config_default(&config);

    /* 可选：提供临界区，主循环读取时与中断互斥 */
    config.critical.enter = my_critical_enter;
    config.critical.exit  = my_critical_exit;

    /* 可选：每解析出一包就回调（在中断上下文执行） */
    config.on_packet = my_on_packet;
    config.user = &my_context;

    if (jy61_init(&g_imu, &config) != JY61_OK) {
        /* 处理错误 */
    }
}

void imu_task(void)
{
    jy61_data_t data;

    if (jy61_get_data(&g_imu, &data) == JY61_OK) {
        /* data.accel_g / data.gyro_dps / data.angle_deg */
    }
}
```

### API 一览

| 函数 | 说明 |
|---|---|
| `jy61_config_default(cfg)` | 填充默认配置（±16 g / ±2000 °/s / ±180°、校验开启） |
| `jy61_init(ctx, cfg)` | 初始化实例；`cfg == NULL` 等价于默认配置 |
| `jy61_reset(ctx)` | 清空解析状态与统计，保留配置 |
| `jy61_feed_byte(ctx, byte)` | 送入 1 字节，返回本次是否解出一包（0/1） |
| `jy61_feed(ctx, data, len)` | 送入一段字节，返回解出的包数 |
| `jy61_get_data(ctx, out)` | 读取最新快照；未收到有效包返回 `JY61_ERR_NO_DATA` |
| `jy61_get_stats(ctx, out)` | 读取收包数、校验错误、丢弃字节数 |

状态码：`JY61_OK`、`JY61_ERR_PARAM`、`JY61_ERR_NO_DATA`、`JY61_ERR_BUSY`。

## 协议与数据

每帧固定 11 字节：

```
[0]      0x55
[1]      0x51 加速度 / 0x52 角速度 / 0x53 角度
[2..7]   X/Y/Z 三个 int16（小端）
[8..9]   温度/时间戳（原样保存到 jy61_data_t.timestamp）
[10]     前 10 字节累加和的低 8 位
```

默认换算系数：

```c
加速度: 16.0f   / 32768.0f   /* g   */
角速度: 2000.0f / 32768.0f   /* °/s */
角度:   180.0f  / 32768.0f   /* °   */
```

若传感器量程被上位机修改，初始化前直接覆盖 `config.accel_g_per_lsb` 等字段即可，无需改驱动源码。`jy61_config.h` 中的宏只提供默认值。

## 移植与并发

1. 在 BSP 中初始化 UART（波特率通常 9600，8N1）。
2. 在 RX 中断/DMA 回调中调用 `jy61_feed_byte()` 或 `jy61_feed()`。
3. 主循环调用 `jy61_get_data()`；若中断可能打断读取，配置 `critical.enter/exit`。
4. 中断里不要调用 `jy61_get_data()`（无意义且可能破坏快照一致性）。

同一实例的 `feed` 只能有一个写入者（通常就是该 UART 的中断）。多个实例之间互不影响。

`Examples/stm32f4/` 提供了 STM32F4 USART3（PB10/PB11）的完整示例，包含 PRIMASK 保存/恢复的临界区实现。

## 编译期配置

`Inc/jy61_config.h`：

| 宏 | 默认 | 说明 |
|---|---|---|
| `JY61_DEFAULT_ACCEL_G_PER_LSB` | `16/32768` | 加速度默认换算 |
| `JY61_DEFAULT_GYRO_DPS_PER_LSB` | `2000/32768` | 角速度默认换算 |
| `JY61_DEFAULT_ANGLE_DEG_PER_LSB` | `180/32768` | 角度默认换算 |
| `JY61_FEATURE_CHECKSUM` | `1` | 初始化时默认是否开启 SUM 校验 |

所有宏都可以用 `-D` 覆盖。

## 从旧 API 迁移

旧文件 `Jy61_P.c/.h` 已移除，对应关系如下：

| 旧接口 | 新接口 |
|---|---|
| `Jy61_Init(baud)` | BSP 初始化 UART + `jy61_init()` |
| `Get_Jy61(Data)` | `jy61_feed_byte(&ctx, byte)` |
| 全局 `SAngle` / `SGyro` / `SAcc` | `jy61_get_data(&ctx, &data)` |
| 旧结构体中的 `*_S[]` 原始值 | 由 `data.accel_g` 等浮点值代替；需要原始 int16 时可在应用层按换算系数反算 |
| `USART3_IRQHandler` 内解析 | 中断只调用 `jy61_feed_byte()` |

旧实现使用 `memcpy` 直接把字节拷进结构体，依赖小端和结构体布局；新实现显式按小端解析，跨平台一致。

## 测试

`Tests/test_jy61.c` 覆盖：

- 默认配置与三种数据包的量程换算；
- 噪声插入后的重同步；
- 校验失败、恢复与关闭校验；
- 回调、临界区、批量喂入、复位；
- 参数校验与未初始化实例。

测试使用主机 gcc 并以 `-Wall -Wextra -Werror` 编译，可直接在 CI 中运行。
