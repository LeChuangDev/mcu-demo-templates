# =====================================================================
#  chip.cmake —— 芯片型号的"唯一真源"
#  换芯片时：改下面 CHIP 一行(同系列)，其余路径/宏全部自动推导
#  换内核系列时：改这一行 + 改 arm-none-eabi.cmake 里的 CPU_FLAGS
#
#  本工程器件头是 GD 官方固件库 V3.3.3 的 gd32f4xx.h，
#  它用的器件宏是 GD32F405 / GD32F407 / GD32F425 / GD32F427 /
#  GD32F450 / GD32F470（每个具体型号一个宏，不是 ST 那种分组宏）。
#
#  ★正则注意★ CMake 自带正则【不支持】{2}/{3} 这种有界重复写法
#    (实测 string(REGEX ...) 里写 [0-9]{3} 会静默匹配失败、返回原串)，
#    所以下面全部老老实实把字符类写三遍。
# =====================================================================

# 第 1 步：芯片型号（完整订货号去掉温度/封装后缀，大写）
#   GD32F407VGT6 -> GD32F407VG
set(CHIP "GD32F407VG" CACHE STRING "芯片型号")

# 第 2 步：拆型号
#   GD32F407VG 拆成: 系列字母 F、三位型号 407、容量+封装 VG
string(REGEX REPLACE "^GD32([FGHLCUW])([0-9][0-9][0-9]).*$" "\\1" CHIP_FAM_LETTER "${CHIP}")
string(REGEX REPLACE "^GD32([FGHLCUW])([0-9][0-9][0-9]).*$" "\\2" CHIP_DEV_CODE   "${CHIP}")

# 拼目录用的系列名: 系列字母 + 型号首位数字 -> F4 / F1 / F7 / H7
string(SUBSTRING "${CHIP_DEV_CODE}" 0 1 CHIP_FAM_DIGIT)
set(GD32_FAMILY    "GD32${CHIP_FAM_LETTER}${CHIP_FAM_DIGIT}")   # -> GD32F4
string(TOLOWER "${GD32_FAMILY}" GD32_FAMILY_LC)                  # -> gd32f4

# 第 3 步：器件宏 + 官方启动文件基名 映射（按 GD32F4xx 固件库 V3.3.3）
#   选错宏 gd32f4xx.h 会直接 #error "Please select the target GD32F4xx device"
if(CHIP_DEV_CODE STREQUAL "405")
    set(DEVICE_DEF   "GD32F405")
    set(_startup_base "gd32f405_425")   # 官方 GCC 启动文件 startup_gd32f405_427.S
elseif(CHIP_DEV_CODE STREQUAL "407")
    set(DEVICE_DEF   "GD32F407")
    set(_startup_base "gd32f407_427")   # 官方 GCC 启动文件 startup_gd32f407_427.S
elseif(CHIP_DEV_CODE STREQUAL "425")
    set(DEVICE_DEF   "GD32F425")
    set(_startup_base "gd32f405_425")
elseif(CHIP_DEV_CODE STREQUAL "427")
    set(DEVICE_DEF   "GD32F427")
    set(_startup_base "gd32f407_427")
elseif(CHIP_DEV_CODE STREQUAL "450")
    set(DEVICE_DEF   "GD32F450")
    set(_startup_base "gd32f450")
elseif(CHIP_DEV_CODE STREQUAL "470")
    set(DEVICE_DEF   "GD32F470")
    set(_startup_base "gd32f470")
else()
    message(FATAL_ERROR
        "无法把型号 ${CHIP}(三位型号=${CHIP_DEV_CODE})映射到 GD32F4xx 器件宏。\n"
        "GD32F4 系列仅支持 405/407/425/427/450/470，"
        "其他型号请在 chip.cmake 第 3 步补充。")
endif()

# 第 4 步：头文件目录（纯头文件，不编译库函数源码）
#   CMSIS_DEV_INC : gd32f4xx.h 器件头（基址/IRQn/内核配置）
#   GD32_SPL_INC  : GD 官方标准外设库的寄存器定义头
#                   (gd32f4xx_rcu.h / _gpio.h 等，仅 REG32 寄存器映射+位宏)
#                   gd32f4xx.h 末尾强制 include 的 gd32f4xx_libopt.h 也在这里
set(CMSIS_CORE_INC "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Core/Include")
set(CMSIS_DEV_INC  "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/GD/GD32F4xx/Include")
set(GD32_SPL_INC   "${CMAKE_SOURCE_DIR}/Drivers/GD32F4xx_StdPeriph/Include")

