/**
 ******************************************************************************
 * @file    startup_cw32f030.s
 * @brief   CW32F030 (Cortex-M0+) 启动文件 —— GNU as / GCC 语法
 *
 * 官方固件库只提供 MDK(armasm) 与 IAR 版启动文件，本文件是等价的手写
 * GCC 版本，做三件事：
 *   1) 在 Flash 地址 0 放中断向量表（48 项 = 16 个内核异常 + 32 个外设中断）
 *   2) 复位后把 .data 从 Flash 拷贝到 SRAM、把 .bss 清零
 *   3) 调用 SystemInit() -> __libc_init_array() -> main()
 *
 * 符号约定与链接脚本 Core/Linker/CW32F030x8_FLASH.ld 一一对应：
 *   _estack / _sidata / _sdata / _edata / _sbss / _ebss
 *
 * 注：Cortex-M0+ 没有位带(bit-band)、没有 MPU，向量表也没有
 *     MemManage/BusFault/UsageFault（这 3 项保留为 0）。
 ******************************************************************************
 */

  .syntax unified
  .cpu cortex-m0plus
  .thumb

  .global  g_pfnVectors
  .global  Default_Handler

/* ==========================================================================
 * 中断向量表
 * ========================================================================== */
  .section  .isr_vector,"a",%progbits
  .type     g_pfnVectors, %object

g_pfnVectors:
  .word  _estack                         /* 0x00 初始栈顶 */
  .word  Reset_Handler                   /* 0x04 复位 */
  .word  NMI_Handler                     /* 0x08 NMI */
  .word  HardFault_Handler               /* 0x0C HardFault */
  .word  0                               /* 0x10 保留 */
  .word  0                               /* 0x14 保留 */
  .word  0                               /* 0x18 保留 */
  .word  0                               /* 0x1C 保留 */
  .word  0                               /* 0x20 保留 */
  .word  0                               /* 0x24 保留 */
  .word  0                               /* 0x28 保留 */
  .word  SVC_Handler                     /* 0x2C SVCall */
  .word  0                               /* 0x30 保留 */
  .word  0                               /* 0x34 保留 */
  .word  PendSV_Handler                  /* 0x38 PendSV */
  .word  SysTick_Handler                 /* 0x3C SysTick */

  /* ---- 32 个外设中断，编号与 cw32f030.h 的 IRQn_Type 一致 ---- */
  .word  WDT_IRQHandler                  /*  0 看门狗 */
  .word  LVD_IRQHandler                  /*  1 低电压检测 */
  .word  RTC_IRQHandler                  /*  2 RTC */
  .word  FLASHRAM_IRQHandler             /*  3 Flash/RAM */
  .word  RCC_IRQHandler                  /*  4 RCC */
  .word  GPIOA_IRQHandler                /*  5 GPIOA */
  .word  GPIOB_IRQHandler                /*  6 GPIOB */
  .word  GPIOC_IRQHandler                /*  7 GPIOC */
  .word  GPIOF_IRQHandler                /*  8 GPIOF */
  .word  DMACH1_IRQHandler               /*  9 DMA 通道 1 */
  .word  DMACH23_IRQHandler              /* 10 DMA 通道 2/3 */
  .word  DMACH45_IRQHandler              /* 11 DMA 通道 4/5 */
  .word  ADC_IRQHandler                  /* 12 ADC */
  .word  ATIM_IRQHandler                 /* 13 高级定时器 */
  .word  VC1_IRQHandler                  /* 14 电压比较器 1 */
  .word  VC2_IRQHandler                  /* 15 电压比较器 2 */
  .word  GTIM1_IRQHandler                /* 16 通用定时器 1 */
  .word  GTIM2_IRQHandler                /* 17 通用定时器 2 */
  .word  GTIM3_IRQHandler                /* 18 通用定时器 3 */
  .word  GTIM4_IRQHandler                /* 19 通用定时器 4 */
  .word  BTIM1_IRQHandler                /* 20 基本定时器 1 */
  .word  BTIM2_IRQHandler                /* 21 基本定时器 2 */
  .word  BTIM3_IRQHandler                /* 22 基本定时器 3 */
  .word  I2C1_IRQHandler                 /* 23 I2C1 */
  .word  I2C2_IRQHandler                 /* 24 I2C2 */
  .word  SPI1_IRQHandler                 /* 25 SPI1 */
  .word  SPI2_IRQHandler                 /* 26 SPI2 */
  .word  UART1_IRQHandler                /* 27 UART1 */
  .word  UART2_IRQHandler                /* 28 UART2 */
  .word  UART3_IRQHandler                /* 29 UART3 */
  .word  AWT_IRQHandler                  /* 30 自动唤醒定时器 */
  .word  FAULT_IRQHandler                /* 31 FAULT */

  .size  g_pfnVectors, .-g_pfnVectors

/* ==========================================================================
 * 复位入口
 * ========================================================================== */
  .section  .text.Reset_Handler,"ax",%progbits
  .weak     Reset_Handler
  .type     Reset_Handler, %function
Reset_Handler:
  /* ---- 1. 把 .data 的初值从 Flash(_sidata) 拷到 SRAM(_sdata.._edata) ---- */
  ldr   r0, =_sdata
  ldr   r1, =_sidata
  ldr   r2, =_edata
