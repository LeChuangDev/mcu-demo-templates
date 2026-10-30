/**
 * @file  zigbee.h
 * @brief Zigbee/无线串口链路通用帧协议框架（平台无关）。
 *
 * 定位
 * ----
 *   不同厂商的 Zigbee 串口模块（DL-LN3x、DRF、CC2530 透传等）帧格式各异。
 *   本模块把“串口分帧”抽象成可配置的链路层：
 *
 *     [SOP 1~2B] [LEN 1~2B, 大端] [PAYLOAD 0~N] [CHECKSUM 0~2B] [EOP 0~2B]
 *
 *   帧头/帧尾、长度字段宽度、校验算法、转义规则都由 zb_config_t 指定；
 *   具体业务字段（地址、端口、命令字等）由应用在 on_frame 回调里解析。
 *   另提供完全透明的 RAW 模式，用于调试或厂商不使用分帧的场合。
 *
 * 特性
 * ----
 *   - 无全局状态、无动态内存、无 printf 依赖；
 *   - 流式解析，任意位置插入噪声后自动重新同步；
 *   - 校验算法：SUM8 / XOR8 / CRC8 / CRC16-CCITT，可选覆盖帧头；
 *   - 可选字节转义（ESC + XOR），避免负载中出现帧头/帧尾/ESC 字节；
 *   - 发送与接收共用同一份配置，便于回环测试；
 *   - 统计计数便于现场诊断链路质量。
 *
 * 上下文约定
 * ----------
 *   - zb_feed_* 可由 UART 中断或 DMA 完成回调调用，但同一实例只能有一个
 *     写入者；
 *   - on_frame/on_raw/on_error 在 feed 上下文同步执行，必须短小且不可阻塞；
 *   - zb_send 可能阻塞（取决于 port.write），不要在中断中调用。
 */
#ifndef ZIGBEE_H
#define ZIGBEE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zigbee_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 版本 ------------------------------------------------------------- */
#define ZB_VERSION_MAJOR 1
#define ZB_VERSION_MINOR 0
#define ZB_VERSION_PATCH 0

/* ---- 返回状态 --------------------------------------------------------- */
typedef enum {
    ZB_OK = 0,
    ZB_ERR_PARAM = -1,    /**< 参数非法或配置不完整 */
    ZB_ERR_NO_SPACE = -2, /**< 负载超过容量或组帧缓冲区不足 */
    ZB_ERR_IO = -3,       /**< port.write 报告发送失败 */
    ZB_ERR_STATE = -4     /**< 实例未初始化 */
} zb_status_t;

/* ---- 配置类型 --------------------------------------------------------- */
typedef enum {
    ZB_MODE_FRAMED = 0, /**< 按帧解析 */
    ZB_MODE_RAW = 1     /**< 透传，不解析 */
} zb_mode_t;

typedef enum {
    ZB_CHECKSUM_NONE = 0,
    ZB_CHECKSUM_SUM8 = 1,
    ZB_CHECKSUM_XOR8 = 2,
    ZB_CHECKSUM_CRC8 = 3,
    ZB_CHECKSUM_CRC16_CCITT = 4
} zb_checksum_t;

/** 解析错误类型，用于 on_error 回调。 */
typedef enum {
    ZB_ERROR_CHECKSUM = 0, /**< 校验失败 */
    ZB_ERROR_OVERFLOW = 1, /**< 长度字段超过 ZB_MAX_PAYLOAD */
    ZB_ERROR_FRAMING = 2   /**< 帧尾不匹配 */
} zb_error_t;

/* ---- 回调 ------------------------------------------------------------- */
typedef void (*zb_frame_cb_t)(void *user, const uint8_t *payload, size_t len);
typedef void (*zb_raw_cb_t)(void *user, const uint8_t *data, size_t len);
typedef void (*zb_error_cb_t)(void *user, zb_error_t error);

/* ---- 统计 ------------------------------------------------------------- */
typedef struct {
    uint32_t frames_ok;
    uint32_t checksum_errors;
    uint32_t overflow_errors;
    uint32_t framing_errors;
    uint32_t discarded_bytes;
    uint32_t raw_chunks;
} zb_stats_t;

