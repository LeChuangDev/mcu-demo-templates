/* =====================================================================
 *  CW32F030x8_FLASH.ld —— GNU ld 链接脚本
 *
 *  目标：CW32F030C8T6（立创·地文星 CW32F030C8T6 开发板）
 *    Flash : 64K @ 0x00000000   （CW32 的 Flash 直接映射在地址 0，
 *                                 复位后硬件从 0x0000_0000 取向量表）
 *    SRAM  :  8K @ 0x20000000
 *
 *  【换容量只改 MEMORY 里的 LENGTH】
 *    C6 = 32K / C8 = 64K（本文件），RAM 均为 8K（x6/x8）
 *
 *  注意：chip.cmake 会解析下面 FLASH 行的 LENGTH 做容量校验，
 *        改格式时保持 "FLASH (rx) : ORIGIN = ..., LENGTH = NNK" 这一行的写法。
 * ===================================================================== */

ENTRY(Reset_Handler)

/* 栈顶 = RAM 最高地址；启动文件向量表第 1 个字 _estack 用的就是它 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 堆/栈最小保留量，链接时若 RAM 不够会报错 */
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
  FLASH (rx)      : ORIGIN = 0x00000000, LENGTH = 64K
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 8K
}

SECTIONS
{
  /* 1. 中断向量表：必须放在 Flash 最开头（地址 0） */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* 2. 代码段 */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* 3. 只读数据（const 变量、字符串） */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* 4. 异常索引表 */
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

  /* 5. C/C++ 构造/析构表（__libc_init_array 使用） */
  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* 6. 已初始化变量：运行前由 Flash 拷贝到 RAM */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* 7. 零初始化变量 */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* 8. 堆栈保留区：只占地址，不产生数据 */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
