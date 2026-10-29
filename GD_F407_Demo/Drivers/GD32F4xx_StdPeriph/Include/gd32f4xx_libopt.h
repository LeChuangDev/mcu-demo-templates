/*!
    \file    gd32f4xx_libopt.h
    \brief   library optional for gd32f4xx

             本工程为纯寄存器开发，只引入 GD 官方标准外设库的【寄存器定义头】
             (gd32f4xx_xxx.h 内仅含 REG32 寄存器映射和位宏，不编译任何库函数 .c)。
             gd32f4xx.h 会强制 #include 本文件，这里只包含当前工程用到的外设头；
             以后用到新外设(如 USART/SPI)时，把对应头文件加入下面列表即可。

    \version V3.3.3, trimmed for register-only project
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#ifndef GD32F4XX_LIBOPT_H
#define GD32F4XX_LIBOPT_H

#if defined (GD32F450) || defined (GD32F405) || defined (GD32F407) || defined (GD32F470) || defined (GD32F425) || defined (GD32F427)
    #include "gd32f4xx_rcu.h"      /* RCU 时钟控制寄存器 */
    #include "gd32f4xx_pmu.h"      /* PMU 电源管理/调压器/高驱动 */
    #include "gd32f4xx_fmc.h"      /* FMC Flash 等待周期 */
    #include "gd32f4xx_gpio.h"     /* GPIO 端口寄存器 */
    #include "gd32f4xx_misc.h"     /* NVIC 向量表等杂项定义 */
#endif

#endif /* GD32F4XX_LIBOPT_H */
