# =====================================================================
#  BSP 层：时钟 + 延时 + 位带
# =====================================================================

# 第 1 步：CMSIS 接口库——不编译任何源码，只把头文件路径和器件宏传播出去
add_library(cmsis INTERFACE)
target_include_directories(cmsis SYSTEM INTERFACE
    ${CMSIS_CORE_INC}          # core_cm7.h 等内核头
    ${CMSIS_DEV_INC})          # stm32h7xx.h 寄存器定义
target_compile_definitions(cmsis INTERFACE ${DEVICE_DEF})

# 第 2 步：BSP 静态库（自动收集 Src/*.c，自动暴露 Inc/）
stm32_add_lib(bsp)

# 第 3 步：依赖关系：bsp -> cmsis
#   PUBLIC 的含义：谁链接了 bsp，就自动获得 cmsis 的头文件路径和宏
target_link_libraries(bsp PUBLIC cmsis)

# 第 4 步：板载晶振频率 ★ 按你板子上实际焊的晶振改 ★
#   当前模板按 25MHz 配置（WeAct H743 / 多数 H743 核心板常见）；
#   NUCLEO-H743ZI 等用 8MHz 时改成 8000000U，并同步修改 bsp_clock.c 的 PLLM。
#   PUBLIC 会把这个编译定义传播给所有依赖方——
#   包括直接编进可执行文件的 system_stm32h7xx.c
#   (SystemCoreClock 的换算需要它，填错会导致 delay_us 时间不准)
target_compile_definitions(bsp PUBLIC HSE_VALUE=25000000U)

# 第 5 步：电源方案 ★ STM32H743 用片内 LDO 供电（H743 没有 SMPS）★
#   system_stm32h7xx.c 里的 ExitRun0Mode()（启动文件在 SystemInit 之前调用）
#   靠这个宏决定上电后如何写 PWR->CR3。不定义时该函数体为空；定义后它会
#   打开 LDO 并等 ACTVOSRDY，和 CubeMX 生成的工程行为一致。
#   若以后换带 SMPS 的 H7（H7A3/H750 等），改成 PWR_SMPS_1V8_SUPPLIES_LDO 等。
target_compile_definitions(bsp PUBLIC USE_PWR_LDO_SUPPLY)