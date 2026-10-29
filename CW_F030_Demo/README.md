# CW32F030_Demo

基于 **arm-none-eabi-gcc + CMake + Ninja + pyOCD** 的 CW32 纯寄存器工程模板。
当前默认目标：**CW32F030C8T6**（LQFP48，64KB Flash / 8KB SRAM，Cortex-M0+），
开发板：**立创·地文星 LCKFB-DWX-CW32F030C8T6**。
板载 HSE **8MHz** 晶振，系统时钟配到 **SYSCLK = HCLK = PCLK = 64MHz**（F030 最高主频）。
下载器：板载 **DAP-Link**（也兼容 ST-Link / 串口 ISP）。

设计目标（与其他 Templates 工程同一套骨架）：

- **芯片 / 板级 / 应用三层解耦**——换芯片动 `Core`+`Drivers`，换板子动 `BSP`，业务代码 `App` 一行不改。
- **纯寄存器开发**：只引用武汉芯源官方固件库 V2.2 的【寄存器结构体头】（GPIO/RCC/FLASH），不编译官方 SPL 的 `.c`；时钟序列在 `BSP/Src/bsp_clock.c` 逐寄存器展开。
- 芯片参数只有一个真源：`cmake/chip.cmake` 配合 `CMakePresets.json` 的 `CHIP` 自动推导（器件宏、启动文件、链接脚本、容量校验、pyOCD target）。

---

## 目录结构

```
CW_F030_Demo/
│
├── CMakeLists.txt                # 总装脚本（目标名 CW32F030_Register）
├── CMakePresets.json             # 一键配置（CHIP=CW32F030C8T6）
├── README.md / .gitignore / .clang-format / .clangd / .editorconfig
│
├── cmake/
│   ├── arm-none-eabi.cmake       #   工具链 + cortex-m0plus 编译标志（无 FPU）
│   ├── chip.cmake                #   ★ CHIP 唯一真源：型号拆字/容量校验/路径推导
│   ├── modules.cmake             #   mcu_add_lib / print_size
│   ├── pyocd.cmake               #   flash / debug_server / reset 目标
│   └── packs/
│       └── WHXY.CW32F030_DFP.1.0.5.pack   # 官方 DFP，含 Flash 算法
│
├── Config/
│   └── cw32f030_conf.h           # 裁剪版配置（HSE 8M，LED PC13，只开 GPIO/RCC/FLASH）
│
├── Core/                         # ── 芯片层 ──
│   ├── Startup/startup_cw32f030.s       # 手写 GCC 启动文件 + 48 项向量表
│   ├── System/system_cw32f030.c/.h      # 官方 SystemInit（加载 Trim，不配 PLL）
│   ├── System/init_stubs.c              # -nostartfiles 下的 _init/_fini 空桩
│   └── Linker/CW32F030x8_FLASH.ld       # 64K Flash / 8K RAM
│
├── Drivers/
│   ├── CMSIS/Core/Include/       #   core_cm0plus.h / cmsis_gcc.h 等
│   ├── CMSIS/Device/CW/CW32F030/Include/
│   │   ├── cw32f030.h            #   器件头（基址/IRQn/寄存器结构体）
│   │   └── base_types.h
│   └── CW32F0xx_StdPeriph/Include/   # 只放寄存器结构体头：gpio/rcc/flash/systick（无 .c）
│
├── BSP/                          # ── 板级层 ──
│   ├── CMakeLists.txt
│   ├── Inc/bsp.h                 #   统一接口：clock / delay
│   ├── Inc/bsp_bitband.h         #   M0+ 无位带 -> GPIO 原子寄存器宏
│   └── Src/
│       ├── bsp_clock.c           #   ★ HSE 8M -> PLL x8 = 64MHz 时钟序列
│       └── bsp_delay.c           #   SysTick 毫秒 + SysTick VAL 微秒
│
├── App/main.c                    # 示例：PC13 推挽输出，低电平点亮，500ms 翻转
├── tools/build.cmake             # 编译/烧录统一入口（自动处理工程移动后的缓存失效）
├── Middlewares/  Doc/            # 预留空目录
└── build/debug/                  # 编译产物（gitignore）
```

