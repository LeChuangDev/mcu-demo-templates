# AT435_Demo

基于 **arm-none-eabi-gcc + CMake + Ninja + OpenOCD** 的 AT32 纯寄存器工程模板。
当前默认目标：**AT32F435CGT7**（LQFP48，1MB Flash / 384KB SRAM，Cortex-M4F），
板载 HEXT **8MHz**，系统时钟配到 **SCLK 288MHz / AHB 288MHz / APB1=APB2 144MHz**（F435 最高主频）。
调试器 **CMSIS-DAP（DAP-Link）**。

设计目标（与 GD407_Demo 同一套骨架）：

- **芯片 / 板级 / 应用三层解耦**——换芯片动 `Core`+`Drivers`，换板子动 `BSP`，业务代码 `App` 一行不改。
- **纯寄存器开发**：只引用雅特力官方固件库 V2.2.x 的【寄存器结构体头】（CRM/GPIO/PWC/FLASH），不编译官方 SPL 的 `.c`；时钟序列在 `BSP/Src/bsp_clock.c` 逐寄存器展开。
- 芯片参数只有一个真源：`cmake/chip.cmake` 配合 `CMakePresets.json` 的 `CHIP` 自动推导（器件宏、启动文件、链接脚本、容量校验、OpenOCD target）。

---

## 目录结构

```
AT435_Demo/
│
├── CMakeLists.txt                # 总装脚本（目标名 AT32F435_Register）
├── CMakePresets.json             # 一键配置（CHIP=AT32F435CGT7）
├── README.md / .gitignore / .clang-format / .clangd / .editorconfig
│
├── cmake/
│   ├── arm-none-eabi.cmake       #   工具链 + cortex-m4 编译标志
│   ├── chip.cmake                #   ★ CHIP 唯一真源：型号拆字/容量校验/路径推导
│   ├── modules.cmake             #   mcu_add_lib / print_size
│   ├── openocd.cmake             #   flash / debug_server / reset 目标
│   └── openocd/{interface,target}/
│       ├── interface/cmsis-dap.cfg
│       └── target/at32f435.cfg   #   包装：source 雅特力版 at32f435xG.cfg
│
├── Config/
│   └── at32f435_437_conf.h       # 裁剪版器件配置（HEXT 8M，只开 CRM/GPIO/PWC/FLASH）
│
├── Core/                         # ── 芯片层（雅特力官方文件，仅 system.c 有 1 处去 SPL 依赖改动）──
│   ├── Startup/startup_at32f435_437.s
│   ├── System/system_at32f435_437.c/.h   # SystemInit 只复位 CRM + 设 VTOR，不配 PLL
│   └── Linker/AT32F435xG_FLASH.ld        # 1024K Flash / 384K RAM，_estack=0x20060000
│
├── Drivers/
│   ├── CMSIS/Core/Include/       #   core_cm4.h 等（与 GD407 工程共用同一份 CMSIS Core）
│   ├── CMSIS/Device/AT/AT32F435_437/Include/
│   │   ├── at32f435_437.h        #   器件头（基址/IRQn/订货号选型，靠 -DAT32F435CGT7）
│   │   └── at32f435_437_def.h
│   └── AT32F4xx_StdPeriph/Include/   # 只放寄存器结构体头：crm/gpio/pwc/flash（无 .c）
│
├── BSP/                          # ── 板级层 ──
│   ├── CMakeLists.txt
│   ├── Inc/bsp.h                 #   统一接口：clock / delay
│   ├── Inc/bsp_bitband.h         #   Cortex-M4 位带操作
│   └── Src/
│       ├── bsp_clock.c           #   ★ 288MHz 时钟序列（含 _init/_fini 链接桩）
│       └── bsp_delay.c           #   SysTick 毫秒 + DWT 微秒
│
├── App/main.c                    # 示例：PB2 推挽输出，位带翻转（500ms 周期闪烁）
├── tools/build.cmake             # 编译/烧录统一入口（自动处理工程移动后的缓存失效）
├── Middlewares/  Doc/            # 预留空目录
└── build/debug/                  # 编译产物（gitignore）
```

### 头文件包含链（与 GD 工程不同，注意）

