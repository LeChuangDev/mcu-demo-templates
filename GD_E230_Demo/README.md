# GD_E230_Demo

基于 **arm-none-eabi-gcc + CMake + Ninja + OpenOCD** 的 GD32 纯寄存器工程模板。
当前默认目标：**立创·GD32E230C8T6 开发板**（MCU：GD32E230C8T6，LQFP48，
**Cortex-M23**，**72MHz**，**64KB Flash / 8KB SRAM**），
板载 HXTAL **8MHz** + 32.768kHz RTC 晶振，用户 LED2 接 **PC13（高电平点亮）**。
调试器 **CMSIS-DAP（DAP-Link）**，也兼容 ST-Link V2。

> 注意：GD32E230 是 **Cortex-M23（ARMv8-M Baseline）**，不是 Cortex-M0/M3/M4。
> 它**没有 FPU**、**没有位带（bit-band）**，CMSIS 的 DWT 结构体里也**没有 CYCCNT**。
> 因此微秒延时改用 TIMER5，LED 翻转改用 GPIO_BOP/BC/TG，详见下文。

设计目标：

- **芯片 / 板级 / 应用三层解耦**——换芯片动 `Core`+`Drivers`，换板子动 `BSP`，业务代码 `App` 一行不改。
- **纯寄存器开发**：只引用兆易创新官方 GD32E23x_Firmware_Library 的寄存器头（RCU/GPIO/FMC/TIMER 等，`REG32` 宏映射），**不编译官方标准外设库的 `.c`**；时钟序列在 `BSP/Src/bsp_clock.c` 逐寄存器展开。
- 芯片参数只有一个真源：`cmake/chip.cmake` 配合 `CMakePresets.json` 的 `CHIP` 自动推导（器件宏、启动文件、链接脚本、Flash 容量校验、OpenOCD target）。

---

## 目录结构

```
GD_E230_Demo/
│
├── CMakeLists.txt                # 总装脚本（目标名 GD32E230_Register）
├── CMakePresets.json             # 一键配置（CHIP=GD32E230C8）
├── README.md / .gitignore / .clang-format / .clangd / .editorconfig
│
├── cmake/
│   ├── arm-none-eabi.cmake       #   工具链 + cortex-m23 编译标志（含中文路径诊断转码启动器）
│   ├── chip.cmake                #   ★ CHIP 唯一真源：型号拆字/容量校验/路径推导
│   ├── modules.cmake             #   gd32_add_lib / print_size
│   ├── openocd.cmake             #   flash / debug_server / reset 目标
│   └── openocd/{interface,target}/
│       ├── interface/cmsis-dap.cfg
│       └── target/gd32e23.cfg    #   包装：CPUTAPID=0 + source gd32e23x.cfg
│
├── Core/                         # ── 芯片层：换芯片才动（官方文件原样）──
│   ├── Startup/startup_gd32e23x.S
│   ├── System/system_gd32e23x.c  #   SystemInit：按 HXTAL 8MHz 配 PLL 到 72MHz + VTOR
│   └── Linker/GD32E230C8x_FLASH.ld   # 64K Flash / 8K RAM
│
├── Drivers/
│   ├── CMSIS/Core/Include/       #   core_cm23.h / cmsis_compiler.h / cmsis_gcc.h
│   ├── CMSIS/Device/GD/GD32E23x/Include/
│   │   ├── gd32e23x.h            #   器件头（基址/IRQn/器件选型，靠 -DGD32E23x）
│   │   └── system_gd32e23x.h
│   └── GD32E23x_StdPeriph/Include/   # 只放寄存器头：rcu/gpio/fmc/timer/misc/pmu（无 .c）
│       └── gd32e23x_libopt.h     #   gd32e23x.h 末尾强制 include，本工程裁剪版
│
├── BSP/                          # ── 板级层 ──
│   ├── CMakeLists.txt
│   ├── Inc/bsp.h                 #   统一接口：clock / delay / led
│   └── Src/
│       ├── bsp_clock.c           #   ★ 72MHz 时钟（可重配、带超时）+ 链接胶水
│       ├── bsp_delay.c           #   SysTick 毫秒（WFI 睡眠）+ TIMER5 微秒
│       └── bsp_led.c             #   板载 LED：PC13，只改顶部 4 个宏即可换板
│
├── App/main.c                    # 示例：LED 500ms 翻转（App 不碰寄存器）
├── Middlewares/  tools/  Doc/    # 预留空目录
└── build/debug/                  # 编译产物（gitignore）
```

