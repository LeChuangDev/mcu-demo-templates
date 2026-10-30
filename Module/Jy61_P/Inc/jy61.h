/**
 * @file  jy61.h
 * @brief JY61P / WitMotion 串口姿态传感器驱动（平台无关核心）。
 *
 * 架构
 * ----
 *   应用 UART 中断/ DMA 收到的字节调用 jy61_feed_byte() / jy61_feed()；
 *   本模块只负责协议解析、量程换算和数据发布，不操作任何 UART/GPIO 寄存器。
 *
 * 特性
 * ----
 *   - 无全局状态、无动态内存、无阻塞、无 printf 依赖；
 *   - 每个实例独立，可在同一工程中接多路 JY61P；
 *   - 逐字节流式解析，支持任意位置插入噪声后的自动重同步；
 *   - 可通过回调或 jy61_get_data() 获取最新数据；
 *   - 可选临界区回调（jy61_critical_t）保证中断与主循环并发读取安全；
 *   - 默认校验 0x55 数据包 SUM 字节，可用配置关闭。
 *
 * 协议
 * ----
 *   每帧固定 11 字节：
 *     [0]      0x55
 *     [1]      0x51 加速度 / 0x52 角速度 / 0x53 角度
 *     [2..7]   X/Y/Z 三个 int16（小端）
 *     [8..9]   温度/时间戳（原样保留）
 *     [10]     前 10 字节的累加和（低 8 位）
 *
 * 线程/中断约定
 * -------------
 *   - jy61_feed_* 只能在一个上下文（通常是 UART 中断）中调用；
 *   - 若主循环会调用 jy61_get_data()，建议在配置中提供临界区回调；
 *     未提供时使用序号检查做尽力而为的无锁快照。
 *   - on_packet 回调在 feed 上下文同步执行，必须短小且不可阻塞。
 */
#ifndef JY61_H
#define JY61_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "jy61_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 版本 ------------------------------------------------------------- */
#define JY61_VERSION_MAJOR 1
#define JY61_VERSION_MINOR 0
#define JY61_VERSION_PATCH 0

/* ---- 协议常量 --------------------------------------------------------- */
#define JY61_PACKET_SIZE 11u
#define JY61_PACKET_HEADER 0x55u
#define JY61_PACKET_TYPE_ACCEL 0x51u
#define JY61_PACKET_TYPE_GYRO 0x52u
#define JY61_PACKET_TYPE_ANGLE 0x53u

/* ---- 类型 ------------------------------------------------------------- */

/** 返回状态。 */
typedef enum {
    JY61_OK = 0,          /**< 成功 */
    JY61_ERR_PARAM = -1,  /**< 参数非法 */
    JY61_ERR_NO_DATA = -2,/**< 尚未收到任何有效数据包 */
    JY61_ERR_BUSY = -3    /**< 多次重试仍与写入冲突（建议配置临界区回调） */
} jy61_status_t;

/** 数据包类型。 */
typedef enum {
    JY61_PACKET_ACCEL = JY61_PACKET_TYPE_ACCEL,
    JY61_PACKET_GYRO = JY61_PACKET_TYPE_GYRO,
    JY61_PACKET_ANGLE = JY61_PACKET_TYPE_ANGLE
} jy61_packet_type_t;

/** 三维浮点向量。 */
typedef struct {
    float x;
    float y;
    float z;
} jy61_vec3f_t;

/** 最新一次解析结果（三种数据包共享一份快照）。 */
typedef struct {
    jy61_vec3f_t accel_g;   /**< 加速度，单位 g */
    jy61_vec3f_t gyro_dps;  /**< 角速度，单位 °/s */
    jy61_vec3f_t angle_deg; /**< 角度，单位 ° */
    uint16_t timestamp;     /**< 最近一帧携带的温度/时间戳原始值 */
    uint32_t packet_count;  /**< 累计解码成功的包数 */
} jy61_data_t;

/** 统计信息（用于诊断链路质量）。 */
typedef struct {
    uint32_t packets;          /**< 解码成功的包数 */
    uint32_t checksum_errors;  /**< 校验失败或类型非法的包数 */
    uint32_t discarded_bytes;  /**< 寻找帧头时丢弃的字节数 */
} jy61_stats_t;

/** 临界区回调；enter/exit 必须成对且可嵌套调用者的其他临界区。 */
typedef struct {
    void (*enter)(void *user);
    void (*exit)(void *user);
    void *user;
} jy61_critical_t;

/** 新数据包回调（在 feed 上下文执行）。 */
typedef void (*jy61_packet_cb_t)(void *user, const jy61_data_t *data,
                                 jy61_packet_type_t type);

/** 初始化配置。 */
typedef struct {
    float accel_g_per_lsb;   /**< 加速度换算系数，必须 > 0 */
    float gyro_dps_per_lsb;  /**< 角速度换算系数，必须 > 0 */
    float angle_deg_per_lsb; /**< 角度换算系数，必须 > 0 */
    bool checksum_enabled;   /**< 是否校验帧尾 SUM */
    jy61_packet_cb_t on_packet; /**< 可空 */
    void *user;                 /**< 回调用户指针 */
    jy61_critical_t critical;   /**< 可全空；用于保护 get_data 快照 */
} jy61_config_t;

/**
 * 驱动实例。
 *
 * 字段均为内部实现细节，应用不要直接读写；请使用公开函数。
 * 结构体保持可见，以便调用者静态分配，避免动态内存。
 */
typedef struct {
    jy61_config_t config;
    jy61_data_t data;
    uint8_t rx_buf[JY61_PACKET_SIZE];
    uint8_t rx_len;
    uint32_t packets;
    uint32_t checksum_errors;
    uint32_t discarded_bytes;
    volatile uint32_t sequence;
} jy61_t;

/* ---- API -------------------------------------------------------------- */

/**
 * @brief 填充一份出厂默认配置。
 *
 * 默认：±16 g / ±2000 °/s / ±180°，校验开启，回调与临界区为空。
 * 使用方仍需按需设置 on_packet / user / critical。
 */
void jy61_config_default(jy61_config_t *config);

/**
 * @brief 初始化实例。
 * @param ctx    实例，由调用者静态分配
 * @param config 配置；传 NULL 等价于使用 jy61_config_default()
 * @return JY61_OK 或 JY61_ERR_PARAM
 */
jy61_status_t jy61_init(jy61_t *ctx, const jy61_config_t *config);

/**
 * @brief 清空解析状态与统计数据，保留配置。
 */
jy61_status_t jy61_reset(jy61_t *ctx);

/**
 * @brief 送入一个字节。
 * @return 1 表示本次送入触发了一个有效数据包，否则 0。
 */
size_t jy61_feed_byte(jy61_t *ctx, uint8_t byte);

/**
 * @brief 送入一段字节。
 * @return 本次调用解码出的有效数据包数量。
 */
size_t jy61_feed(jy61_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief 获取最新数据快照。
 * @param ctx 实例
 * @param out 输出副本
 * @return JY61_OK；未收到过有效包时返回 JY61_ERR_NO_DATA。
 */
jy61_status_t jy61_get_data(jy61_t *ctx, jy61_data_t *out);

/** @brief 获取统计信息。 */
void jy61_get_stats(const jy61_t *ctx, jy61_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* JY61_H */
