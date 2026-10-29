# =====================================================================
#  BSP 层：时钟 + 延时 + LED
# =====================================================================

# 第 1 步：CMSIS 接口库——不编译任何源码，只把头文件路径和器件宏传播出去
add_library(cmsis INTERFACE)
target_include_directories(cmsis SYSTEM INTERFACE
    ${CMSIS_CORE_INC}          # core_cm23.h / cmsis_gcc.h 等内核头
    ${CMSIS_DEV_INC}           # gd32e23x.h 器件头(基址/IRQn)
    ${GD32_SPL_INC})           # GD 寄存器定义头(gd32e23x_rcu.h 等 + gd32e23x_libopt.h)
target_compile_definitions(cmsis INTERFACE ${DEVICE_DEF})

# 第 2 步：BSP 静态库（自动收集 Src/*.c，自动暴露 Inc/）
gd32_add_lib(bsp)

# 第 3 步：依赖关系：bsp -> cmsis
#   PUBLIC 的含义：谁链接了 bsp，就自动获得 cmsis 的头文件路径和宏
target_link_libraries(bsp PUBLIC cmsis)

# 第 4 步：板载晶振频率 ★ 按你板子上实际焊的晶振改 ★
#   立创·GD32E230C8T6 开发板板载 8MHz 晶振；部分最小系统板也是 8MHz。
#   宏名是 HXTAL_VALUE（不是 ST 的 HSE_VALUE），gd32e23x.h 与
#   system_gd32e23x.c 都依赖它，填错会导致 SystemInit 等错晶振起振超时、
#   SystemCoreClock 换算和 delay 时间全错。
#   PUBLIC 会把这个编译定义传播给所有依赖方——
#   包括下面直接编进可执行文件的 system_gd32e23x.c。
target_compile_definitions(bsp PUBLIC HXTAL_VALUE=8000000U)