### 头文件包含链

```
源码 #include "gd32e23x.h"
        ├─ #include "core_cm23.h"
        │     └─ #include "cmsis_compiler.h" → cmsis_gcc.h
        └─ #include "gd32e23x_libopt.h"   ← StdPeriph/Include 下的裁剪版
               ├─ gd32e23x_rcu.h           （时钟/复位，REG32 裸宏风格）
               ├─ gd32e23x_gpio.h
               ├─ gd32e23x_fmc.h
               ├─ gd32e23x_timer.h
               ├─ gd32e23x_misc.h
               └─ gd32e23x_pmu.h
```

---

## 72MHz 时钟要点（bsp_clock.c）

PLL：`72M = HXTAL(8M) × PLLMF(9)`，PLL 输入不分频；AHB÷1、APB2÷1、APB1÷1。
72MHz 时 Flash 需要 **2 个等待周期**（`WS_WSCNT_2`）。

GD32E230 与 GD32F4 的关键寄存器差异：

| GD32F4 | GD32E23x |
|---|---|
| `RCU_CTL` | `RCU_CTL0` |
| `RCU_CFG0` + `RCU_PLL` | 全部在 `RCU_CFG0`（PLLSEL / PLLMF / PLLDV） |
| `PLLPSC/PLLN/PLLP/PLLQ` 三段式 | 只有 `PLLMF` 倍频，没有 M/N/P/Q |
| `RCU_AHB1EN` | `RCU_AHBEN`（GPIO 挂在 AHB 上） |
| PMU `LDOVS/HDEN/HDS` 高驱动 | **不需要**，72MHz 直接跑 |
| `nvic_vector_table_set` 在 misc.c | 同样在 misc.c，本工程在 BSP 里补寄存器实现 |

> 注意：`SystemInit()`（system_gd32e23x.c）在进 main 前**已经**配好 72MHz。
> `bsp_clock_init()` 会先切回 IRC8M、关闭 PLL，再完整重配（保证 PLL 参数以 BSP 为准），
> 所有等待都带超时；返回非 0 表示某一步超时，`SystemCoreClock` 已按实际时钟更新，
> 程序仍可继续运行。换晶振/降频时只改 `bsp_clock.c`。
> 另外官方 `SystemInit` 调用的 `nvic_vector_table_set()`（在官方 misc.c 里）由
> bsp_clock.c 补了一个寄存器实现，避免编译官方 `.c`。

---

## 延时实现（bsp_delay.c）

| 功能 | 实现 |
|---|---|
| 毫秒 | SysTick 1ms 中断累加；`bsp_delay_ms()` 等待时执行 `__WFI()` 让 CPU 睡眠 |
| 微秒 | TIMER5 基本定时器，`72MHz ÷ 72 = 1MHz` 自由递增；16 位计数器，超长延时按 50ms 分块 |

为什么不用 DWT：Cortex-M23 属于 ARMv8-M Baseline，`core_cm23.h` 里的 `DWT_Type`
**没有 CYCCNT 成员**（该计数器是可选组件，CMSIS 按不存在处理），照抄 M4 的
`DWT->CYCCNT` 写法会直接编译报错。改用 TIMER5 后，微秒延时与内核调试单元无关，
也不占用 SysTick。

---

## 板载 LED（bsp_led.c）

立创·GD32E230C8T6 开发板的 **LED2 = PC13，高电平点亮**。

Cortex-M23 **不支持位带**，所以不能用 M3/M4 的 `BB_REG()` 写法；这里使用 GD32
自带的位操作寄存器：

| 操作 | 寄存器 | 说明 |
|---|---|---|
| 置高 | `GPIO_BOP(GPIOC)` | 写 1 置位，写 0 不影响其它位 |
| 置低 | `GPIO_BC(GPIOC)` | 写 1 清位 |
| 翻转 | `GPIO_TG(GPIOC)` | 写 1 取反 |

换板子时只改 `bsp_led.c` 顶部的 `LED_GPIO / LED_PIN / LED_RCC_EN / LED_ACTIVE_LEVEL`
四个宏，App 层不用动。

---

## 工具链

| 工具 | 本机路径 |
|---|---|
| arm-none-eabi-gcc 15.3.1 | `C:\EmbeddedTools\arm-gcc\bin` |
| CMake ≥ 3.20 | 系统 PATH |
| Ninja | 系统 PATH |
| OpenOCD 0.12（主线即可） | PATH |

