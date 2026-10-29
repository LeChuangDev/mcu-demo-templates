# STM32F103_Demo

基于 **arm-none-eabi-gcc + CMake + Ninja + OpenOCD** 的 STM32 工程模板。
当前默认目标：**STM32F103C8T6**（64KB Flash / 20KB SRAM，Cortex-M3，无 FPU），
系统时钟 **72MHz**（8MHz HSE → PLL ×9），调试器 **CMSIS-DAP（DAP-Link）**。

设计目标：

- **芯片 / 板级 / 应用三层解耦**——换芯片动 `Core`+`Drivers`，换板子动 `BSP`，业务代码 `App` 一行不改。
- 所有第三方库（StdPeriph、FreeRTOS、LVGL…）**原样放入、永不修改**，差异只通过 `Config/` 和"包装层"吸收。
- 芯片参数（Flash/RAM 大小、内核型号、HSE 频率）只有一个真源：`cmake/chip.cmake` 配合 `CMakePresets.json` 的 `CHIP` 变量自动推导。

---

## 目录结构

```
ST_F103_Demo/
│
├── CMakeLists.txt                # 总装：只做全局选项 + add_subdirectory
├── CMakePresets.json             # 一键配置（CHIP/BOARD/模块开关全在这里）
├── README.md / .gitignore / .clang-format / .editorconfig
│
├── cmake/                        # ── 构建系统（与业务代码零耦合）──
│   ├── arm-none-eabi.cmake       #   工具链 + 全局编译标志（Cortex-M3）
│   ├── chip.cmake                #   ★ CHIP 唯一真源：芯片参数全部自动推导
│   ├── modules.cmake             #   stm32_add_lib / print_size
│   ├── openocd.cmake             #   flash / debug_server / reset
│   └── openocd/{interface,target}/   # 仿真器层 与 芯片层 解耦
│
├── Config/                       # ── 编译期配置集中地 ──
│   ├── stm32f10x_conf.h          #   StdPeriph 裁剪（用官方库时才需要）
│   ├── FreeRTOSConfig.h          #   启用 FreeRTOS 时才有
│   ├── lv_conf.h                 #   启用 LVGL 时才有
│   └── mbedtls_config.h          #   用加密时
│
├── Core/                         # ── 芯片层：换芯片才动 ──
│   ├── Startup/startup_stm32f10x_md.s
│   ├── System/
│   │   ├── system_stm32f10x.c    #   SystemInit / SystemCoreClock
│   │   └── init_stubs.c          #   _init/_fini（-nostartfiles 配套）
│   └── Linker/STM32F103C8Tx_FLASH.ld
│
├── Drivers/                      # ── 芯片层：换芯片才动 ──
│   ├── CMSIS/                    #   CMSIS Core + Device（官方原样）
│   │   ├── Core/Include/         #   core_cm3.h、cmsis_gcc.h …
│   │   └── Device/ST/STM32F1xx/  #   stm32f10x.h / system_stm32f10x.h
│   ├── STM32F10x_StdPeriph_Driver/  # ST 官方库，原样复制，永不修改（可选）
│   └── External/                 # ★ 片外芯片驱动：协议/寄存器层，与板无关
│       ├── ssd1306/  mpu6050/  w25q64/   # 每个芯片 = Inc + Src
│
├── BSP/                          # ★ 板级支持包（模板当前实现）
│   ├── CMakeLists.txt            #   自动收集 Src/*.c，对外暴露 Inc/
│   ├── Inc/bsp.h                 #   ★ App 唯一可见的接口
│   ├── Inc/bsp_bitband.h         #   位带工具（F1 的 GPIO 可以位带）
│   └── Src/
│       ├── bsp_clock.c           #   72MHz：PLL ×9，可重配、带超时、失败回退 HSI
│       ├── bsp_delay.c           #   SysTick 毫秒（WFI 睡眠）+ DWT 微秒
│       └── bsp_led.c             #   板载 LED：只改顶部 4 个宏即可换板
│
├── Middlewares/                  # ★ 第三方中间件：原样放入，只写"包装"，不改源码
│   ├── FreeRTOS/  LVGL/  FatFs/  mbedtls/  micro_ros/
│
├── App/                          # ── 业务层：只写应用逻辑 ──
│   ├── main.c
│   ├── Common/{Inc,Src}/         #   ringbuf、delay、retarget 等通用件
│   └── Modules/                  #   一个目录 = 一个功能模块（led_task/ui/ros_app）
│
├── tools/                        # svd、量产脚本、图片→C数组脚本
├── Doc/                          # 数据手册/原理图（一般不入库）
└── build/{debug,release}/        # 编译产物，gitignore
```

### 依赖方向（只能向下，不能反向）

```
App  ──►  BSP  ──►  Drivers/External(片外芯片)
 │          │
 └──────────┴──►  Core / Drivers(CMSIS、StdPeriph)  ──► 寄存器
                        ▲
        Middlewares（FreeRTOS/LVGL…，通过包装层被 App 调用）
```

- `Drivers/External` 只讲"这个芯片本身怎么操作"（寄存器/协议时序），**不写"它焊在第几路 I2C、CS 接哪个脚"**；引脚绑定一律放 `BSP`。
- 这样同一个 `ssd1306` 驱动换块板子可以直接复用，只在新板的 `bsp_display.c` 里改接线。

---

## 器件配置（F1 特有）

