/*!
    \file    gd32e23x_libopt.h
    \brief   library optional for gd32e23x —— 本工程裁剪版

    说明：
      官方原版会把全部外设头都 include 进来；本工程只做寄存器级开发，
      不编译官方标准外设库的 .c，因此只保留 BSP 当前用到的外设头：
        rcu   —— 时钟/复位（bsp_clock.c）
        gpio  —— 板载 LED（bsp_led.c）
        fmc   —— Flash 等待周期（bsp_clock.c）
        timer —— 微秒延时 TIMER5（bsp_delay.c）
        misc  —— nvic_vector_table_set 声明（bsp_clock.c 补实现）
        pmu   —— 低功耗/唤醒预留（当前未使用）
      以后要加外设时，把对应的 gd32e23x_xxx.h 加进来即可。
*/
#ifndef GD32E23X_LIBOPT_H
#define GD32E23X_LIBOPT_H

#include "gd32e23x_rcu.h"
#include "gd32e23x_gpio.h"
#include "gd32e23x_fmc.h"
#include "gd32e23x_timer.h"
#include "gd32e23x_misc.h"
#include "gd32e23x_pmu.h"

#endif /* GD32E23X_LIBOPT_H */