OpenOCD 0.12 已内置 `target/gd32e23x.cfg`（Cortex-M23 + stm32f1x Flash 算法），
本工程的 `cmake/openocd/target/gd32e23.cfg` 只是包装一层并设 `CPUTAPID=0`
跳过 ID 校验。

> 工程文件里不写死任何工具链绝对路径：换电脑时只要把上表四个工具加进 `PATH`，
> 就能直接编译，`cmake/arm-none-eabi.cmake` 和 `CMakePresets.json` 都不用改。

### 烧录接线（DAP-Link / ST-Link V2）

| 调试器 | 开发板 |
|---|---|
| 3V3 | 3V3 |
| GND | GND |
| SWDIO | SWDIO |
| SWCLK | SWCLK |
| （可选）NRST | NRST |

用 SWD 烧录时把 **BOOT0 接 GND**（板上跳线帽），再复位进入用户 Flash 启动。
也可以用板载 USB 串口走 ISP 下载，但本模板的 `flash` 目标是 SWD 方案。

## 移动 / 复制整个工程

把 `GD_E230_Demo/` 整个文件夹复制或移动到任何位置（路径可以含空格、中文），
**不需要修改任何配置**，直接用下面的方式编译/烧录：

- VSCode：`Alt+B` 编译、`Alt+U` 编译并烧录（单手、任意焦点）；也可以按 `Ctrl+Shift+B`
  只编译，或在任务面板选择“编译 Build”“烧录 Flash”（“烧录 Flash”会自动先编译再烧录）。
- 命令行：
  ```bash
  cmake -P tools/build.cmake                    # 编译
  cmake -DTARGET=flash -P tools/build.cmake     # 编译并烧录
  ```

`tools/build.cmake` 每次构建前都会核对 `build/debug/CMakeCache.txt` 里记录的工程路径：
目录变了就自动删除旧缓存、重新执行 `cmake --preset debug`，然后正常增量编译。
因此 `build/` 可以跟着文件夹一起拷贝，不会因为旧绝对路径而报错。

> 只有绕开该脚本、直接执行 `cmake --build build/debug` 时，移动目录后才需要手动删除 `build/`。

## 常用命令

```bash
cmake -P tools/build.cmake                    # 编译（自动配置，产出 elf/hex/bin + map + 体积打印）
cmake -DTARGET=flash -P tools/build.cmake     # 编译并烧录+校验+复位运行
cmake -DPRESET=release -P tools/build.cmake   # Release（-Os + LTO + newlib-nano）

# 等价的手动方式（首次配置后）
cmake --preset debug
cmake --build build/debug
cmake --preset release
cmake --build build/release
cmake --build build/debug --target flash          # 烧录+校验+复位运行
cmake --build build/debug --target debug_server   # GDB Server 监听 localhost:3333
cmake --build build/debug --target reset          # 复位目标芯片
```

产物：`build/debug/GD32E230_Register`（ELF 可执行文件，无扩展名）
以及 `GD32E230_Register.hex / .bin / .map`。

---

## 换型号备忘

- 订货号末位字母即 Flash 密度码（`GD32E230C8` → `8` → 64K），链接脚本容量在 configure 阶段自动校验，配错直接报错。
- 换同系列型号（E230/E231/E235）只改 `CMakePresets.json` 的 `CHIP`（三者共用 `gd32e23x.h` 与 `startup_gd32e23x.S`）。
- 板载晶振不是 8MHz 时改 [BSP/CMakeLists.txt](BSP/CMakeLists.txt) 的 `HXTAL_VALUE`（填错会导致起振超时、`SystemCoreClock` 换算和延时全错）。
- 换板子且 LED 不在 PC13 时改 [BSP/Src/bsp_led.c](BSP/Src/bsp_led.c) 顶部四个宏。

## 器件库来源

- `Core/` 与 `Drivers/` 下的 GD32E23x 文件来自 GigaDevice 官方仓库
  [GD32E23x_Firmware_Library](https://github.com/GigaDevice-GD32-MCU/GD32E23x_Firmware_Library)
  （`Firmware/CMSIS` + `Firmware/GD32E23x_standard_peripheral/Include`）。
- `gd32e23x_libopt.h` 是官方头文件的裁剪版，只保留本工程用到的外设头。
