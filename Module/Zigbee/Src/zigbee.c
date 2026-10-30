/**
 * @file  zigbee.c
 * @brief 可配置串口帧链路实现。
 */
#include "zigbee.h"

#include <string.h>

enum {
    ZB_STATE_SOP = 0u,
    ZB_STATE_LEN = 1u,
    ZB_STATE_BODY = 2u,
    ZB_STATE_EOP = 3u
};

/* ------------------------------------------------------------------------- */
/* 校验                                                                      */
/* ------------------------------------------------------------------------- */

static uint8_t zb_checksum_size(zb_checksum_t type)
{
#if ZB_ENABLE_CHECKSUM
    switch (type) {
    case ZB_CHECKSUM_NONE:
        return 0u;
    case ZB_CHECKSUM_CRC16_CCITT:
        return 2u;
    case ZB_CHECKSUM_SUM8:
    case ZB_CHECKSUM_XOR8:
    case ZB_CHECKSUM_CRC8:
    default:
        return 1u;
    }
#else
    (void)type;
    return 0u;
#endif
}

static uint16_t zb_checksum_start(zb_checksum_t type)
{
#if ZB_ENABLE_CHECKSUM
    return (type == ZB_CHECKSUM_CRC16_CCITT) ? 0xFFFFu : 0u;
#else
    (void)type;
    return 0u;
#endif
}

static uint16_t zb_checksum_update(zb_checksum_t type, uint16_t accumulator,
                                   const uint8_t *data, size_t len)
{
#if ZB_ENABLE_CHECKSUM
    size_t i;

    switch (type) {
    case ZB_CHECKSUM_SUM8:
        for (i = 0u; i < len; ++i) {
            accumulator = (uint16_t)((accumulator + data[i]) & 0xFFu);
        }
        break;
    case ZB_CHECKSUM_XOR8:
        for (i = 0u; i < len; ++i) {
            accumulator = (uint16_t)((accumulator ^ data[i]) & 0xFFu);
        }
        break;
    case ZB_CHECKSUM_CRC8: {
        uint8_t crc = (uint8_t)accumulator;

        for (i = 0u; i < len; ++i) {
            uint8_t bit;

            crc ^= data[i];
            for (bit = 0u; bit < 8u; ++bit) {
                crc = (crc & 0x80u) != 0u ? (uint8_t)((crc << 1) ^ 0x07u)
                                          : (uint8_t)(crc << 1);
            }
        }
        accumulator = crc;
        break;
    }
    case ZB_CHECKSUM_CRC16_CCITT:
#if ZB_ENABLE_CRC16
    {
        uint16_t crc = accumulator;

        for (i = 0u; i < len; ++i) {
            uint8_t bit;

            crc ^= (uint16_t)((uint16_t)data[i] << 8);
            for (bit = 0u; bit < 8u; ++bit) {
                crc = (crc & 0x8000u) != 0u
                          ? (uint16_t)((crc << 1) ^ 0x1021u)
                          : (uint16_t)(crc << 1);
            }
        }
        accumulator = crc;
        break;
    }
#endif
    case ZB_CHECKSUM_NONE:
    default:
        break;
    }
    return accumulator;
#else
    (void)type;
    (void)accumulator;
    (void)data;
    (void)len;
    return 0u;
#endif
}

static uint16_t zb_compute_checksum(const zb_link_t *link,
                                    const uint8_t *length_bytes,
                                    const uint8_t *payload, size_t payload_len)
{
    const zb_config_t *config = &link->config;
    uint16_t accumulator = zb_checksum_start(config->checksum);

    if (config->checksum_covers_header) {
        accumulator =
            zb_checksum_update(config->checksum, accumulator, config->sop,
                               config->sop_len);
    }
    accumulator = zb_checksum_update(config->checksum, accumulator,
                                     length_bytes, config->length_bytes);
    accumulator =
        zb_checksum_update(config->checksum, accumulator, payload, payload_len);
    return accumulator;
}

/* ------------------------------------------------------------------------- */
/* 长度字段与转义                                                            */
/* ------------------------------------------------------------------------- */

static uint16_t zb_read_length(const uint8_t *bytes, uint8_t length_bytes)
{
    if (length_bytes == 2u) {
        return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
    }
    return bytes[0];
}

static void zb_write_length(uint8_t *bytes, uint8_t length_bytes,
                            uint16_t value)
{
    if (length_bytes == 2u) {
        bytes[0] = (uint8_t)(value >> 8);
        bytes[1] = (uint8_t)(value & 0xFFu);
    } else {
        bytes[0] = (uint8_t)(value & 0xFFu);
    }
}

