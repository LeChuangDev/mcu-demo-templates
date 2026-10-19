# STM32F407_Demo

基于 **arm-none-eabi-gcc + CMake + Ninja + OpenOCD** 的 STM32 工程模板。
当前默认目标：**STM32F407VGT6**（1MB Flash / 128KB SRAM + 64KB CCM），调试器 **CMSIS-DAP（DAP-Link）**。

设计目标：

- **芯片 / 板级 / 应用三层解耦**——换芯片动 `Core`+`Drivers`，换板子动 `BSP`，业务代码 `App` 一行不改。
- 所有第三方库（HAL、FreeRTOS、LVGL…）**原样放入、永不修改**，差异只通过 `Config/` 和"包装层"吸收。
- 芯片参数（Flash/RAM 大小、内核型号、HSE 频率）只有一个真源：`cmake/chip.cmake` 配合 `CMakePresets.json` 的 `CHIP` 变量自动推导。

---

## 目录结构

```
STM32F407_Demo/
│
├── CMakeLists.txt                # 总装：只做全局选项 + add_subdirectory
├── CMakePresets.json             # 一键配置（CHIP/BOARD/模块开关全在这里）
├── README.md / .gitignore / .clang-format / .editorconfig
│
├── cmake/                        # ── 构建系统（与业务代码零耦合）──
│   ├── arm-none-eabi.cmake       #   工具链 + 全局编译标志
│   ├── chip.cmake                #   ★ CHIP 唯一真源：芯片参数全部自动推导
│   ├── modules.cmake             #   stm32_add_lib / stm32_add_exe / print_size
│   ├── openocd.cmake             #   flash / debug_server / verify / unlock
│   └── openocd/{interface,target}/   # 仿真器层 与 芯片层 解耦
│
├── Config/                       # ── 编译期配置集中地 ──
│   ├── stm32f4xx_hal_conf.h      #   HAL 裁剪（HSE_VALUE、模块开关）
│   ├── FreeRTOSConfig.h          #   启用 FreeRTOS 时才有
│   ├── lv_conf.h                 #   启用 LVGL 时才有
│   ├── mbedtls_config.h          #   用加密时
│   └── boards/                   #   可选：纯宏配置的板级差异也放这里
│
├── Core/                         # ── 芯片层：换芯片才动 ──
│   ├── Startup/startup_stm32f407xx.s
│   ├── System/system_stm32f4xx.c
│   └── Linker/STM32F407VGTx_FLASH.ld
│
├── Drivers/                      # ── 芯片层：换芯片才动 ──
│   ├── CMSIS/                    #   CMSIS Core + Device（官方原样）
│   ├── STM32F4xx_HAL_Driver/     #   ST 官方库，原样复制，永不修改
│   └── External/                 # ★ 片外芯片驱动：协议/寄存器层，与板无关
│       ├── ssd1306/  mpu6050/  w25q64/   # 每个芯片 = Inc + Src
│
├── BSP/                          # ★ 板级支持包：一块板 = 一个目录
│   ├── CMakeLists.txt            #   按 BOARD 变量选择哪个板目录
│   ├── Inc/bsp.h                 #   ★ 所有板卡必须实现的统一接口契约
│   ├── MyBoard_407/              #   板 A：自己画的板
│   └── NUCLEO_F407ZG/            #   板 B：官方板（换板 = 切 BOARD 变量）
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
 └──────────┴──►  Core / Drivers(CMSIS、HAL)  ──►  寄存器
                        ▲
        Middlewares（FreeRTOS/LVGL…，通过包装层被 App 调用）
```

- `Drivers/External` 只讲"这个芯片本身怎么操作"（寄存器/协议时序），**不写"它焊在第几路 I2C、CS 接哪个脚"**；引脚绑定一律放 `BSP`。
- 这样同一个 `ssd1306` 驱动换块板子可以直接复用，只在新板的 `bsp_display.c` 里改接线。

---

## 工具链

| 工具 | 本机路径 |
|---|---|
| arm-none-eabi-gcc 15.x | `C:\EmbeddedTools\arm-gcc\bin` |
| CMake ≥ 3.28 | `C:\EmbeddedTools\cmake\bin` |
| Ninja | `C:\EmbeddedTools\ninja` |
| OpenOCD 0.12 | `C:\EmbeddedTools\openocd-general\bin` |

> 换电脑时只需修改 `cmake/arm-none-eabi.cmake`（或 `CMakePresets.json` 中的工具链路径），其他文件不动。

## 常用命令（CMake 文件就绪后）

```bash
# 配置（debug / release 两种预设，二进制目录隔离）
cmake --preset debug
cmake --preset release

# 编译
cmake --build build/debug
cmake --build build/release

# 烧录 / 校验 / 解除读保护 / 起 GDB Server
cmake --build build/debug --target flash
cmake --build build/debug --target verify
cmake --build build/debug --target unlock
cmake --build build/debug --target debug_server
```

切换芯片或板子只改 `CMakePresets.json` 里的 `-DCHIP=... -DBOARD=...`，不动任何 CMakeLists。
