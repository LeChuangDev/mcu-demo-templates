# Zigbee / 无线串口通用帧协议框架

面向 Zigbee 透传模块的可配置串口链路层。不同厂商的帧头、长度字段、校验算法和转义规则各不相同，本模块把这些差异收敛到一份 `zb_config_t` 中，业务层只需要处理 `on_frame` 回调里的负载。

## 特性

- **格式可配置**：SOP/EOP 1~2 字节、长度字段 1~2 字节（大端）、校验 0/1/2 字节。
- **多种校验**：SUM8、XOR8、CRC8（poly 0x07）、CRC16-CCITT（poly 0x1021，初值 0xFFFF），可选覆盖帧头。
- **可选转义**：ESC + XOR，自动转义长度、负载、校验中的 SOP/EOP/ESC 字节，SOP/EOP 本身保持原样。
- **RAW 透传模式**：完全不分帧，适合调试或厂商模块本身就是裸串口。
- **流式解析**：逐字节状态机，任意位置插入噪声后自动重新同步。
- **实例化**：无全局状态、无动态内存；`zb_link_t` 由调用者静态分配。
- **可诊断**：统计成功帧、校验错误、长度溢出、帧尾错误、丢弃字节和 RAW 块数。
- **可测试**：主机侧测试覆盖组帧、回环、转义、CRC、异常恢复和 RAW 模式。

## 目录结构

```
Zigbee/
├── Inc/
│   ├── zigbee.h            # 公开 API、配置、实例、回调
│   └── zigbee_config.h     # 容量与功能开关
├── Src/
│   └── zigbee.c            # 解析状态机、组帧、校验
├── Tests/
│   ├── test_zigbee.c       # 主机侧单元测试
│   └── CMakeLists.txt
├── Examples/stm32f4/
│   ├── zigbee_uart_example.h
│   └── zigbee_uart_example.c
└── CMakeLists.txt
```

## 帧格式

```
[SOP 1~2B] [LEN 1~2B, 大端] [PAYLOAD 0~ZB_MAX_PAYLOAD] [CHECKSUM 0~2B] [EOP 0~2B]
```

默认配置（`zb_config_default()`）：

```
0xFA | LEN | PAYLOAD | SUM8 | 0xFB
```

其中 SUM8 覆盖 `[LEN][PAYLOAD]`（默认不覆盖 SOP，也不覆盖 EOP）。

### 校验覆盖范围

- 默认：`LEN` 字节 + `PAYLOAD`；
- `checksum_covers_header = true`：额外包含 `SOP`；
- `EOP` 永远不参与校验；
- `CHECKSUM_NONE` 时长度字段后直接是 EOP。

### 转义规则

开启 `escape_enabled` 后，仅对 `[LEN][PAYLOAD][CHECKSUM]` 转义：

- 需要转义的字节：`escape_byte`、任意 SOP 字节、任意 EOP 字节；
- 输出两个字节：`escape_byte`、`original ^ escape_xor`；
- SOP/EOP 原样发送，接收端在对应状态直接匹配。

这样即使负载里出现帧头/帧尾，也不会破坏分帧。若模块协议不使用转义，保持关闭即可；解析器按长度字段消费负载，负载中的 SOP/EOP 不会干扰正常帧。

## 快速开始

### 独立编译与测试（主机）

```bash
cmake -S Zigbee -B Zigbee/build-test -G Ninja
cmake --build Zigbee/build-test
ctest --test-dir Zigbee/build-test --output-on-failure
```

### 集成到 STM32 工程

```cmake
add_subdirectory(path/to/Zigbee)
target_link_libraries(bsp PUBLIC zigbee)
```

或只加入 `Src/zigbee.c` 和 `Inc/`。核心只依赖 C11 标准库的 `<stdint.h>/<stdbool.h>/<stddef.h>/<string.h>`。

## 基本用法

```c
#include "zigbee.h"

static zb_link_t g_link;

static void on_frame(void *user, const uint8_t *payload, size_t len)
{
    /* 在 feed 上下文（通常是 UART 中断）中执行，保持短小 */
    (void)user;
    /* 解析业务字段：地址、端口、命令字…… */
}

static void on_error(void *user, zb_error_t error)
{
    (void)user;
    (void)error;
}

/* UART 中断 */
void USART2_IRQHandler(void)
{
    if ((USART2->SR & USART_SR_RXNE) != 0u) {
        uint8_t byte = (uint8_t)USART2->DR;
        (void)zb_feed_byte(&g_link, byte);
    }
}

static bool uart_write(void *user, const uint8_t *data, size_t len)
{
    (void)user;
    /* 把 data[0..len) 写入 UART；成功返回 true */
    return true;
}

void link_init(void)
{
    zb_config_t config;
    zb_port_t port = { .write = uart_write, .user = NULL };

    zb_config_default(&config);
    config.on_frame = on_frame;
    config.on_error = on_error;

    if (zb_init(&g_link, &config, &port) != ZB_OK) {
        /* 处理错误 */
    }
}

void link_send_example(void)
{
    const uint8_t payload[] = {0x01, 0x02, 0x03};
    (void)zb_send(&g_link, payload, sizeof(payload));
}
```

### API 一览

| 函数 | 说明 |
|---|---|
| `zb_config_default(cfg)` | 填充默认配置 |
| `zb_init(link, cfg, port)` | 初始化；`port` 可为 `NULL`（只收不发） |
| `zb_reset(link)` | 清空解析状态与统计，保留配置和端口 |
| `zb_feed_byte(link, byte)` | 送入 1 字节 |
| `zb_feed(link, data, len)` | 送入一段字节 |
| `zb_send(link, payload, len)` | 按配置组帧并调用 `port.write` |
| `zb_get_stats(link, out)` | 读取统计 |

