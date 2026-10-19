# GD407_Demo

基于 **arm-none-eabi-gcc + CMake + Ninja + OpenOCD** 的 GD32 纯寄存器工程模板。
当前默认目标：**GD32F407VG**（LQFP100，1MB Flash / 128KB SRAM + 64KB TCM，Cortex-M4F），
板载 HXTAL **25MHz**，系统时钟配到 **SYSCLK 168MHz / AHB 168MHz / APB1 42MHz / APB2 84MHz**（F407 极限频率）。
调试器 **CMSIS-DAP（DAP-Link）**。

设计目标：

- **芯片 / 板级 / 应用三层解耦**——换芯片动 `Core`+`Drivers`，换板子动 `BSP`，业务代码 `App` 一行不改。
- **纯寄存器开发**：只引用兆易创新官方固件库 GD32F4xx_Firmware_Library V3.3.3 的寄存器头（RCU/GPIO/PMU/FMC 等，`REG32` 宏映射），**不编译官方标准外设库的 `.c`**；时钟序列在 `BSP/Src/bsp_clock.c` 逐寄存器展开。
- 芯片参数只有一个真源：`cmake/chip.cmake` 配合 `CMakePresets.json` 的 `CHIP` 自动推导（器件宏、启动文件、链接脚本、Flash 容量校验、OpenOCD target）。

---

## 目录结构

```
GD407_Demo/
│
├── CMakeLists.txt                # 总装脚本（目标名 GD32F407_Register）
├── CMakePresets.json             # 一键配置（CHIP=GD32F407VG）
├── README.md / .gitignore / .clang-format / .clangd / .editorconfig
│
├── cmake/
│   ├── arm-none-eabi.cmake       #   工具链 + cortex-m4 编译标志（含中文路径诊断转码启动器）
│   ├── chip.cmake                #   ★ CHIP 唯一真源：型号拆字/容量校验/路径推导
│   ├── modules.cmake             #   gd32_add_lib / print_size
│   ├── openocd.cmake             #   flash / debug_server / reset 目标
│   └── openocd/{interface,target}/
│       ├── interface/cmsis-dap.cfg
│       └── target/gd32f4.cfg     #   包装：CPUTAPID=0 + source gd32f4x.cfg（兼容 stm32f4x 算法）
│
├── Core/                         # ── 芯片层：换芯片才动（官方文件原样）──
│   ├── Startup/startup_gd32f407_427.S
│   ├── System/system_gd32f4xx.c  #   SystemInit：按 HXTAL 25MHz 配 PLL 到 168MHz + VTOR
│   └── Linker/GD32F407VGx_FLASH.ld   # 1024K Flash / 128K RAM + 64K TCM(0x10000000)
│
├── Drivers/
│   ├── CMSIS/Core/Include/       #   core_cm4.h 等
│   ├── CMSIS/Device/GD/GD32F4xx/Include/
│   │   ├── gd32f4xx.h            #   器件头（基址/IRQn/器件选型，靠 -DGD32F407）
│   │   └── system_gd32f4xx.h
│   └── GD32F4xx_StdPeriph/Include/   # 只放寄存器头：rcu/gpio/pmu/fmc/misc（无 .c）
│       └── gd32f4xx_libopt.h     #   gd32f4xx.h 末尾强制 include，本工程裁剪版
│
├── BSP/                          # ── 板级层 ──
│   ├── CMakeLists.txt
│   ├── Inc/bsp.h                 #   统一接口：clock / delay
│   ├── Inc/bsp_bitband.h         #   Cortex-M4 位带操作
│   └── Src/
│       ├── bsp_clock.c           #   ★ 168MHz 时钟序列 + nvic_vector_table_set/_init/_fini 胶水
│       └── bsp_delay.c           #   SysTick 毫秒 + DWT 微秒
│
├── App/main.c                    # 示例：PB2 推挽输出，位带翻转（500ms 周期闪烁）
├── Middlewares/  tools/  Doc/    # 预留空目录
└── build/debug/                  # 编译产物（gitignore）
```

