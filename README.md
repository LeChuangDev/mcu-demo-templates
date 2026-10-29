# MCU Firmware Demo Templates

一套基于 **arm-none-eabi-gcc + CMake + Ninja** 的 MCU 工程模板集合。
所有模板共用同一套分层骨架（App / BSP / Core / Drivers / Config），
以**纯寄存器**方式驱动外设，芯片/板级/应用三层解耦；`ESP_S3_Demo` 则基于 ESP-IDF。

## 模板一览

| 目录 | 目标芯片 | 内核 | 主频 | 存储 | 烧录 / 调试 |
|---|---|---|---|---|---|
| [AT_F435_Demo](AT_F435_Demo/) | AT32F435CGT7 | Cortex-M4F | 288 MHz | 1 MB Flash / 384 KB SRAM | OpenOCD + CMSIS-DAP |
| [CW_F030_Demo](CW_F030_Demo/) | CW32F030C8T6 | Cortex-M0+ | 64 MHz | 64 KB Flash / 8 KB SRAM | pyOCD（CMSIS-Pack DFP） |
| [ESP_S3_Demo](ESP_S3_Demo/) | ESP32-S3 | Xtensa LX7 双核 | 240 MHz | 512 KB SRAM + 片外 Flash（默认 16 MB） | ESP-IDF + esptool / USB-JTAG |
| [GD_E230_Demo](GD_E230_Demo/) | GD32E230C8T6 | Cortex-M23 | 72 MHz | 64 KB Flash / 8 KB SRAM | OpenOCD + CMSIS-DAP |
| [GD_F407_Demo](GD_F407_Demo/) | GD32F407VG | Cortex-M4F | 168 MHz | 1 MB Flash / 128 KB SRAM + 64 KB TCM | OpenOCD + CMSIS-DAP |
| [ST_F103_Demo](ST_F103_Demo/) | STM32F103C8T6 | Cortex-M3 | 72 MHz | 64 KB Flash / 20 KB SRAM | OpenOCD + CMSIS-DAP |
| [ST_F407_Demo](ST_F407_Demo/) | STM32F407VGT6 | Cortex-M4F | 168 MHz | 1 MB Flash / 128 KB SRAM + 64 KB CCM | OpenOCD + CMSIS-DAP |
| [ST_G431_Demo](ST_G431_Demo/) | STM32G431CBT6 | Cortex-M4F | 170 MHz | 128 KB Flash / 32 KB SRAM + 10 KB CCM | OpenOCD + CMSIS-DAP |
| [ST_H743_Demo](ST_H743_Demo/) | STM32H743VG | Cortex-M7 | 400 MHz | 1 MB Flash / 512 KB AXI + 128 KB DTCM + 64 KB ITCM | OpenOCD + CMSIS-DAP |

> 各模板的时钟树、引脚定义、工具链细节和注意事项，见对应目录下的 `README.md`。

## 统一目录结构

除 `ESP_S3_Demo` 外，所有模板结构一致：

```
<芯片>_Demo/
├── App/                 # 应用层：main.c，只 include "bsp.h"
├── BSP/                 # 板级层：时钟 / 延时 / LED 等板级封装
├── Core/                # 启动文件、System、链接脚本（换芯片才动）
├── Drivers/
│   ├── CMSIS/           # ARM CMSIS Core + 厂商器件头
│   ├── <厂商库>/         # 部分模板：厂商寄存器结构体头（不编译官方 SPL/HAL 的 .c）
│   └── External/        # 片外芯片驱动（ssd1306 / mpu6050 / w25q64）
├── Config/              # 编译期配置（器件与中间件裁剪）
├── Middlewares/         # 预留：FreeRTOS / LVGL / FatFs / mbedTLS / micro-ROS
├── cmake/               # 工具链、chip.cmake、模块与烧录脚本
├── tools/build.cmake    # 统一构建入口
├── CMakePresets.json    # CHIP 预设
└── build/               # 构建产物（已 gitignore）
```

`ESP_S3_Demo` 沿用相同的 App / BSP / Config 分层，只是目录名为
`main/`、`BSP/`、`Config/`，构建入口是 `tools/idf.ps1`（Linux/macOS 为 `idf.sh`）。

### 设计约定

- **三层解耦**：换芯片动 `Core` + `Drivers`，换板子动 `BSP`，`App` 一行不改。
- **单一真源**：`cmake/chip.cmake` 根据 `CMakePresets.json` 里的 `CHIP`
  自动推导器件宏、启动文件、链接脚本、容量校验和仿真器 target。
- **第三方库原样放置**：厂商库与中间件不修改，差异通过 `Config/` 和 BSP 包装层吸收。
- **可移动**：整个工程目录可以复制到任意路径（含空格、中文），无需改配置。

## 环境要求

| 工具 | 说明 |
|---|---|
| arm-none-eabi-gcc | 模板按 15.x 验证；加入 `PATH` 即可 |
| CMake ≥ 3.20 | 构建与配置 |
| Ninja | 生成器 |
| OpenOCD 0.12 | AT / GD / ST 模板的烧录与调试 |
| pyOCD ≥ 0.45 | 仅 `CW_F030_Demo`（`pip install pyocd`） |
| ESP-IDF ≥ v5.0 | 仅 `ESP_S3_Demo`，推荐 v5.5.x |

工程内不写死任何工具链绝对路径，换电脑时只需把对应工具加入 `PATH`。

## 快速开始

### ARM-GCC 模板（AT / CW / GD / ST）

```bash
cd ST_F407_Demo

# 推荐：统一入口，自动判断是否需要重新配置
cmake -P tools/build.cmake                    # 编译（debug），产出 elf / hex / bin / map
cmake -DTARGET=flash -P tools/build.cmake     # 编译并烧录
cmake -DPRESET=release -P tools/build.cmake   # Release 构建

# 等价的 CMake 原生命令
cmake --preset debug
cmake --build build/debug
cmake --build build/debug --target flash
```

VSCode 下：`Alt+B` 编译、`Alt+U` 编译并烧录，`Ctrl+Shift+B` 只编译；
也可在任务面板选择「编译 Build」「烧录 Flash」。

> `AT_F435_Demo` 与 `CW_F030_Demo` 目前只提供 `debug` 预设，暂不支持 `-DPRESET=release`。

### ESP32-S3

```powershell
cd ESP_S3_Demo
.\tools\idf.ps1 build
.\tools\idf.ps1 flash monitor
.\tools\idf.ps1 -p COM3 flash monitor    # 指定串口
```

Linux / macOS 把 `.\tools\idf.ps1` 换成 `./tools/idf.sh` 即可。
脚本会自动查找并激活 ESP-IDF，无需事先打开「ESP-IDF PowerShell」。

## 新增一个模板

1. 复制芯片/内核最接近的模板目录，重命名为 `<厂商缩写>_<系列>_Demo`。
2. 修改 `CMakePresets.json` 中的 `CHIP` 为目标型号。
3. 替换 `Core/Startup`、`Core/Linker` 与 `Drivers` 下的厂商器件头。
4. 在 `BSP` 中按新板子修改时钟配置和引脚定义。
5. 执行 `cmake -P tools/build.cmake` 验证配置、编译与容量校验。

`CHIP` 型号会由 `cmake/chip.cmake` 自动拆分并校验，无需手写器件宏或路径。