CopyDataInit:
  cmp   r0, r2
  bcs   CopyDataDone
  ldr   r3, [r1]
  str   r3, [r0]
  adds  r0, r0, #4
  adds  r1, r1, #4
  b     CopyDataInit
CopyDataDone:

  /* ---- 2. 把 .bss(_sbss.._ebss) 清零 ---- */
  ldr   r0, =_sbss
  ldr   r1, =_ebss
  movs  r3, #0
FillZerobss:
  cmp   r0, r1
  bcs   FillZerobssDone
  str   r3, [r0]
  adds  r0, r0, #4
  b     FillZerobss
FillZerobssDone:

  /* ---- 3. 芯片初始化 -> C 运行时构造 -> main ---- */
  bl    SystemInit
  bl    __libc_init_array
  bl    main

  /* main 正常返回时停在这里（裸机不应返回） */
  b     .
  .size Reset_Handler, .-Reset_Handler

/* ==========================================================================
 * 默认中断处理：未实现的异常/中断都进这里死循环，便于调试器定位
 * ========================================================================== */
  .section  .text.Default_Handler,"ax",%progbits
  .weak     Default_Handler
  .type     Default_Handler, %function
Default_Handler:
  b     Default_Handler
  .size Default_Handler, .-Default_Handler

/* ---- 内核异常弱符号 ---- */
  .weak NMI_Handler
  .thumb_set NMI_Handler, Default_Handler
  .weak HardFault_Handler
  .thumb_set HardFault_Handler, Default_Handler
  .weak SVC_Handler
  .thumb_set SVC_Handler, Default_Handler
  .weak PendSV_Handler
  .thumb_set PendSV_Handler, Default_Handler
  .weak SysTick_Handler
  .thumb_set SysTick_Handler, Default_Handler

/* ---- 外设中断弱符号（需要时在 C 里定义同名函数即可覆盖） ---- */
  .weak WDT_IRQHandler
  .thumb_set WDT_IRQHandler, Default_Handler
  .weak LVD_IRQHandler
  .thumb_set LVD_IRQHandler, Default_Handler
  .weak RTC_IRQHandler
  .thumb_set RTC_IRQHandler, Default_Handler
  .weak FLASHRAM_IRQHandler
  .thumb_set FLASHRAM_IRQHandler, Default_Handler
  .weak RCC_IRQHandler
  .thumb_set RCC_IRQHandler, Default_Handler
  .weak GPIOA_IRQHandler
  .thumb_set GPIOA_IRQHandler, Default_Handler
  .weak GPIOB_IRQHandler
  .thumb_set GPIOB_IRQHandler, Default_Handler
  .weak GPIOC_IRQHandler
  .thumb_set GPIOC_IRQHandler, Default_Handler
  .weak GPIOF_IRQHandler
  .thumb_set GPIOF_IRQHandler, Default_Handler
  .weak DMACH1_IRQHandler
  .thumb_set DMACH1_IRQHandler, Default_Handler
  .weak DMACH23_IRQHandler
  .thumb_set DMACH23_IRQHandler, Default_Handler
  .weak DMACH45_IRQHandler
  .thumb_set DMACH45_IRQHandler, Default_Handler
  .weak ADC_IRQHandler
  .thumb_set ADC_IRQHandler, Default_Handler
  .weak ATIM_IRQHandler
  .thumb_set ATIM_IRQHandler, Default_Handler
  .weak VC1_IRQHandler
  .thumb_set VC1_IRQHandler, Default_Handler
  .weak VC2_IRQHandler
  .thumb_set VC2_IRQHandler, Default_Handler
  .weak GTIM1_IRQHandler
  .thumb_set GTIM1_IRQHandler, Default_Handler
  .weak GTIM2_IRQHandler
  .thumb_set GTIM2_IRQHandler, Default_Handler
  .weak GTIM3_IRQHandler
  .thumb_set GTIM3_IRQHandler, Default_Handler
  .weak GTIM4_IRQHandler
  .thumb_set GTIM4_IRQHandler, Default_Handler
  .weak BTIM1_IRQHandler
  .thumb_set BTIM1_IRQHandler, Default_Handler
  .weak BTIM2_IRQHandler
  .thumb_set BTIM2_IRQHandler, Default_Handler
  .weak BTIM3_IRQHandler
  .thumb_set BTIM3_IRQHandler, Default_Handler
  .weak I2C1_IRQHandler
  .thumb_set I2C1_IRQHandler, Default_Handler
  .weak I2C2_IRQHandler
  .thumb_set I2C2_IRQHandler, Default_Handler
  .weak SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler, Default_Handler
  .weak SPI2_IRQHandler
  .thumb_set SPI2_IRQHandler, Default_Handler
  .weak UART1_IRQHandler
  .thumb_set UART1_IRQHandler, Default_Handler
  .weak UART2_IRQHandler
  .thumb_set UART2_IRQHandler, Default_Handler
  .weak UART3_IRQHandler
  .thumb_set UART3_IRQHandler, Default_Handler
  .weak AWT_IRQHandler
  .thumb_set AWT_IRQHandler, Default_Handler
  .weak FAULT_IRQHandler
  .thumb_set FAULT_IRQHandler, Default_Handler
