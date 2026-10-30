/**
 * @file  jy61.c
 * @brief JY61P 协议解析与量程换算实现。
 */
#include "jy61.h"

#include <string.h>

#define JY61_GET_DATA_RETRIES 8u

/* ------------------------------------------------------------------------- */
/* 内部工具                                                                  */
/* ------------------------------------------------------------------------- */

static int16_t jy61_read_i16_le(const uint8_t *p)
{
    uint16_t raw = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return (int16_t)raw;
}

static uint8_t jy61_checksum(const uint8_t *packet)
{
    uint32_t sum = 0u;
    uint8_t i;

    for (i = 0u; i < (JY61_PACKET_SIZE - 1u); ++i) {
        sum += packet[i];
    }
    return (uint8_t)(sum & 0xFFu);
}

static bool jy61_packet_is_valid(const jy61_t *ctx)
{
    uint8_t type = ctx->rx_buf[1];

    if (type != JY61_PACKET_TYPE_ACCEL &&
        type != JY61_PACKET_TYPE_GYRO &&
        type != JY61_PACKET_TYPE_ANGLE) {
        return false;
    }

    if (ctx->config.checksum_enabled &&
        jy61_checksum(ctx->rx_buf) != ctx->rx_buf[JY61_PACKET_SIZE - 1u]) {
        return false;
    }

    return true;
}

/**
 * @brief 丢弃无效帧，并从已缓存的字节中寻找下一个可能的帧头。
 *
 * 数据包只有 11 字节，搜索窗口很小，使用线性扫描即可。
 * 找到 0x55 后把它及其后面的字节移到缓冲区开头，避免丢包。
 */
static void jy61_resync(jy61_t *ctx)
{
    uint8_t i;

    for (i = 1u; i < JY61_PACKET_SIZE; ++i) {
        if (ctx->rx_buf[i] == JY61_PACKET_HEADER) {
            uint8_t remaining = (uint8_t)(JY61_PACKET_SIZE - i);
            memmove(ctx->rx_buf, &ctx->rx_buf[i], remaining);
            ctx->rx_len = remaining;
            return;
        }
    }

    ctx->rx_len = 0u;
}

static jy61_vec3f_t jy61_scale_vector(const uint8_t *payload, float scale)
{
    jy61_vec3f_t value;

    value.x = (float)jy61_read_i16_le(&payload[0]) * scale;
    value.y = (float)jy61_read_i16_le(&payload[2]) * scale;
    value.z = (float)jy61_read_i16_le(&payload[4]) * scale;
    return value;
}

/**
 * @brief 发布一帧数据。
 *
 * 写入过程用 sequence 的奇偶标记“正在更新”，无临界区时读取方可据此重试。
 * 回调放在发布完成之后，避免回调里看到半更新的数据。
 */
static void jy61_publish(jy61_t *ctx, jy61_packet_type_t type)
{
    const uint8_t *payload = &ctx->rx_buf[2];
    uint16_t timestamp =
        (uint16_t)((uint16_t)ctx->rx_buf[8] | ((uint16_t)ctx->rx_buf[9] << 8));

    ctx->sequence++;

    switch (type) {
    case JY61_PACKET_ACCEL:
        ctx->data.accel_g =
            jy61_scale_vector(payload, ctx->config.accel_g_per_lsb);
        break;
    case JY61_PACKET_GYRO:
        ctx->data.gyro_dps =
            jy61_scale_vector(payload, ctx->config.gyro_dps_per_lsb);
        break;
    case JY61_PACKET_ANGLE:
        ctx->data.angle_deg =
            jy61_scale_vector(payload, ctx->config.angle_deg_per_lsb);
        break;
    default:
        break;
    }

    ctx->data.timestamp = timestamp;
    ctx->packets++;
    ctx->data.packet_count = ctx->packets;

    ctx->sequence++;

    if (ctx->config.on_packet != NULL) {
        ctx->config.on_packet(ctx->config.user, &ctx->data, type);
    }
}

/* ------------------------------------------------------------------------- */
/* 公开 API                                                                  */
/* ------------------------------------------------------------------------- */

