# =====================================================================
#  chip.cmake —— 芯片型号的"唯一真源"
#  换芯片时：改下面 CHIP 一行(同系列)，其余路径/宏全部自动推导
#  换内核系列时：改这一行 + 改 arm-none-eabi.cmake 里的 CPU_FLAGS
#
#  ★注意★ 本工程的器件头是【老版标准外设库 SPL V1.8.1】的 stm32f4xx.h，
#    它用的器件宏是 STM32F40_41xxx 这种"一组芯片共用一个宏"的命名；
#    若以后改用 CubeF4 新版器件头(stm32f407xx.h 时代)，宏名是 STM32F407xx，
#    只需在本文件第 3 步的映射表里加一个分支，其他文件不用动。
#
#  ★正则注意★ CMake 自带正则【不支持】{2}/{3} 这种有界重复写法
#    (实测 string(REGEX ...) 里写 [0-9]{3} 会静默匹配失败、返回原串)，
#    所以下面全部老老实实把字符类写三遍。
# =====================================================================

# 第 1 步：芯片型号（完整订货号去掉温度/封装后缀，大写）
#   STM32F407VGT6 -> STM32F407VG
set(CHIP "STM32F407VG" CACHE STRING "芯片型号")

# 第 2 步：拆型号
#   STM32F407VG 拆成: 系列字母 F、三位型号 407、容量+封装 VG
string(REGEX REPLACE "^STM32([FGHLCUW])([0-9][0-9][0-9]).*$" "\\1" CHIP_FAM_LETTER "${CHIP}")
string(REGEX REPLACE "^STM32([FGHLCUW])([0-9][0-9][0-9]).*$" "\\2" CHIP_DEV_CODE   "${CHIP}")

# 拼目录用的系列名: 系列字母 + 型号首位数字 -> F4 / F1 / F7 / H7
string(SUBSTRING "${CHIP_DEV_CODE}" 0 1 CHIP_FAM_DIGIT)
set(STM32_FAMILY    "STM32${CHIP_FAM_LETTER}${CHIP_FAM_DIGIT}")   # -> STM32F4
string(TOLOWER "${STM32_FAMILY}" STM32_FAMILY_LC)                  # -> stm32f4

# 第 3 步：器件宏映射（当前按 SPL V1.8.1 头文件支持的分组）
#   选错宏 stm32f4xx.h 会直接 #error "Please select first the target ..."
if(CHIP_DEV_CODE STREQUAL "405" OR CHIP_DEV_CODE STREQUAL "407"
   OR CHIP_DEV_CODE STREQUAL "415" OR CHIP_DEV_CODE STREQUAL "417")
    set(DEVICE_DEF "STM32F40_41xxx")          # F405/F407/F415/F417 共用
elseif(CHIP_DEV_CODE STREQUAL "427" OR CHIP_DEV_CODE STREQUAL "437")
    set(DEVICE_DEF "STM32F427_437xx")
elseif(CHIP_DEV_CODE STREQUAL "429" OR CHIP_DEV_CODE STREQUAL "439")
    set(DEVICE_DEF "STM32F429_439xx")
elseif(CHIP_DEV_CODE STREQUAL "401")
    set(DEVICE_DEF "STM32F401xx")
elseif(CHIP_DEV_CODE STREQUAL "410")
    set(DEVICE_DEF "STM32F410xx")
elseif(CHIP_DEV_CODE STREQUAL "411")
    set(DEVICE_DEF "STM32F411xE")
elseif(CHIP_DEV_CODE STREQUAL "412")
    set(DEVICE_DEF "STM32F412xG")
elseif(CHIP_DEV_CODE STREQUAL "413" OR CHIP_DEV_CODE STREQUAL "423")
    set(DEVICE_DEF "STM32F413_423xx")
elseif(CHIP_DEV_CODE STREQUAL "446")
    set(DEVICE_DEF "STM32F446xx")
elseif(CHIP_DEV_CODE STREQUAL "469" OR CHIP_DEV_CODE STREQUAL "479")
    set(DEVICE_DEF "STM32F469_479xx")
else()
    message(FATAL_ERROR
        "无法把型号 ${CHIP}(三位型号=${CHIP_DEV_CODE})映射到 SPL 器件宏。\n"
        "请在 chip.cmake 第 3 步补充该型号对应的宏。")
endif()

# 第 4 步：CMSIS 头文件目录（纯头文件，不编译）
#   目录名就是 STM32F4xx，注意不要再拼一个 STM32 前缀（曾踩坑：STM32STM32F4xx）
set(CMSIS_CORE_INC "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Core/Include")
set(CMSIS_DEV_INC  "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/${STM32_FAMILY}xx/Include")

# 第 5 步：找启动文件
# 策略：
#   1) 先按精确候选名找（官方模板名 / CubeMX 风格名 / SPL 共用名）
#   2) 都找不到时，若 Core/Startup/ 下恰好只有一个 .s 文件，就直接用它
#      （本工程就是 startup_stm32f40_41xxx.s 这种 SPL 共用名）
string(TOLOWER "${CHIP}" CHIP_LC)

set(_startup_candidates
    "${CMAKE_SOURCE_DIR}/Core/Startup/startup_${CHIP_LC}xx.s"                     # Cube 新名
    "${CMAKE_SOURCE_DIR}/Core/Startup/startup_${CHIP_LC}tx.s"                     # CubeMX 风格名
    "${CMAKE_SOURCE_DIR}/Core/Startup/startup_${STM32_FAMILY_LC}0_41xxx.s")       # ★ SPL 共用名
