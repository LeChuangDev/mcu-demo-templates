/**
 * @file  init_stubs.c —— newlib 启动桩（配合 -nostartfiles）
 *
 * 官方 startup_stm32h743xx.s 在跳进 main 之前会调用 __libc_init_array()，
 * 它会遍历 .preinit_array / .init_array（C++ 全局构造、__attribute__((constructor))
 * 等），最后调用 _init()。
 *
 * 本工程链接时用了 -nostartfiles（不使用 GCC 自带的 crt0/crti/crtn），
 * 因此 newlib 里 crti.o 提供的 _init/_fini 不会被链进来，链接器会报：
 *     undefined reference to `_init'
 *
 * 这里给出空的 _init/_fini：C 工程没有需要额外初始化的运行环境，
 * .init_array 仍会被 __libc_init_array 正常遍历，功能不受影响。
 * 若以后加入 C++ 全局对象，也不需要改这里。
 */

void _init(void)
{
}

void _fini(void)
{
}

/**
 * H7 的 GCC 启动文件在 SystemInit() 之前会调用 ExitRun0Mode()，用来选择
 * SMPS/LDO 供电。当前 Core/System/system_stm32h7xx.c 里已经提供了强定义
 * （本模板通过 USE_PWR_LDO_SUPPLY 让它配置 PWR->CR3 的 LDO），所以下面
 * 这个弱符号实际不会生效，只作为"以后换成不带该函数的旧版 system 文件"
 * 时的兜底，避免链接阶段突然报 undefined reference。
 */
__attribute__((weak)) void ExitRun0Mode(void)
{
}