### 头文件包含链

```
源码 #include "cw32f030_conf.h"
        ├─ #include "cw32f030.h"
        │      ├─ #include "core_cm0plus.h"
        │      ├─ #include "system_cw32f030.h"
        │      └─ #include "base_types.h"
        ├─ #include "cw32f030_gpio.h"   （GPIO 寄存器结构体位域）
        ├─ #include "cw32f030_rcc.h"    （SYSCTRL 寄存器 + 时钟宏）
        └─ #include "cw32f030_flash.h"  （FLASH 等待周期）
```

CW32 与 AT32/ST 不同：器件头 `cw32f030.h` **不会**自动 include 一个 conf.h，
所以本工程的 `cw32f030_conf.h` 由 BSP/App 主动包含，负责聚合外设头并写板级常量。

---

## 64MHz 时钟要点（bsp_clock.c）

```
HSE 8MHz ──> PLL x8 ──> SYSCLK 64MHz
HCLK = SYSCLK / 1 = 64MHz
PCLK = HCLK  / 1 = 64MHz
```

与 AT32/STM32 模板的关键差异：

1. 时钟外设叫 **SYSCTRL**（`CW_SYSCTRL`），官方把 RCC 功能全放在这里；
2. **CR0/CR1/CR2/IER 带 KEY 保护**：写入时高 16 位必须是 `0x5A5A`
   （`SYSCTRL_BYPASS`），否则写不进去。HSE/PLL/HSI 配置寄存器、AHBEN/APBENx 不需要 KEY；
3. HSE 参数是一个 32 位寄存器：`DETCNT = 8GHz / f_HSE`（8MHz 时为 1000）、
   `WAITCYCLE = 131072`、`FREQRANGE = f_HSE / 8MHz`（8MHz 时为 1）、
   `DRIVER = NORMAL`、`FLT = 关`。PF0/PF1 必须先切成模拟功能；
4. PLL 的输入/输出档位由 `FREQIN`/`FREQOUT` 两个字段描述，不是简单倍数：
   8MHz 对应 `FREQIN = 1`，64MHz 对应 `FREQOUT = 4`；
5. 64MHz 需要 **Flash 3 个等待周期**（`FLASH_Latency_3`），且等待周期非 0 时
   必须同时打开 **FETCH + CACHE**（`FLASH->CR2 = 0x5A5A0000 | ...`）；
6. 官方 `SystemInit()` 进 main 前只加载 HSI/LSI 出厂 Trim、把 `SystemCoreClock`
   设为 8MHz，**不配置 PLL**；真正的 64MHz 配置在 `bsp_clock_init()` 完成；
7. 全局时钟变量是 CMSIS 标准的 **`SystemCoreClock`**（注意大小写）。

### Cortex-M0+ 的两个坑

- **没有位带（bit-band）**：`0x22000000/0x42000000` 别名区不存在。
  用 GPIO 的 `BSRR`（置位/清位）、`BRR`（清位）、`TOG`（翻转）实现原子操作，
  见 `BSP/Inc/bsp_bitband.h`。
- **DWT 没有 CYCCNT**：微秒延时不能用 `DWT->CYCCNT`，本工程改为读
  `SysTick->VAL` 并处理 24 位重装，分辨率 1/64MHz ≈ 15.6ns。

---

## 工具链

| 工具 | 说明 |
|---|---|
| arm-none-eabi-gcc 15.3.1 | `C:\EmbeddedTools\arm-gcc\bin` |
| CMake ≥ 3.20 | 系统 PATH |
| Ninja | 系统 PATH |
| Python + pyOCD ≥ 0.45 | `pip install pyocd` |

### 为什么用 pyOCD 而不是 OpenOCD

OpenOCD 主线**没有** CW32F030 的 target 配置和 Flash 驱动（`target/cw32f030.cfg`
不存在），直接烧录会失败。pyOCD 支持厂商 CMSIS-Pack（DFP），
`WHXY.CW32F030_DFP.1.0.5.pack` 内自带 `FlashCW32F030.FLM` 算法，
target 名 `cw32f030c8`（由 `chip.cmake` 按型号自动推导）。