/* ---- 平台端口 --------------------------------------------------------- */
/** 发送回调；成功返回 true。RX-only 场景可把 port 设为 NULL。 */
typedef struct {
    bool (*write)(void *user, const uint8_t *data, size_t len);
    void *user;
} zb_port_t;

/* ---- 配置 ------------------------------------------------------------- */
/**
 * @brief 链路配置。
 *
 * 校验覆盖范围：
 *   - 默认覆盖 [LEN][PAYLOAD]；
 *   - checksum_covers_header = true 时额外覆盖 [SOP]；
 *   - EOP 永远不参与校验。
 *
 * 转义规则：
 *   - 仅对 [LEN][PAYLOAD][CHECKSUM] 转义，SOP/EOP 保持原样；
 *   - 需要转义的字节：escape_byte、任意 SOP 字节、任意 EOP 字节；
 *   - 转义后写入两个字节：escape_byte、(byte ^ escape_xor)。
 */
typedef struct {
    zb_mode_t mode;

    uint8_t sop[ZB_MAX_SOP_SIZE];
    uint8_t sop_len;
    uint8_t eop[ZB_MAX_EOP_SIZE];
    uint8_t eop_len;

    uint8_t length_bytes; /**< 1 或 2，大端 */
    zb_checksum_t checksum;
    bool checksum_covers_header;

    bool escape_enabled;
    uint8_t escape_byte;
    uint8_t escape_xor;

    zb_frame_cb_t on_frame;
    zb_raw_cb_t on_raw;
    zb_error_cb_t on_error;
    void *user;
} zb_config_t;

/* ---- 实例 ------------------------------------------------------------- */
/** 链路实例；字段为内部实现，请使用公开 API。 */
typedef struct {
    zb_config_t config;
    zb_port_t port;

    uint8_t state;
    uint8_t sop_pos;
    uint8_t len_bytes[ZB_MAX_LENGTH_SIZE];
    uint8_t len_received;
    uint16_t payload_len;
    uint16_t body_expected;
    uint16_t body_received;
    uint8_t body[ZB_MAX_BODY_SIZE];
    uint8_t eop_received;
    bool escape_pending;

    uint8_t tx_buffer[ZB_MAX_FRAME_SIZE];
    zb_stats_t stats;
    bool initialized;
} zb_link_t;

/* ---- API -------------------------------------------------------------- */

/** @brief 填默认配置：0xFA 帧头、1 字节长度、SUM8 校验、0xFB 帧尾。 */
void zb_config_default(zb_config_t *config);

/**
 * @brief 初始化链路。
 * @param link   实例，由调用者静态分配
 * @param config 配置，不能为空
 * @param port   发送端口，可为 NULL（只收不发）
 */
zb_status_t zb_init(zb_link_t *link, const zb_config_t *config,
                    const zb_port_t *port);

/** @brief 清空解析状态与统计，保留配置与端口。 */
zb_status_t zb_reset(zb_link_t *link);

/** @brief 输入一个字节；返回 ZB_OK 或 ZB_ERR_STATE/ZB_ERR_PARAM。 */
zb_status_t zb_feed_byte(zb_link_t *link, uint8_t byte);

/** @brief 输入一段字节；返回 ZB_OK 或 ZB_ERR_STATE/ZB_ERR_PARAM。 */
zb_status_t zb_feed(zb_link_t *link, const uint8_t *data, size_t len);

/**
 * @brief 按配置组帧并发送。
 * @return ZB_OK；未配置 port 时返回 ZB_ERR_PARAM；负载过大返回
 *         ZB_ERR_NO_SPACE；底层写失败返回 ZB_ERR_IO。
 */
zb_status_t zb_send(zb_link_t *link, const uint8_t *payload, size_t len);

/** @brief 读取统计信息。 */
void zb_get_stats(const zb_link_t *link, zb_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ZIGBEE_H */
