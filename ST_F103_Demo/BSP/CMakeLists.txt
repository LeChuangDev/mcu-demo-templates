# =====================================================================
#  BSP 层：时钟 + 延时 + 位带
# =====================================================================

# 第 1 步：CMSIS 接口库——不编译任何源码，只把头文件路径和器件宏传播出去
add_library(cmsis INTERFACE)
target_include_directories(cmsis SYSTEM INTERFACE
    ${CMSIS_CORE_INC}          # core_cm3.h 等内核头
    ${CMSIS_DEV_INC})          # stm32f10x.h 寄存器定义
target_compile_definitions(cmsis INTERFACE ${DEVICE_DEF})

# 第 2 步：BSP 静态库（自动收集 Src/*.c，自动暴露 Inc/）
stm32_add_lib(bsp)

# 第 3 步：依赖关系：bsp -> cmsis
#   PUBLIC 的含义：谁链接了 bsp，就自动获得 cmsis 的头文件路径和宏
target_link_libraries(bsp PUBLIC cmsis)

# 第 4 步：板载晶振频率 ★ 按你板子上实际焊的晶振改 ★
#   当前模板按 8MHz 配置（Blue Pill / 最小系统板最常见）。
#   PUBLIC 会把这个编译定义传播给所有依赖方——
#   包括下面直接编进可执行文件的 system_stm32f10x.c
#   (SystemCoreClock 的换算需要它，填错会导致 delay_us 时间不准)
target_compile_definitions(bsp PUBLIC HSE_VALUE=8000000U)