/**
 * @brief 输入字节 → 未转义字节。
 * @return false 表示该字节是转义前缀，需要等待下一个字节。
 */
static bool zb_unescape(zb_link_t *link, uint8_t raw, uint8_t *decoded)
{
#if ZB_ENABLE_ESCAPE
    if (link->config.escape_enabled) {
        if (link->escape_pending) {
            link->escape_pending = false;
            *decoded = (uint8_t)(raw ^ link->config.escape_xor);
            return true;
        }
        if (raw == link->config.escape_byte) {
            link->escape_pending = true;
            return false;
        }
    }
#else
    (void)link;
#endif
    *decoded = raw;
    return true;
}

static bool zb_needs_escape(const zb_config_t *config, uint8_t byte)
{
#if ZB_ENABLE_ESCAPE
    uint8_t i;

    if (!config->escape_enabled) {
        return false;
    }
    if (byte == config->escape_byte) {
        return true;
    }
    for (i = 0u; i < config->sop_len; ++i) {
        if (byte == config->sop[i]) {
            return true;
        }
    }
    for (i = 0u; i < config->eop_len; ++i) {
        if (byte == config->eop[i]) {
            return true;
        }
    }
    return false;
#else
    (void)config;
    (void)byte;
    return false;
#endif
}

static bool zb_put_escaped(zb_link_t *link, uint8_t byte, size_t *position)
{
    if (zb_needs_escape(&link->config, byte)) {
        if (*position + 2u > sizeof(link->tx_buffer)) {
            return false;
        }
        link->tx_buffer[(*position)++] = link->config.escape_byte;
        link->tx_buffer[(*position)++] =
            (uint8_t)(byte ^ link->config.escape_xor);
    } else {
        if (*position + 1u > sizeof(link->tx_buffer)) {
            return false;
        }
        link->tx_buffer[(*position)++] = byte;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/* 解析状态机                                                                */
/* ------------------------------------------------------------------------- */

static void zb_frame_clear(zb_link_t *link)
{
    link->state = (uint8_t)ZB_STATE_SOP;
    link->sop_pos = 0u;
    link->len_received = 0u;
    link->payload_len = 0u;
    link->body_expected = 0u;
    link->body_received = 0u;
    link->eop_received = 0u;
    link->escape_pending = false;
}

static void zb_report_error(zb_link_t *link, zb_error_t error)
{
    switch (error) {
    case ZB_ERROR_CHECKSUM:
        link->stats.checksum_errors++;
        break;
    case ZB_ERROR_OVERFLOW:
        link->stats.overflow_errors++;
        break;
    case ZB_ERROR_FRAMING:
        link->stats.framing_errors++;
        break;
    default:
        break;
    }

    if (link->config.on_error != NULL) {
        link->config.on_error(link->config.user, error);
    }
}

static void zb_finish_frame(zb_link_t *link)
{
    uint8_t checksum_size = zb_checksum_size(link->config.checksum);

    if (checksum_size != 0u) {
        uint16_t expected =
            zb_compute_checksum(link, link->len_bytes, link->body,
                                link->payload_len);
        uint16_t received;

        if (checksum_size == 2u) {
            received = (uint16_t)(((uint16_t)link->body[link->payload_len]
                                   << 8) |
                                  link->body[link->payload_len + 1u]);
        } else {
            received = link->body[link->payload_len];
        }

        if (expected != received) {
            zb_report_error(link, ZB_ERROR_CHECKSUM);
            zb_frame_clear(link);
            return;
        }
    }

    link->stats.frames_ok++;
    if (link->config.on_frame != NULL) {
        link->config.on_frame(link->config.user, link->body,
                              link->payload_len);
    }
    zb_frame_clear(link);
}

static void zb_after_body(zb_link_t *link)
{
    if (link->config.eop_len == 0u) {
        zb_finish_frame(link);
    } else {
        link->state = (uint8_t)ZB_STATE_EOP;
        link->eop_received = 0u;
    }
}

static void zb_process_sop_byte(zb_link_t *link, uint8_t byte)
{
    const zb_config_t *config = &link->config;

    if (byte == config->sop[link->sop_pos]) {
        link->sop_pos++;
        if (link->sop_pos >= config->sop_len) {
            link->sop_pos = 0u;
            link->state = (uint8_t)ZB_STATE_LEN;
            link->len_received = 0u;
        }
    } else {
        link->stats.discarded_bytes++;
        link->sop_pos = (byte == config->sop[0]) ? 1u : 0u;
    }
}

static void zb_process_framed_byte(zb_link_t *link, uint8_t byte)
{
    const zb_config_t *config = &link->config;

    switch (link->state) {
    case ZB_STATE_SOP:
        zb_process_sop_byte(link, byte);
        break;

    case ZB_STATE_LEN: {
        uint8_t decoded;
        uint16_t payload_len;
        uint8_t checksum_size;

        if (!zb_unescape(link, byte, &decoded)) {
            break;
        }
        link->len_bytes[link->len_received++] = decoded;
        if (link->len_received < config->length_bytes) {
            break;
        }

        payload_len = zb_read_length(link->len_bytes, config->length_bytes);
        checksum_size = zb_checksum_size(config->checksum);

        if (payload_len > ZB_MAX_PAYLOAD) {
            zb_report_error(link, ZB_ERROR_OVERFLOW);
            zb_frame_clear(link);
            break;
        }

        link->payload_len = payload_len;
        link->body_expected =
            (uint16_t)(payload_len + (uint16_t)checksum_size);
        link->body_received = 0u;

        if (link->body_expected == 0u) {
            zb_after_body(link);
        } else {
            link->state = (uint8_t)ZB_STATE_BODY;
        }
        break;
    }

    case ZB_STATE_BODY: {
        uint8_t decoded;

        if (!zb_unescape(link, byte, &decoded)) {
            break;
        }
        if (link->body_received < (uint16_t)sizeof(link->body)) {
            link->body[link->body_received++] = decoded;
        }
        if (link->body_received >= link->body_expected) {
            zb_after_body(link);
        }
        break;
    }

    case ZB_STATE_EOP:
        if (byte == config->eop[link->eop_received]) {
            link->eop_received++;
            if (link->eop_received >= config->eop_len) {
                zb_finish_frame(link);
            }
        } else {
            zb_report_error(link, ZB_ERROR_FRAMING);
            zb_frame_clear(link);
            /* 当前字节可能正是下一帧的帧头，重新参与匹配。 */
            zb_process_sop_byte(link, byte);
        }
        break;

    default:
        zb_frame_clear(link);
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* 公开 API                                                                  */
/* ------------------------------------------------------------------------- */

void zb_config_default(zb_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->mode = ZB_MODE_FRAMED;
    config->sop[0] = 0xFAu;
    config->sop_len = 1u;
    config->eop[0] = 0xFBu;
    config->eop_len = 1u;
    config->length_bytes = 1u;
    config->checksum = ZB_CHECKSUM_SUM8;
    config->checksum_covers_header = false;
    config->escape_enabled = false;
    config->escape_byte = 0x7Du;
    config->escape_xor = 0x20u;
}

zb_status_t zb_init(zb_link_t *link, const zb_config_t *config,
                    const zb_port_t *port)
{
    if (link == NULL || config == NULL) {
        return ZB_ERR_PARAM;
    }
    if (port != NULL && port->write == NULL) {
        return ZB_ERR_PARAM;
    }

    if (config->mode != ZB_MODE_FRAMED && config->mode != ZB_MODE_RAW) {
        return ZB_ERR_PARAM;
    }

    if (config->mode == ZB_MODE_FRAMED) {
        if (config->sop_len == 0u || config->sop_len > ZB_MAX_SOP_SIZE) {
            return ZB_ERR_PARAM;
        }
        if (config->eop_len > ZB_MAX_EOP_SIZE) {
            return ZB_ERR_PARAM;
        }
        if (config->length_bytes == 0u ||
            config->length_bytes > ZB_MAX_LENGTH_SIZE) {
            return ZB_ERR_PARAM;
        }
        if (config->checksum < ZB_CHECKSUM_NONE ||
            config->checksum > ZB_CHECKSUM_CRC16_CCITT) {
            return ZB_ERR_PARAM;
        }
    }

#if !ZB_ENABLE_CHECKSUM
    if (config->checksum != ZB_CHECKSUM_NONE) {
        return ZB_ERR_PARAM;
    }
#endif
#if !ZB_ENABLE_CRC16
    if (config->checksum == ZB_CHECKSUM_CRC16_CCITT) {
        return ZB_ERR_PARAM;
    }
#endif
#if !ZB_ENABLE_ESCAPE
    if (config->escape_enabled) {
        return ZB_ERR_PARAM;
    }
#endif
#if !ZB_ENABLE_RAW_MODE
    if (config->mode == ZB_MODE_RAW) {
        return ZB_ERR_PARAM;
    }
#endif

    memset(link, 0, sizeof(*link));
    link->config = *config;
    if (port != NULL) {
        link->port = *port;
    }
    link->initialized = true;
    zb_frame_clear(link);
    return ZB_OK;
}

zb_status_t zb_reset(zb_link_t *link)
{
    if (link == NULL || !link->initialized) {
        return ZB_ERR_STATE;
    }

    zb_frame_clear(link);
    memset(&link->stats, 0, sizeof(link->stats));
    return ZB_OK;
}

zb_status_t zb_feed_byte(zb_link_t *link, uint8_t byte)
{
    if (link == NULL || !link->initialized) {
        return ZB_ERR_STATE;
    }

    if (link->config.mode == ZB_MODE_RAW) {
        link->stats.raw_chunks++;
        if (link->config.on_raw != NULL) {
            link->config.on_raw(link->config.user, &byte, 1u);
        }
        return ZB_OK;
    }

    zb_process_framed_byte(link, byte);
    return ZB_OK;
}

zb_status_t zb_feed(zb_link_t *link, const uint8_t *data, size_t len)
{
    size_t i;

    if (link == NULL || !link->initialized) {
        return ZB_ERR_STATE;
    }
    if (data == NULL && len != 0u) {
        return ZB_ERR_PARAM;
    }

    if (link->config.mode == ZB_MODE_RAW) {
        link->stats.raw_chunks++;
        if (link->config.on_raw != NULL) {
            link->config.on_raw(link->config.user, data, len);
        }
        return ZB_OK;
    }

    for (i = 0u; i < len; ++i) {
        zb_process_framed_byte(link, data[i]);
    }
    return ZB_OK;
}

zb_status_t zb_send(zb_link_t *link, const uint8_t *payload, size_t len)
{
    const zb_config_t *config;
    uint8_t length_bytes[ZB_MAX_LENGTH_SIZE];
    uint8_t checksum_size;
    uint16_t checksum = 0u;
    size_t position = 0u;
    size_t i;

    if (link == NULL || !link->initialized) {
        return ZB_ERR_STATE;
    }
    if (payload == NULL && len != 0u) {
        return ZB_ERR_PARAM;
    }
    if (len > ZB_MAX_PAYLOAD) {
        return ZB_ERR_NO_SPACE;
    }
    if (link->port.write == NULL) {
        return ZB_ERR_PARAM;
    }

    config = &link->config;

    if (config->mode == ZB_MODE_RAW) {
        return link->port.write(link->port.user, payload, len) ? ZB_OK
                                                               : ZB_ERR_IO;
    }

    if (config->length_bytes == 1u && len > 255u) {
        return ZB_ERR_NO_SPACE;
    }

    zb_write_length(length_bytes, config->length_bytes, (uint16_t)len);
    checksum_size = zb_checksum_size(config->checksum);
    if (checksum_size != 0u) {
        checksum = zb_compute_checksum(link, length_bytes, payload, len);
    }

    /* SOP 原样发送。 */
    for (i = 0u; i < config->sop_len; ++i) {
        link->tx_buffer[position++] = config->sop[i];
    }

    /* 长度、负载、校验允许转义。 */
    for (i = 0u; i < config->length_bytes; ++i) {
        if (!zb_put_escaped(link, length_bytes[i], &position)) {
            return ZB_ERR_NO_SPACE;
        }
    }
    for (i = 0u; i < len; ++i) {
        if (!zb_put_escaped(link, payload[i], &position)) {
            return ZB_ERR_NO_SPACE;
        }
    }
    if (checksum_size == 2u) {
        if (!zb_put_escaped(link, (uint8_t)(checksum >> 8), &position) ||
            !zb_put_escaped(link, (uint8_t)(checksum & 0xFFu), &position)) {
            return ZB_ERR_NO_SPACE;
        }
    } else if (checksum_size == 1u) {
        if (!zb_put_escaped(link, (uint8_t)(checksum & 0xFFu), &position)) {
            return ZB_ERR_NO_SPACE;
        }
    }

    /* EOP 原样发送。 */
    for (i = 0u; i < config->eop_len; ++i) {
        link->tx_buffer[position++] = config->eop[i];
    }

    return link->port.write(link->port.user, link->tx_buffer, position)
               ? ZB_OK
               : ZB_ERR_IO;
}

void zb_get_stats(const zb_link_t *link, zb_stats_t *out)
{
    if (link == NULL || out == NULL) {
        return;
    }
    *out = link->stats;
}
