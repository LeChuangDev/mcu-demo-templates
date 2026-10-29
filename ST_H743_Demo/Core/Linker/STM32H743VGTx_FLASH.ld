/* =====================================================================
 *  STM32H743VGTx_FLASH.ld —— GNU ld 链接脚本
 *  【换规格只改 MEMORY 里的 LENGTH】
 *    STM32H743VG : Flash 1MB  (本文件)
 *    STM32H743VI : Flash 2MB  (只把 FLASH 的 LENGTH 改成 2048K)
 *    RAM: 512K AXI SRAM(0x2400_0000) + 128K DTCM(0x2000_0000)
 *          + 64K ITCM(0x0000_0000)
 *  参考: RM0433 存储器映射 / DS12110 数据手册
 *
 *  ★ H7 与 G4/F4 的存储器差异 ★
 *   1) 0x2000_0000 不再是普通 SRAM，而是 CPU 私有 DTCM：CPU 访问最快，
 *      但 DMA1/DMA2/BDMA 访问不到！DMA 缓冲区要放 AXI SRAM(0x2400_0000)。
 *   2) 0x0000_0000 是 ITCM，不是 Flash 的启动别名 —— 所以 SystemInit
 *      必须把 VTOR 指向 0x0800_0000（已在 system_stm32h7xx.c 中启用）。
 *   3) H743 的 1MB/2MB Flash 由两个 512KB Bank 组成，地址连续，
 *      普通链接脚本按一整块 FLASH 处理即可（本模板不做双 Bank 交换）。
 *
 *  本模板默认把 .data/.bss/堆/栈都放在 AXI SRAM（512K，DMA 可访问）。
 *  DTCM/ITCM 只声明区域，留给以后按需放高频数据或中断代码。
 * ===================================================================== */

ENTRY(Reset_Handler)                 /* 入口 = 启动文件里的 Reset_Handler */

/* 栈顶 = RAM 最高地址；启动文件第一句话 .word _estack 用的就是这个 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 堆栈最小保留量：堆 512 字节、栈 4KB（H7 中断嵌套深，栈给足一些） */
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x1000;

/* ---- 存储器布局 ---- */
MEMORY
{
  /* ITCM：CPU 私有指令 RAM，零等待；放热代码需自己加 SECTION，本模板不用 */
  ITCMRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
  /* DTCM：CPU 私有数据 RAM，DMA 访问不到！本模板不主动放数据进去 */
  DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  /* AXI SRAM（D1 域）：512K，DMA1/DMA2/BDMA 都能访问，作为主 RAM */
  RAM     (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
  /* H743VG = 1MB；H743VI 改成 2048K */
  FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
}

/* ---- 段布局 ---- */
SECTIONS
{
  /* 1. 中断向量表：必须放 Flash 最开头，复位后硬件先读它 */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))            /* KEEP 防止 --gc-sections 误删 */
    . = ALIGN(4);
  } >FLASH

  /* 2. 代码段 */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)                      /* ARM/Thumb 互调用胶水 */
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;                     /* 代码结束地址 */
  } >FLASH

  /* 3. 只读数据（const 变量、字符串） */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* 4. 异常索引表（有 FPU 必须保留） */
  .ARM.extab :
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH
  .ARM :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  /* 5. C++ 构造/析构表（C 工程也会生成，保留） */
  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH
  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH
  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  /* 6. .data 的 Flash 源地址：初值存在 Flash，Reset_Handler 拷到 RAM */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;                     /* RAM 中的起始 */
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;                     /* RAM 中的结束 */
  } >RAM AT> FLASH                  /* 运行地址在 RAM，内容存 Flash */

  /* 7. .bss：零初始化变量，启动时清 0，不占 Flash */
  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;          /* newlib 也认这个符号名 */
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* 8. 堆栈占位区：只占坑，防止别的数据挤进栈空间 */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* 9. ARM 属性段（编译器版本等标记，不占运行空间） */
  .ARM.attributes 0 : { *(.ARM.attributes) }

  /* ★不要★ 写 `libc.a(*)/libm.a(*)/libgcc.a(*)` 丢进 /DISCARD/！
     那会把整个库的所有段都删掉——一旦用到 64 位除法(__aeabi_uldivmod)、
     浮点辅助函数等 libgcc 成员，链接阶段直接 undefined reference。
     没用上的库函数在 --gc-sections 阶段本来就不会被链接，无需手工丢弃。 */
}
