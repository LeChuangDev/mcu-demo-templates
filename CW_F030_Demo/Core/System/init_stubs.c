/**
 * @file  init_stubs.c —— newlib 启动桩（配合 -nostartfiles）
 *
 * 启动文件 Reset_Handler 会调用 __libc_init_array()，它遍历
 * .preinit_array / .init_array（C++ 全局构造、__attribute__((constructor))），
 * 最后调用 _init()。本工程用 -nostartfiles 链接，newlib 的 crti.o 不会进来，
 * 因此 _init/_fini 需要在这里给空桩，否则链接报 undefined reference。
 *
 * C 工程这两个函数实际不会被调用到；以后加 C++ 全局对象时，
 * .init_array 仍由 __libc_init_array 正常遍历，无需改动。
 */

void _init(void)
{
}

void _fini(void)
{
}
