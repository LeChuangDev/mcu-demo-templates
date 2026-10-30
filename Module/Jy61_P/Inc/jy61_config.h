/**
 * @file  jy61_config.h
 * @brief JY61P 模块编译期配置。
 *
 * 所有宏都可以在编译命令行通过 -D 覆盖，例如：
 *   -DJY61_DEFAULT_ACCEL_G_PER_LSB=...
 * 也可以修改本文件作为项目默认值。
 */
#ifndef JY61_CONFIG_H
#define JY61_CONFIG_H

/* ---- 默认量程换算系数 ------------------------------------------------ */
/* JY61P 出厂配置：加速度 ±16 g、角速度 ±2000 °/s、角度 ±180°。
 * 传感器输出 int16，满量程对应 32768，因此每 LSB 的物理量如下。 */
#ifndef JY61_DEFAULT_ACCEL_G_PER_LSB
#define JY61_DEFAULT_ACCEL_G_PER_LSB (16.0f / 32768.0f)
#endif

#ifndef JY61_DEFAULT_GYRO_DPS_PER_LSB
#define JY61_DEFAULT_GYRO_DPS_PER_LSB (2000.0f / 32768.0f)
#endif

#ifndef JY61_DEFAULT_ANGLE_DEG_PER_LSB
#define JY61_DEFAULT_ANGLE_DEG_PER_LSB (180.0f / 32768.0f)
#endif

/* ---- 协议校验 --------------------------------------------------------- */
/* 1 = 校验 0x55 数据包末尾的 SUM 字节（推荐）。
 * 若使用的固件关闭了校验和，可在运行时把 jy61_config_t.checksum_enabled
 * 置为 false，或在这里改成 0 作为全局默认。 */
#ifndef JY61_FEATURE_CHECKSUM
#define JY61_FEATURE_CHECKSUM 1
#endif

#endif /* JY61_CONFIG_H */