状态码：`ZB_OK`、`ZB_ERR_PARAM`、`ZB_ERR_NO_SPACE`、`ZB_ERR_IO`、`ZB_ERR_STATE`。

## 配置示例

### 默认：0xFA / SUM8 / 0xFB

```c
zb_config_t config;
zb_config_default(&config);
```

### CRC16-CCITT + 覆盖帧头 + 双字节长度 + 双字节帧尾

```c
zb_config_t config;
zb_config_default(&config);

config.length_bytes = 2;
config.checksum = ZB_CHECKSUM_CRC16_CCITT;
config.checksum_covers_header = true;
config.eop[0] = 0x0D;
config.eop[1] = 0x0A;
config.eop_len = 2;
```

发送顺序为：

```
SOP | LEN_H LEN_L | PAYLOAD | CRC_H CRC_L | EOP0 EOP1
```

CRC16 为大端输出，覆盖 `SOP + LEN + PAYLOAD`。

### 带转义

```c
config.escape_enabled = true;
config.escape_byte = 0x7D;
config.escape_xor = 0x20;
```

负载中的 `0xFA`、`0xFB`、`0x7D` 会被转义；接收端自动还原。

### 纯透传

```c
config.mode = ZB_MODE_RAW;
config.on_raw = my_raw_handler;
```

RAW 模式下：

- `zb_feed()` 把整段数据原样交给 `on_raw`，不解析；
- `zb_send()` 原样写出负载，不加帧头/校验；
- `zb_feed_byte()` 每次回调 1 字节。

## 移植与并发

1. 在 BSP 中初始化 UART（8N1，波特率按模块手册）。
2. RX 中断/DMA 回调中调用 `zb_feed_byte()` 或 `zb_feed()`。
3. `port.write` 中实现阻塞或 DMA 发送；`zb_send()` 可能阻塞，**不要在中断里调用**。
4. 同一实例的 `feed` 只能有一个写入者；`on_frame/on_error/on_raw` 在 feed 上下文同步执行，不能阻塞。
5. 如果主循环和中断都会访问回调结果，请由应用层加临界区（示例中演示了 PRIMASK 保存/恢复）。

`Examples/stm32f4/` 提供 USART2（PA2/PA3）的完整示例，包含接收中断、发送端口和主循环取帧。

## 统计与排障

```c
zb_stats_t stats;
zb_get_stats(&link, &stats);
```

| 字段 | 含义 |
|---|---|
| `frames_ok` | 成功校验并通过回调的帧数 |
| `checksum_errors` | 校验失败的帧数 |
| `overflow_errors` | 长度字段超过 `ZB_MAX_PAYLOAD` 的次数 |
| `framing_errors` | 帧尾不匹配的次数 |
| `discarded_bytes` | 寻找 SOP 时丢弃的字节数 |
| `raw_chunks` | RAW 模式收到的数据块数 |

常见现象：

- `discarded_bytes` 持续增长：波特率/接线错误，或 SOP 配置与模块不符；
- `checksum_errors` 增长：校验算法/覆盖范围配置错误，或线路干扰；
- `overflow_errors` 增长：长度字段宽度或字节序配置错误；
- `framing_errors` 增长：EOP 配置错误，或发送端未按协议组帧。

## 容量与裁剪

`Inc/zigbee_config.h`：

| 宏 | 默认 | 说明 |
|---|---|---|
| `ZB_MAX_PAYLOAD` | 128 | 单帧最大负载；同时决定实例内 RX/TX 缓冲大小 |
| `ZB_ENABLE_ESCAPE` | 1 | 转义逻辑 |
| `ZB_ENABLE_CHECKSUM` | 1 | 校验逻辑（关闭后只允许 `CHECKSUM_NONE`） |
| `ZB_ENABLE_CRC16` | 1 | CRC16 实现（关闭后不允许选择 CRC16） |
| `ZB_ENABLE_RAW_MODE` | 1 | RAW 模式（关闭后不允许 `ZB_MODE_RAW`） |

`ZB_MAX_FRAME_SIZE` 按最坏情况（长度、负载、校验全部被转义）计算，发送缓冲固定为这个大小。

## 从旧 API 迁移

旧 `Zigbee.c/.h` 是空占位文件，没有可迁移的接口。新框架的接入方式：

1. 按模块手册填写 `zb_config_t`（SOP/EOP、长度宽度、校验、转义）；
2. 在 UART 中断中喂字节；
3. 在 `on_frame` 中解析业务字段；
4. 用 `zb_send()` 组帧发送。

如果模块是纯透传，直接使用 `ZB_MODE_RAW`，无需自己拼帧。

## 测试

`Tests/test_zigbee.c` 覆盖：

- 默认帧与 SUM8 黄金字节；
- 发送→解析回环、逐字节喂入、批量多帧；
- 校验覆盖帧头、XOR8、CRC8、CRC16-CCITT；
- 转义（含负载和长度字段中的特殊字节）；
- 校验失败、长度溢出、EOP 错误后的恢复；
- 噪声/多字节 SOP 重同步；
- 零负载、无 EOP、无校验、RAW 模式；
- 参数校验、无端口发送、发送缓冲不足、底层写失败。

主机测试以 `-Wall -Wextra -Werror` 编译，可直接在 CI 中运行。