- 器件头使用 **StdPeriph V3.5.1 的 CMSIS Device 头**（`stm32f10x.h`），不是 CubeF1 的 `stm32f1xx.h`。
- `stm32f10x.h` 按**容量等级**选宏，而不是按具体型号：

  | 容量等级 | 器件宏 | Flash 范围 | 例 |
  |---|---|---|---|
  | 小容量 | `STM32F10X_LD` | 16~32K | F103C6 |
  | 中容量 | `STM32F10X_MD` | 64~128K | **F103C8**、F103CB |
  | 大容量 | `STM32F10X_HD` | 256~512K | F103RC、F103VE |
  | 超大容量 | `STM32F10X_XL` | 768K~1M | F103ZE |
  | 互联型 | `STM32F10X_CL` | — | F105/F107 |
  | 超值型 | `STM32F10X_MD_VL` 等 | — | F100 |

  `chip.cmake` 会根据 `CHIP` 的容量位自动选宏和启动文件，例如
  `CHIP=STM32F103C8` → `STM32F10X_MD` + `startup_stm32f10x_md.s`。
- 换芯片只改 `CMakePresets.json` 的 `CHIP`；**同时要放好对应容量的链接脚本**
  （`Core/Linker/STM32F103xxTx_FLASH.ld`），`chip.cmake` 会在配置阶段校验 Flash 容量。

---

## 工具链

| 工具 | 本机路径 |
|---|---|
| arm-none-eabi-gcc 15.x | `C:\EmbeddedTools\arm-gcc\bin` |
| CMake ≥ 3.28 | `C:\EmbeddedTools\cmake\bin` |
| Ninja | `C:\EmbeddedTools\ninja` |
| OpenOCD 0.12 | `C:\EmbeddedTools\openocd-general\bin` |

> 工程文件里不写死任何工具链绝对路径：换电脑时只要把上表四个工具加进 `PATH`，
> 就能直接编译，`cmake/arm-none-eabi.cmake` 和 `CMakePresets.json` 都不用改。

## 移动 / 复制整个工程

把 `ST_F103_Demo/` 整个文件夹复制或移动到任何位置（路径可以含空格、中文），
**不需要修改任何配置**，直接用下面的方式编译/烧录：

- VSCode：`Alt+B` 编译、`Alt+U` 编译并烧录（单手、任意焦点）；也可以按 `Ctrl+Shift+B`
  只编译，或在任务面板选择“编译 Build”“烧录 Flash”（“烧录 Flash”会自动先编译再烧录）。
- 命令行：
  ```bash
  cmake -P tools/build.cmake                    # 编译（debug）
  cmake -DTARGET=flash -P tools/build.cmake     # 编译并烧录
  cmake -DPRESET=release -P tools/build.cmake   # Release 构建
  ```

`tools/build.cmake` 每次构建前都会核对 `build/debug/CMakeCache.txt` 里记录的工程路径：
目录变了就自动删除旧缓存、重新执行 `cmake --preset debug`，然后正常增量编译。
因此 `build/` 可以跟着文件夹一起拷贝，不会因为旧绝对路径而报错。

> 只有绕开该脚本、直接执行 `cmake --build build/debug` 时，移动目录后才需要手动删除 `build/`。

## 时钟 / 延时 / LED 实现要点

- **时钟**：`bsp_clock_init()` 返回 `int`。F1 的时钟树比 G4/F4 简单，
  但有几个硬上限：
  - PLL 只有一级倍频：`8MHz × 9 = 72MHz`，没有 M/N/R 分频；
  - **APB1 最高 36MHz**，所以 72MHz 下 `PPRE1` 必须 ÷2；APB2 可以跑满 72MHz；
  - 72MHz 需要 Flash **2WS** 等待周期并开预取；
  - ADC 时钟上限 14MHz，`ADCPRE` 取 ÷6（12MHz）；USB 需要 48MHz，`USBPRE` 取 ÷1.5。
  函数会先切回 HSI、关闭 PLL 再完整重配，失败时回退 HSI 并更新
  `SystemCoreClock`（返回非 0，程序仍可运行）。
- **毫秒延时**：`bsp_delay_ms()` 在等待期间执行 `__WFI()`，CPU 睡眠而不是空转。
- **微秒延时**：`bsp_delay_us()` 基于 DWT->CYCCNT，内部按 30 亿周期分块，
  超过 59s 也不会因乘法溢出/计数器回绕出错。
- **LED**：App 只调用 `bsp_led_init/on/off/toggle()`；换板子改
  [BSP/Src/bsp_led.c](BSP/Src/bsp_led.c) 顶部的 `LED_GPIO / LED_PIN / LED_RCC_EN / LED_ACTIVE_LEVEL`。
  默认是 Blue Pill 的 **PC13、低电平点亮**。F1 的 GPIO 配置用
  `CRL/CRH`（每个引脚 4 位：MODE 2 位 + CNF 2 位），不是 G4/F4 的 `MODER/OTYPER/...`；
  输出仍用 `GPIOx->BSRR`，天然原子。F1 的 GPIO 在 `0x4001_0800`，位于位带区内，
  需要时也可以用 `bsp_bitband.h`。

## 常用命令（CMake 文件就绪后）

```bash
# Debug（默认）
cmake --preset debug
cmake --build build/debug

# Release（-Os + LTO + newlib-nano，体积更小）
cmake --preset release
cmake --build build/release

# 烧录 / 复位 / 起 GDB Server（Debug）
cmake --build build/debug --target flash
cmake --build build/debug --target reset
cmake --build build/debug --target debug_server
```

也可以直接用统一入口，预设名由 `-DPRESET` 指定（缺省 `debug`）：

```bash
cmake -DPRESET=release -P tools/build.cmake
```

切换芯片或板子只改 `CMakePresets.json` 里的 `CHIP`，不动任何 CMakeLists。