set(STARTUP_FILE "")
foreach(f ${_startup_candidates})
    if(EXISTS "${f}")
        set(STARTUP_FILE "${f}")
        break()
    endif()
endforeach()

# 兜底：目录里只有一个 .s 就认定是它（自动识别，不用管文件名）
if(NOT STARTUP_FILE)
    file(GLOB _startup_globs
         "${CMAKE_SOURCE_DIR}/Core/Startup/*.s"
         "${CMAKE_SOURCE_DIR}/Core/Startup/*.S")
    list(LENGTH _startup_globs _startup_count)
    if(_startup_count EQUAL 1)
        set(STARTUP_FILE "${_startup_globs}")
    elseif(_startup_count GREATER 1)
        message(FATAL_ERROR
            "Core/Startup/ 下有 ${_startup_count} 个启动文件，无法自动判断。\n"
            "该目录只能放一个 .s 文件，或把多余文件移出。")
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

# 第 6.1 步：从型号推导 Flash 容量（F4 第 10 号字符(从 0 数)是容量代码）
#   "STM32F407VG" -> 下标 10 = 'G' -> 1024K
string(LENGTH "${CHIP}" _chip_len)
if(_chip_len GREATER_EQUAL 11)
    string(SUBSTRING "${CHIP}" 10 1 _flash_letter)
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
    message(WARNING "无法从型号 ${CHIP} 推导 Flash 容量（非 F4 命名规则），跳过容量校验")
endif()

# 第 6.2 步：自动校验链接脚本的 Flash 容量与型号推导值一致
#   VE 芯片配 VG 的 ld 能编译能烧录但会跑飞，这个检查把它扼杀在配置阶段
#   注意：. 不匹配换行，FLASH 行本身是同一行，这样能避免误抓到 CCMRAM 的 64K
if(CHIP_FLASH_KB AND LDSCRIPT)
    file(READ "${LDSCRIPT}" _ld_text)
    string(REGEX MATCH "FLASH \\(rx\\).*LENGTH[ \t]*=[ \t]*([0-9]+)K" _m "${_ld_text}")
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
if(NOT EXISTS "${CMSIS_DEV_INC}/${STM32_FAMILY_LC}xx.h")
    message(FATAL_ERROR
        "\n缺少器件头文件 ${STM32_FAMILY_LC}xx.h\n"
        "期望目录: ${CMSIS_DEV_INC}\n"
        "老版 SPL: STM32F4xx_DSP_StdPeriph_Lib 的 Libraries/CMSIS/Device/ST/STM32F4xx/Include/\n"
        "新版 Cube: https://github.com/STMicroelectronics/cmsis-device-${STM32_FAMILY_LC} 的 Include/")
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
set(OPENOCD_INTERFACE_CFG "cmsis-dap.cfg" CACHE STRING
    "仿真器接口配置文件名（cmake/openocd/interface/ 下），如 cmsis-dap.cfg/stlink.cfg/jlink.cfg")
set(_ocd_tgt_map
    "STM32F1:stm32f1.cfg"
    "STM32F4:stm32f4.cfg"
    "STM32F7:stm32f7.cfg"
    "STM32H7:stm32h7.cfg"
    "STM32G4:stm32g4.cfg"
    "STM32L4:stm32l4.cfg")
set(OPENOCD_TARGET_CFG "" CACHE STRING "芯片 target 包装配置文件名（cmake/openocd/target/ 下）")
foreach(pair ${_ocd_tgt_map})
    string(REPLACE ":" ";" kv "${pair}")
    list(GET kv 0 k)
    list(GET kv 1 v)
    if("${k}" STREQUAL "${STM32_FAMILY}")
        set(OPENOCD_TARGET_CFG "${v}")
    endif()
endforeach()
if(NOT OPENOCD_TARGET_CFG)
    message(FATAL_ERROR
        "系列 ${STM32_FAMILY} 还没有 OpenOCD target 包装文件。\n"
        "请在 cmake/openocd/target/ 下新建一个 .cfg（可仿照 stm32f4.cfg，\n"
        "内部 source [find target/xxxx.cfg]），并在本映射表登记。")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cmake/openocd/interface/${OPENOCD_INTERFACE_CFG}")
    message(FATAL_ERROR "找不到 OpenOCD 接口配置: cmake/openocd/interface/${OPENOCD_INTERFACE_CFG}")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/cmake/openocd/target/${OPENOCD_TARGET_CFG}")
    message(FATAL_ERROR "找不到 OpenOCD target 配置: cmake/openocd/target/${OPENOCD_TARGET_CFG}")
endif()

# 第 9 步：打印配置摘要（配置阶段一眼确认芯片没选错）
message(STATUS ">>> 芯片: ${CHIP}  器件宏: ${DEVICE_DEF}  系列: ${STM32_FAMILY}  Flash: ${CHIP_FLASH_KB}K")
message(STATUS ">>> CMSIS Core : ${CMSIS_CORE_INC}")
message(STATUS ">>> CMSIS Device: ${CMSIS_DEV_INC}")
message(STATUS ">>> 启动文件: ${STARTUP_FILE}")
message(STATUS ">>> 链接脚本: ${LDSCRIPT}")
message(STATUS ">>> OpenOCD: ${OPENOCD_INTERFACE_CFG} + ${OPENOCD_TARGET_CFG}")
