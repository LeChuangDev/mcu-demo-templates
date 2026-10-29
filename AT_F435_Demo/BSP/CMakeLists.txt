# =====================================================================
#  BSP 层：时钟 + 延时 + 位带
# =====================================================================

# 第 1 步：CMSIS 接口库——不编译任何源码，只把头文件路径和器件宏传播出去
add_library(cmsis INTERFACE)
target_include_directories(cmsis SYSTEM INTERFACE
    ${CMSIS_CORE_INC}          # core_cm4.h 等内核头
    ${CMSIS_DEV_INC}           # at32f435_437.h 器件头(基址/IRQn)
    ${AT32_SPL_INC}            # AT 寄存器结构体头(crm/gpio/pwc/flash)
    ${AT32_CONF_INC}           # 裁剪版 at32f435_437_conf.h(器件头强制 include)
    ${CMSIS_SYS_INC})          # system_at32f435_437.h(器件头内部 include)
target_compile_definitions(cmsis INTERFACE ${DEVICE_DEF})

# 第 2 步：BSP 静态库（自动收集 Src/*.c，自动暴露 Inc/）
mcu_add_lib(bsp)

# 第 3 步：依赖关系：bsp -> cmsis
#   PUBLIC 的含义：谁链接了 bsp，就自动获得 cmsis 的头文件路径和宏
target_link_libraries(bsp PUBLIC cmsis)

# 第 4 步：板载晶振频率 ★ 按你板子上实际焊的晶振改 ★
#   本板 HEXT = 8MHz。宏名是 HEXT_VALUE（AT 官方命名，不是 ST 的 HSE_VALUE），
#   conf.h 里有 #if !defined HEXT_VALUE 保护，命令行传值可覆盖默认值。
#   PUBLIC 会把这个编译定义传播给所有依赖方——
#   包括直接编进可执行文件的 system_at32f435_437.c。
target_compile_definitions(bsp PUBLIC HEXT_VALUE=8000000U)