# 第 5 步：找启动文件
# 策略：
#   1) 先按精确候选名找（本工程放的是 GD 官方 GCC 启动文件，.S 大写后缀，
#      会被 gcc 当汇编(带预处理)处理）
#   2) 都找不到时，若 Core/Startup/ 下恰好只有一个启动文件，就直接用它
set(STARTUP_FILE "")
set(_startup_candidates
    "${CMAKE_SOURCE_DIR}/Core/Startup/startup_${_startup_base}.S"   # GD 官方 GCC 名(大写)
    "${CMAKE_SOURCE_DIR}/Core/Startup/startup_${_startup_base}.s")  # 小写后缀兜底
foreach(f ${_startup_candidates})
    if(EXISTS "${f}")
        set(STARTUP_FILE "${f}")
        break()
    endif()
endforeach()

# 兜底：目录里只有一个启动文件就认定是它（自动识别，不用管文件名）
if(NOT STARTUP_FILE)
    file(GLOB _startup_globs
         "${CMAKE_SOURCE_DIR}/Core/Startup/*.s"
         "${CMAKE_SOURCE_DIR}/Core/Startup/*.S")
    list(LENGTH _startup_globs _startup_count
    )
    if(_startup_count EQUAL 1)
        set(STARTUP_FILE "${_startup_globs}")
    elseif(_startup_count GREATER 1)
        message(FATAL_ERROR
            "Core/Startup/ 下有 ${_startup_count} 个启动文件，无法自动判断。\n"
            "该目录只能放一个启动文件，或把多余文件移出。")
    endif()
endif()

# 第 6 步：找链接脚本（Flash/RAM 容量必须和芯片型号一致！
#   VG=1MB Flash，VE=512KB，用错轻则链接溢出，重则能编译但程序跑飞）
set(_ld_candidates
    "${CMAKE_SOURCE_DIR}/Core/Linker/${CHIP}Tx_FLASH.ld"
    "${CMAKE_SOURCE_DIR}/Core/Linker/${CHIP}x_FLASH.ld"
)
foreach(f ${_ld_candidates})
    if(EXISTS "${f}")
        set(LDSCRIPT "${f}")
        break()
    endif()
endforeach()

# 第 6.1 步：从型号推导 Flash 容量
#   GD32/STM32 订货号最后一个字母就是容量代码：
#   "GD32F407VG" -> 末位 'G' -> 1024K
string(LENGTH "${CHIP}" _chip_len)
if(_chip_len GREATER 1)
    math(EXPR _flash_idx "${_chip_len} - 1")
    string(SUBSTRING "${CHIP}" ${_flash_idx} 1 _flash_letter)
endif()
set(_flash_map
    "4:16" "6:32" "8:64" "B:128" "C:256" "D:384"
    "E:512" "F:768" "G:1024" "I:2048")
foreach(pair ${_flash_map})
    string(REPLACE ":" ";" kv "${pair}")
    list(GET kv 0 k)
    list(GET kv 1 v)
    if("${k}" STREQUAL "${_flash_letter}")
        set(CHIP_FLASH_KB "${v}")
    endif()
endforeach()
if(NOT CHIP_FLASH_KB)
    message(WARNING "无法从型号 ${CHIP} 推导 Flash 容量，跳过容量校验")
endif()

# 第 6.2 步：自动校验链接脚本的 Flash 容量与型号推导值一致
#   VE 芯片配 VG 的 ld 能编译能烧录但会跑飞，这个检查把它扼杀在配置阶段
#   注意：用 Flash 源地址 0x08 做锚点而不是 "FLASH (rx).*LENGTH"——
#   CMake 正则在贪婪 .* 后接捕获组时取到的 CMAKE_MATCH_1 会是空串（实测坑）
if(CHIP_FLASH_KB AND LDSCRIPT)
    file(READ "${LDSCRIPT}" _ld_text)
    string(REGEX MATCH "0x08[0-9A-Fa-fx, =]*LENGTH *= *([0-9]+)K" _m "${_ld_text}")
    if(CMAKE_MATCH_1)
        if(NOT "${CMAKE_MATCH_1}" STREQUAL "${CHIP_FLASH_KB}")
            message(FATAL_ERROR
                "\n链接脚本容量与型号不符!\n"
                "  ${CHIP} 推导 Flash = ${CHIP_FLASH_KB}K\n"
                "  ${LDSCRIPT} 实际 LENGTH = ${CMAKE_MATCH_1}K\n"
                "请换正确容量的链接脚本")
        endif()
        message(STATUS ">>> Flash 容量校验通过: ${CHIP_FLASH_KB}K")
    else()
        message(WARNING "未能从链接脚本解析出 FLASH LENGTH，跳过校验")
    endif()
endif()