void jy61_config_default(jy61_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->accel_g_per_lsb = JY61_DEFAULT_ACCEL_G_PER_LSB;
    config->gyro_dps_per_lsb = JY61_DEFAULT_GYRO_DPS_PER_LSB;
    config->angle_deg_per_lsb = JY61_DEFAULT_ANGLE_DEG_PER_LSB;
    config->checksum_enabled = (JY61_FEATURE_CHECKSUM != 0);
}

jy61_status_t jy61_init(jy61_t *ctx, const jy61_config_t *config)
{
    jy61_config_t defaults;

    if (ctx == NULL) {
        return JY61_ERR_PARAM;
    }

    if (config == NULL) {
        jy61_config_default(&defaults);
        config = &defaults;
    }

    if (!(config->accel_g_per_lsb > 0.0f) ||
        !(config->gyro_dps_per_lsb > 0.0f) ||
        !(config->angle_deg_per_lsb > 0.0f)) {
        return JY61_ERR_PARAM;
    }
    if ((config->critical.enter == NULL) !=
        (config->critical.exit == NULL)) {
        return JY61_ERR_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->config = *config;
    return JY61_OK;
}

jy61_status_t jy61_reset(jy61_t *ctx)
{
    if (ctx == NULL) {
        return JY61_ERR_PARAM;
    }

    memset(&ctx->data, 0, sizeof(ctx->data));
    memset(ctx->rx_buf, 0, sizeof(ctx->rx_buf));
    ctx->rx_len = 0u;
    ctx->packets = 0u;
    ctx->checksum_errors = 0u;
    ctx->discarded_bytes = 0u;
    ctx->sequence = 0u;
    return JY61_OK;
}

size_t jy61_feed_byte(jy61_t *ctx, uint8_t byte)
{
    if (ctx == NULL) {
        return 0u;
    }

    if (ctx->rx_len == 0u) {
        if (byte != JY61_PACKET_HEADER) {
            ctx->discarded_bytes++;
            return 0u;
        }
        ctx->rx_buf[0] = byte;
        ctx->rx_len = 1u;
        return 0u;
    }

    ctx->rx_buf[ctx->rx_len++] = byte;
    if (ctx->rx_len < JY61_PACKET_SIZE) {
        return 0u;
    }

    if (jy61_packet_is_valid(ctx)) {
        jy61_publish(ctx, (jy61_packet_type_t)ctx->rx_buf[1]);
        ctx->rx_len = 0u;
        return 1u;
    }

    ctx->checksum_errors++;
    jy61_resync(ctx);
    return 0u;
}

size_t jy61_feed(jy61_t *ctx, const uint8_t *data, size_t len)
{
    size_t packets = 0u;
    size_t i;

    if (ctx == NULL || (data == NULL && len != 0u)) {
        return 0u;
    }

    for (i = 0u; i < len; ++i) {
        packets += jy61_feed_byte(ctx, data[i]);
    }
    return packets;
}

jy61_status_t jy61_get_data(jy61_t *ctx, jy61_data_t *out)
{
    uint32_t attempt = 0u;

    if (ctx == NULL || out == NULL) {
        return JY61_ERR_PARAM;
    }

    if (ctx->config.critical.enter != NULL) {
        jy61_status_t status = JY61_OK;

        ctx->config.critical.enter(ctx->config.critical.user);
        if (ctx->packets == 0u) {
            status = JY61_ERR_NO_DATA;
        } else {
            *out = ctx->data;
        }
        if (ctx->config.critical.exit != NULL) {
            ctx->config.critical.exit(ctx->config.critical.user);
        }
        return status;
    }

    /* 没有临界区回调时，使用序号重试做无锁快照。 */
    for (attempt = 0u; attempt < JY61_GET_DATA_RETRIES; ++attempt) {
        uint32_t before = ctx->sequence;

        if ((before & 1u) != 0u) {
            continue;
        }
        if (ctx->packets == 0u) {
            return JY61_ERR_NO_DATA;
        }

        *out = ctx->data;

        if (before == ctx->sequence) {
            return JY61_OK;
        }
    }

    return JY61_ERR_BUSY;
}

void jy61_get_stats(const jy61_t *ctx, jy61_stats_t *out)
{
    if (ctx == NULL || out == NULL) {
        return;
    }

    out->packets = ctx->packets;
    out->checksum_errors = ctx->checksum_errors;
    out->discarded_bytes = ctx->discarded_bytes;
}
