/**
 * @file  at32f435_437_conf.h —— 本工程裁剪版器件配置头
 *
 * 背景：at32f435_437.h 末尾会【无条件】include 本文件（文件名固定）。
 *   官方模板的 conf.h 会打开全部 *_MODULE_ENABLED 并聚合 20+ 个外设头；
 *   本工程是纯寄存器工程（不编译官方 SPL 的 .c，只借寄存器结构体/位域），
 *   所以这里只声明晶振频率 + 打开本工程实际用到的 4 个外设模块头：
 *     CRM   时钟复位单元
 *     GPIO  通用 IO
 *     PWC   电源控制（288MHz 需要 LDO 1.3V）
 *     FLASH Flash 控制（288MHz 需要 fdiv=3）
 *   以后用到新外设时，把驱动头放进 Drivers/AT32F4xx_StdPeriph/Include/
 *   并在这里加一个 *_MODULE_ENABLED 即可。
 */
#ifndef __AT32F435_437_CONF_H
#define __AT32F435_437_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* HEXT 外部高速晶振：本板 8MHz。
   也可在编译命令行 -DHEXT_VALUE=... 覆盖（见 BSP/CMakeLists.txt） */
#if !defined HEXT_VALUE
#define HEXT_VALUE              ((uint32_t)8000000)
#endif
#define HEXT_STARTUP_TIMEOUT    ((uint16_t)0x3000)
#define HICK_VALUE              ((uint32_t)8000000)
#define LEXT_VALUE              ((uint32_t)32768)

/* ---- 模块开关（只开本工程用到的外设头）---- */
#define CRM_MODULE_ENABLED
#define GPIO_MODULE_ENABLED
#define PWC_MODULE_ENABLED
#define FLASH_MODULE_ENABLED

#ifdef CRM_MODULE_ENABLED
#include "at32f435_437_crm.h"
#endif
#ifdef GPIO_MODULE_ENABLED
#include "at32f435_437_gpio.h"
#endif
#ifdef PWC_MODULE_ENABLED
#include "at32f435_437_pwc.h"
#endif
#ifdef FLASH_MODULE_ENABLED
#include "at32f435_437_flash.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AT32F435_437_CONF_H */
