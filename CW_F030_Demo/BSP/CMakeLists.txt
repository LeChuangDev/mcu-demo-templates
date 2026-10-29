# =====================================================================
#  BSP 层：时钟 + 延时
# =====================================================================

# 第 1 步：CMSIS 接口库——不编译任何源码，只把头文件路径和器件宏传播出去
add_library(cmsis INTERFACE)
target_include_directories(cmsis SYSTEM INTERFACE
    ${CMSIS_CORE_INC}          # core_cm0plus.h / cmsis_gcc.h 等内核头
    ${CMSIS_DEV_INC}           # cw32f030.h 器件头(基址/IRQn)
    ${CW32_SPL_INC}            # CW32 寄存器结构体头(gpio/rcc/flash/systick)
    ${CW32_CONF_INC}           # 裁剪版 cw32f030_conf.h
    ${CMSIS_SYS_INC})          # system_cw32f030.h(器件头内部 include)
target_compile_definitions(cmsis INTERFACE ${DEVICE_DEF})

# 第 2 步：BSP 静态库（自动收集 Src/*.c，自动暴露 Inc/）
mcu_add_lib(bsp)

# 第 3 步：依赖关系：bsp -> cmsis
#   PUBLIC 的含义：谁链接了 bsp，就自动获得 cmsis 的头文件路径和宏
target_link_libraries(bsp PUBLIC cmsis)

# 第 4 步：板载晶振频率 ★ 按你板子上实际焊的晶振改 ★
#   立创·地文星板载 Y1 = 8MHz（HSE）。
#   官方 cw32f030_rcc.h 里 HSE_VALUE 默认 16MHz，这里有 #if !defined 保护，
#   命令行传值即可覆盖，并传播给所有依赖方——包括直接编进固件、
#   内部会用到 HSE_VALUE 的官方 system_cw32f030.c。
target_compile_definitions(bsp PUBLIC HSE_VALUE=${BOARD_HSE_VALUE})