### 头文件包含链

```
源码 #include "gd32f4xx.h"
        ├─ #include "core_cm4.h"
        └─ #include "gd32f4xx_libopt.h"   ← StdPeriph/Include 下的裁剪版
               ├─ gd32f4xx_rcu.h          （时钟/复位，REG32 裸宏风格）
               ├─ gd32f4xx_gpio.h
               ├─ gd32f4xx_pmu.h
               ├─ gd32f4xx_fmc.h
               └─ gd32f4xx_misc.h
```

---

## 168MHz 时钟要点（bsp_clock.c）

PLL：`168M = HXTAL(25M) / PLLM(25) × PLLN(336) / PLLP(2)`，AHB÷1、APB2÷2、APB1÷4。

GD32F407 与 STM32F407 的关键寄存器差异：

| STM32 | GD32 |
|---|---|
| `RCC->CR/CFGR/PLLCFGR` | `RCU_CTL / RCU_CFG0 / RCU_PLL`（裸宏，非结构体） |
| `RCC->APB1ENR` | `RCU_APB1EN` |
| `PWR->CR/CSR` | `PMU_CTL / PMU_CS` |
| `FLASH->ACR`（WS+预取） | `FMC_WS`（只有 WSCNT，ART 加速器常开） |
| HSE/HSERDY/PLLRDY | HXTAL/HXTALSTB/PLLSTB |

另两点不同：168MHz 时 GD 要开**高驱动**（`PMU_CTL_HDEN`→等 HDRF→`HDS`→等 HDSRF），ST 只需 VOS Scale1；GD 的 FMC 没有预取/缓存使能位。

> 注意：`SystemInit()`（system_gd32f4xx.c）在进 main 前**已经**配好 168MHz，`bsp_clock_init()` 是同参数的完整重配（幂等）。保留它是为了让完整寄存器流程留在 BSP 层，换晶振/降频时只改这里。
> 另外 GD 官方 `SystemInit` 调用的 `nvic_vector_table_set()`（在官方 misc.c 里）由 bsp_clock.c 补了一个寄存器实现，避免编译官方 `.c`。

---

## 工具链

| 工具 | 本机路径 |
|---|---|
| arm-none-eabi-gcc 15.3.1 | `C:\EmbeddedTools\arm-gcc\bin` |
| CMake ≥ 3.20 | 系统 PATH |
| Ninja | 系统 PATH |
| OpenOCD 0.12（主线即可） | PATH |

GD32F4 与 STM32F4 的 SW-DP ID 相同、Flash 控制器兼容，主线 OpenOCD 用
`cmake/openocd/target/gd32f4.cfg`（`CPUTAPID=0` 跳过 ID 校验后 source 自带的 `gd32f4x.cfg`）即可烧录，无需专用分支。

## 常用命令

```bash
cmake --preset debug                          # 配置
cmake --build build/debug                     # 编译（产出 elf/hex/bin + map + 体积打印）
cmake --build build/debug --target flash          # 烧录+校验+复位运行
cmake --build build/debug --target debug_server   # GDB Server 监听 localhost:3333
cmake --build build/debug --target reset          # 复位目标芯片
```

---

## 换型号备忘

- 订货号末位字母即 Flash 密度码（`GD32F407VG` → `G` → 1024K），链接脚本容量在 configure 阶段自动校验，配错直接报错。
- 换同系列型号（405/407/425/427/450/470）只改 `CMakePresets.json` 的 `CHIP`，器件宏/启动文件名在 [chip.cmake](cmake/chip.cmake) 里映射。
- 板载晶振不是 25MHz 时改 [BSP/CMakeLists.txt](BSP/CMakeLists.txt) 的 `HXTAL_VALUE`（填错会导致起振超时、`SystemCoreClock` 换算和延时全错）。