# 第 7 步：存在性检查——缺文件时给出"去哪找"的提示，而不是报 CMake 内部错误
if(NOT EXISTS "${CMSIS_DEV_INC}/${GD32_FAMILY_LC}xx.h")
    message(FATAL_ERROR
        "\n缺少器件头文件 ${GD32_FAMILY_LC}xx.h\n"
        "期望目录: ${CMSIS_DEV_INC}\n"
        "来源: GigaDevice 官方 GD32F4xx_Firmware_Library 的 "
        "Firmware/CMSIS/GD/GD32F4xx/Include/\n"
        "仓库: https://github.com/GigaDevice-GD32-MCU/GD32F4xx_Firmware_Library")
endif()
if(NOT EXISTS "${GD32_SPL_INC}/gd32f4xx_libopt.h")
    message(FATAL_ERROR
        "\n缺少 gd32f4xx_libopt.h（gd32f4xx.h 会强制 include 它）\n"
        "期望目录: ${GD32_SPL_INC}\n"
        "来源: 官方固件库的 GD32F4xx_standard_peripheral/Include 寄存器头，"
        "libopt.h 可按本工程只保留用到外设的裁剪版")
endif()
if(NOT STARTUP_FILE)
    message(FATAL_ERROR "找不到启动文件，候选: ${_startup_candidates}")
endif()
if(NOT LDSCRIPT)
    message(FATAL_ERROR "找不到链接脚本，候选: ${_ld_candidates}")
endif()

# 第 8 步：OpenOCD 配置选择（仿真器层 与 芯片层 解耦）
#   换仿真器：改 interface（cmake/openocd/interface 下的文件名）
#   换芯片：target 按系列自动映射到 cmake/openocd/target 下的【包装文件】，
#           包装文件内部再 source OpenOCD 自带的标准 target 脚本
#   说明：OpenOCD 官方暂无 GD32F4 专用 target，gd32f4.cfg 复用 stm32f4x.cfg
#         （两者 SWD/Flash 控制器兼容），并设 CPUTAPID=0 跳过 ID 校验
set(OPENOCD_INTERFACE_CFG "cmsis-dap.cfg" CACHE STRING
    "仿真器接口配置文件名（cmake/openocd/interface/ 下），如 cmsis-dap.cfg/stlink.cfg/jlink.cfg")
set(_ocd_tgt_map
    "GD32F1:gd32f1.cfg"
    "GD32F4:gd32f4.cfg"
    "GD32F7:gd32f7.cfg"
    "GD32H7:gd32h7.cfg"
    "GD32G4:gd32g4.cfg"
    "GD32L4:gd32l4.cfg")
set(OPENOCD_TARGET_CFG "" CACHE STRING "芯片 target 包装配置文件名（cmake/openocd/target/ 下）")
foreach(pair ${_ocd_tgt_map})
    string(REPLACE ":" ";" kv "${pair}")
    list(GET kv 0 k)
    list(GET kv 1 v)
    if("${k}" STREQUAL "${GD32_FAMILY}")
        set(OPENOCD_TARGET_CFG "${v}")
    endif()
endforeach()
if(NOT OPENOCD_TARGET_CFG)
    message(FATAL_ERROR
        "系列 ${GD32_FAMILY} 还没有 OpenOCD target 包装文件。\n"
        "请在 cmake/openocd/target/ 下新建一个 .cfg（可仿照 gd32f4.cfg，\n"
        "内部 source [find target/xxxx.cfg]），并在本映射表登记。")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cmake/openocd/interface/${OPENOCD_INTERFACE_CFG}")
    message(FATAL_ERROR "找不到 OpenOCD 接口配置: cmake/openocd/interface/${OPENOCD_INTERFACE_CFG}")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cmake/openocd/target/${OPENOCD_TARGET_CFG}")
    message(FATAL_ERROR "找不到 OpenOCD target 配置: cmake/openocd/target/${OPENOCD_TARGET_CFG}")
endif()

# 第 9 步：打印配置摘要（配置阶段一眼确认芯片没选错）
message(STATUS ">>> 芯片: ${CHIP}  器件宏: ${DEVICE_DEF}  系列: ${GD32_FAMILY}  Flash: ${CHIP_FLASH_KB}K")
message(STATUS ">>> CMSIS Core : ${CMSIS_CORE_INC}")
message(STATUS ">>> CMSIS Device: ${CMSIS_DEV_INC}")
message(STATUS ">>> GD SPL 头  : ${GD32_SPL_INC}")
message(STATUS ">>> 启动文件: ${STARTUP_FILE}")
message(STATUS ">>> 链接脚本: ${LDSCRIPT}")
message(STATUS ">>> OpenOCD: ${OPENOCD_INTERFACE_CFG} + ${OPENOCD_TARGET_CFG}")