pack 已放在工程内，无需全局安装。验证：

```bash
pyocd list --targets --pack cmake/packs/WHXY.CW32F030_DFP.1.0.5.pack
```

应能看到 `cw32f030c8  WHXY  CW32F030C8`。

## 常用命令

```bash
# ★ 推荐：统一入口，自动判断是否需要重新配置
cmake -P tools/build.cmake                       # 编译（产出 elf/hex/bin + map + 体积打印）
cmake -DTARGET=flash -P tools/build.cmake        # 编译 + 烧录 + 校验 + 复位
cmake -DTARGET=debug_server -P tools/build.cmake # GDB Server :3333
cmake -DTARGET=reset -P tools/build.cmake        # 复位

# 等价的原生命令（工程目录未移动时可用）
cmake --preset debug                 # 配置
cmake --build build/debug            # 编译
cmake --build build/debug --target flash         # 烧录
```

### VSCode 快捷键

工程内 `.vscode/tasks.json` 提供两个任务，终端面板始终弹出并实时显示输出。
Templates 下所有工程使用同一套快捷键（单手可操作、任意焦点生效）：

| 键 | 任务 | 说明 |
|---|---|---|
| `Alt+B` | 编译 Build | 只编译，不碰芯片 |
| `Alt+U` | 烧录 Flash | 先增量编译，再 pyOCD 烧录+校验+复位 |
| `F1` / `F7` | 编译 Build | 原有功能键，仍可用 |
| `F8` | 烧录 Flash | 原有功能键，仍可用 |

### 终端编码说明（乱码排查）

- **编译/烧录日志刻意保持纯 ASCII**：Ninja 的 `COMMENT` 在 Windows 控制台上
  会被按 ANSI 代码页（GBK）再解码一次，UTF-8 中文必然变成 `鐑у綍...`，
  所以 `cmake/*.cmake` 里的 `COMMENT` 一律用英文。
- **GCC 诊断由转码包装器处理**：`cmake/gcc_utf8_wrapper.py` 按行判断编码——
  源码行是 UTF-8（中文注释原样透传），含中文的路径行是 GBK（转成 UTF-8）。
  这样警告/错误里引用的源码和路径都不会乱码。
- **在 VSCode 之外的 cmd/PowerShell 窗口**看到 CMake 配置阶段的中文
  （`>>> 芯片: ...`）乱码时，先执行 `chcp 65001` 再运行命令。

### 整体移动文件夹

`tools/build.cmake` 每次运行都会核对 `build/debug/CMakeCache.txt` 里记录的源码目录：

- 与当前工程根一致 → 直接增量编译；
- 不一致（整个文件夹被移动/改名/拷贝）→ 自动删除 `build/debug` 并重新 `cmake --preset debug`。

所以工程可以整个拷到 U 盘、换盘符、换用户名目录，`F7`/`F8` 和多核命令行都不用改。
其余配置均使用相对路径（`${sourceDir}` / `compile_commands.json` / `.clangd` 的
`CompilationDatabase: build/debug`），不受移动影响。

### 烧录接线

板载 DAP-Link 直接插 USB 即可；若用外部下载器，接 SWDIO/SWCLK/GND/3V3。
pyOCD 会枚举 CMSIS-DAP 探针，默认选中第一个；多探针时加 `-u <UID>`。

---

## 换型号备忘

订货号 `CW32F030C8T6`：`CW32F030` + 封装码 `C` + 密度码 `8` + 尾缀 `T6`，
**密度码在封装码后一位（倒数第 3 位）**。

| 密度码 | Flash | SRAM | 本模板支持 |
|---|---|---|---|
| `6` | 32KB | 8KB | 需改用 `CW32F030x6_FLASH.ld` |
| `8` | 64KB | 8KB | ✅ 默认 |

换同系列型号只需改 `CMakePresets.json` 的 `CHIP`；`chip.cmake` 会：
推导 `PYOCD_TARGET`、选择启动文件/system 文件、校验链接脚本容量。
换内核系列（如 CW32L 系列）才需要同时改 `cmake/arm-none-eabi.cmake` 的 `CPU_FLAGS`。
