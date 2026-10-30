# 通用外设模块库

面向 MCU 工程的可复用外设模块集合。每个模块都是**平台无关的核心 + 可注入的端口层**，可以直接接入 STM32F1/F4/G4/H7、GD32、AT32 等工程，也可以在主机上编译运行单元测试。

| 模块 | 目标 | 核心能力 |
|---|---|---|
| [Jy61_P](Jy61_P/) | JY61P / WitMotion 姿态传感器 | 0x55 协议流式解析、量程换算、线程安全快照 |
| [Oled_Spi](Oled_Spi/) | SSD1306 / SH1106 OLED | 显存、图元、ASCII/UTF-8 文本、局部刷新 |
| [Zigbee](Zigbee/) | Zigbee 串口透传模块 | 可配置帧格式、校验、转义、RAW 透传 |

## 设计原则

所有模块遵循同一套企业级嵌入式约定：

1. **平台无关核心**
   - 不包含任何厂商头文件，不直接操作 GPIO/UART/SPI 寄存器。
   - 硬件差异通过端口回调（`*_port_t`）注入，核心可在主机上编译和测试。

2. **实例化，无全局状态**
   - 每个设备一个上下文（`jy61_t`、`oled_t`、`zb_link_t`），由调用者静态分配。
   - 同一工程可同时使用多路相同外设，互不干扰。

3. **零动态内存、零隐式阻塞**
   - 不使用 `malloc/free`；缓冲区在编译期确定或由调用者提供。
   - 中断回调只做轻量解析，不调用 printf、不延时、不刷屏。

4. **显式状态码**
   - 所有可能失败的 API 返回 `*_status_t`，不依赖 errno 或全局错误变量。
   - 参数校验在入口完成，未初始化实例返回明确的 `*_ERR_STATE`。

5. **可裁剪**
   - 字体、校验、转义、RAW 模式等功能通过 `*_config.h` 宏裁剪，兼顾 Flash 和功能。

6. **主机可测试**
   - 每个模块带独立单元测试，使用假端口验证协议和边界条件，不依赖目标板。
   - 测试以 `-Wall -Wextra -Werror` 编译，可作为 CI 门禁。

## 目录约定

```
Module/
├── CMakeLists.txt
├── README.md
├── Jy61_P/
│   ├── Inc/               # 公开头文件（API、配置）
│   ├── Src/               # 平台无关实现
│   ├── Tests/             # 主机侧单元测试
│   ├── Examples/stm32f4/  # 目标板集成示例（不参与库编译）
│   ├── CMakeLists.txt
│   └── README.md
├── Oled_Spi/
└── Zigbee/
```

每个模块的 `CMakeLists.txt` 都支持两种用法：

- **独立构建**：`cmake -S <module> -B <module>/build-test`，自动启用测试；
- **作为子目录**：`add_subdirectory(<module>)`，只创建库目标，测试由根工程的 `MODULE_BUILD_TESTS` 控制。

## 构建与测试

要求：CMake ≥ 3.20、Ninja、C11 编译器（GCC/Clang/MinGW）。

```bash
# 在 Module/ 目录
cmake -S . -B build-test -G Ninja
cmake --build build-test
ctest --test-dir build-test --output-on-failure
```

也可以单独构建某个模块：

```bash
cmake -S Jy61_P -B Jy61_P/build-test -G Ninja
cmake --build Jy61_P/build-test
ctest --test-dir Jy61_P/build-test --output-on-failure
```

关闭测试（仅编译库）：

```bash
cmake -S . -B build -G Ninja -DMODULE_BUILD_TESTS=OFF
cmake --build build
```

## 集成到 MCU 工程

### 方式一：CMake 子目录（推荐）

```cmake
add_subdirectory(path/to/Module/Jy61_P)
add_subdirectory(path/to/Module/Oled_Spi)
add_subdirectory(path/to/Module/Zigbee)

target_link_libraries(bsp PUBLIC jy61 oled zigbee)
```

模块会导出各自的 `Inc/`，使用方无需手写 include 路径。

### 方式二：源码拷贝

把模块的 `Inc/` 加入头文件搜索路径，把 `Src/*.c` 加入工程源文件列表。核心只依赖 C11 标准库：

- JY61：`stdint.h`、`stdbool.h`、`stddef.h`、`string.h`
- OLED：同上；启用 `OLED_ENABLE_PRINTF` 时额外依赖 `vsnprintf`
- Zigbee：同上

### 方式三：沿用模板的 `stm32_add_lib`

把模块目录放到 `Drivers/External/<name>/`，在模块目录的 CMakeLists 中调用：

```cmake
stm32_add_lib(jy61)
```

函数会自动收集 `Src/*.c` 并把 `Inc/` 导出为公共头文件路径。

## 目标板示例

每个模块的 `Examples/stm32f4/` 提供一份 STM32F4 集成示例，展示端口回调、UART 中断和主循环协作方式：

- `Jy61_P/Examples/stm32f4/jy61_uart_example.c`：USART3 + 接收中断 + 临界区快照；
- `Oled_Spi/Examples/stm32f4/oled_spi_example.c`：GPIO 软件 SPI + 显存 + 刷屏；
- `Zigbee/Examples/stm32f4/zigbee_uart_example.c`：USART2 + 帧回调 + 主循环取帧。

示例不参与库编译，复制到目标工程后按实际引脚/时钟修改即可。

## 从旧代码迁移

旧版 `Jy61_P/Jy61_P.c/.h`、`Oled_Spi/Oled.c/.h`、`Oled_Spi/Oled_data.c/.h` 以及空的 `Zigbee/Zigbee.c/.h` 已被新实现取代并移除。每个模块的 README 都包含新旧 API 对照表。

主要变化：

- 全局变量 → 调用者静态分配的实例；
- 直接写寄存器 → 端口回调注入；
- 隐式内部缓冲 → 调用者提供显存/配置；
- 固定实现 → 可配置帧格式、字体、校验和功能裁剪；
- 无测试 → 主机侧边界/协议测试。

## 新增模块清单

新增一个模块时，建议保持同样的结构：

1. `Inc/<module>.h`：公开 API、状态码、配置、实例、端口；
2. `Inc/<module>_config.h`：编译期容量与功能开关；
3. `Src/<module>.c`：只依赖标准库，不引用厂商头文件；
4. `Tests/test_<module>.c`：用假端口覆盖协议、边界和错误路径；
5. `Examples/stm32f4/`：一份真实 BSP 接入示例；
6. `CMakeLists.txt`：独立构建/子目录两用，目标名与模块一致；
7. `README.md`：架构、用法、移植、迁移和测试说明。

命名约定：公开符号统一使用模块前缀（`jy61_`、`oled_`、`zb_`），宏使用大写下划线，状态码以 `_OK` / `_ERR_*` 结尾。