```
源码 #include "at32f435_437.h"
        ├─ #include "core_cm4.h"
        ├─ #include "system_at32f435_437.h"
        ├─ #include "at32f435_437_def.h"
        └─ #include "at32f435_437_conf.h"        ← Config/ 下的裁剪版
               ├─ at32f435_437_crm.h            （CRM 寄存器结构体位域）
               ├─ at32f435_437_gpio.h
               ├─ at32f435_437_pwc.h
               └─ at32f435_437_flash.h
```

---

## 288MHz 时钟要点（bsp_clock.c）

PLL：`sclk = HEXT × pll_ns / (pll_ms × pll_fr) = 8M × 144 / (1 × 4) = 288M`。

与 GD32F407 168MHz 序列的关键差异：

1. 时钟外设叫 **CRM**，寄存器是结构体+位域：`CRM->ctrl / ->cfg / ->pllcfg / ->misc2`；
2. 288MHz 必须先把 **LDO 切到 1.3V**（`PWC->ldoov_bit.ldoovsel = PWC_LDO_OUTPUT_1V3`，要求 VDD>3.0V、-40~85℃）；
3. Flash 不是写 WS 等待周期，而是写时钟分频 **`FLASH->divr_bit.fdiv = FLASH_CLOCK_DIV_3`**；
4. 切 SCLK 到 PLL 前后要开/关 **auto step mode**（`misc2.auto_step_en`）；
5. AT 的 `SystemInit()` 进 main 前**只复位 CRM/设 VTOR，不配 PLL**，`bsp_clock_init()` 是正式配置，必须调用；
6. 全局时钟变量是全小写的 **`system_core_clock`**（更新函数 `system_core_clock_update()`）。

---

## 工具链

| 工具 | 本机路径 |
|---|---|
| arm-none-eabi-gcc 15.3.1 | `C:\EmbeddedTools\arm-gcc\bin` |
| CMake ≥ 3.20 | 系统 PATH |
| Ninja | 系统 PATH |
| OpenOCD | ★ 需雅特力 **OpenOCD_Artery**（见下） |

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

工程内 `.vscode/tasks.json` 提供两个任务，终端面板始终弹出并实时显示输出：

| 键 | 任务 | 说明 |
|---|---|---|
| `F1` / `F7` | 编译 Build | 只编译，不碰芯片 |
| `F8` | 烧录 Flash | 先增量编译，再 OpenOCD 烧录+校验+复位 |

### 整体移动文件夹

`tools/build.cmake` 每次运行都会核对 `build/debug/CMakeCache.txt` 里记录的源码目录：

- 与当前工程根一致 → 直接增量编译；
- 不一致（整个文件夹被移动/改名/拷贝）→ 自动删除 `build/debug` 并重新 `cmake --preset debug`。

所以工程可以整个拷到 U 盘、换盘符、换用户名目录，`F7`/`F8` 和多核命令行都不用改。
其余配置均使用相对路径（`${sourceDir}` / `compile_commands.json` / `.clangd` 的
`CompilationDatabase: build/debug`），不受移动影响。

### OpenOCD 烧录（重要）

主线 OpenOCD（openocd.org，含本机 0.12）源码虽已带 artery flash 驱动，但发布包的
`tcl/target/` 下**没有** `at32*.cfg`，不能像 GD32F4 那样复用 stm32f4x.cfg。
必须使用雅特力官方 **OpenOCD_Artery**（官网"第三方编程工具"或 GitHub ArteryTek 组织下载），
其 `scripts/target/` 按密度提供 `at32f435xC/xD/xG/xM.cfg`。

configure 时用 `-DOPENOCD_EXECUTABLE` 指定雅特力版可执行文件即可，例如：

```bash
cmake --preset debug -DOPENOCD_EXECUTABLE=C:/OpenOCD_Artery/bin/openocd.exe
```

换密度芯片时同步改两处：`cmake/openocd/target/at32f435.cfg` 里 source 的文件名，
以及 `CMakePresets.json` 的 `CHIP`（链接脚本按 CHIP 自动选并做容量校验）。

---

## 换型号备忘

订货号 `AT32F435CGT7`：`AT32F435` + 封装码 `C` + 密度码 `G` + 尾缀 `T7`，
**密度字母在倒数第 3 位**（与 GD32F407VG"末位即密度"不同）。
容量码：C=256K / D=384K / G=1024K / M=4032K。
器件宏直接用完整订货号（CMake 传 `-DAT32F435CGT7`，头文件内部再推导 `AT32F435xx`）。
