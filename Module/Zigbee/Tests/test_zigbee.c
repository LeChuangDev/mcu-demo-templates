/**
 * @file  test_zigbee.c
 * @brief Zigbee 通用帧协议主机端测试。
 */
#include "zigbee.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);      \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

/* ------------------------------------------------------------------------- */
/* 测试夹具                                                                  */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint8_t data[1024];
    size_t len;
    int writes;
    bool fail;
} capture_t;

static bool capture_write(void *user, const uint8_t *data, size_t len)
{
    capture_t *capture = (capture_t *)user;

    if (capture == NULL) {
        return false;
    }
    capture->writes++;
    if (capture->fail) {
        return false;
    }
    if (len > sizeof(capture->data) - capture->len) {
        return false;
    }
    if (len != 0u) {
        memcpy(capture->data + capture->len, data, len);
    }
    capture->len += len;
    return true;
}

typedef struct {
    uint8_t payload[ZB_MAX_PAYLOAD];
    size_t len;
    int frames;
    uint8_t raw[64];
    size_t raw_len;
    int raw_calls;
    int errors;
    zb_error_t last_error;
} sink_t;

static sink_t g_sink;

static void sink_on_frame(void *user, const uint8_t *payload, size_t len)
{
    sink_t *sink = (sink_t *)user;

    sink->frames++;
    if (len <= sizeof(sink->payload)) {
        if (len != 0u) {
            memcpy(sink->payload, payload, len);
        }
        sink->len = len;
    }
}

static void sink_on_raw(void *user, const uint8_t *data, size_t len)
{
    sink_t *sink = (sink_t *)user;

    sink->raw_calls++;
    sink->raw_len = len;
    if (len <= sizeof(sink->raw)) {
        if (len != 0u) {
            memcpy(sink->raw, data, len);
        }
    }
}

static void sink_on_error(void *user, zb_error_t error)
{
    sink_t *sink = (sink_t *)user;

    sink->errors++;
    sink->last_error = error;
}

static void config_with_sink(zb_config_t *config)
{
    zb_config_default(config);
    config->on_frame = sink_on_frame;
    config->on_raw = sink_on_raw;
    config->on_error = sink_on_error;
    config->user = &g_sink;
}

static void init_receiver(zb_link_t *rx, zb_config_t *config)
{
    memset(&g_sink, 0, sizeof(g_sink));
    config->on_frame = sink_on_frame;
    config->on_raw = sink_on_raw;
    config->on_error = sink_on_error;
    config->user = &g_sink;
    CHECK(zb_init(rx, config, NULL) == ZB_OK);
}

/** 发送一次，再让接收端把线上字节完整解析一遍。 */
static void run_loopback(zb_config_t *config, const uint8_t *payload, size_t len)
{
    zb_link_t tx;
    zb_link_t rx;
    capture_t capture;
    zb_port_t port;
    zb_stats_t stats;

    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;

    CHECK(zb_init(&tx, config, &port) == ZB_OK);
    init_receiver(&rx, config);
    CHECK(zb_send(&tx, payload, len) == ZB_OK);

    CHECK(capture.len > 0u);
    CHECK(zb_feed(&rx, capture.data, capture.len) == ZB_OK);

    CHECK(g_sink.frames == 1);
    CHECK(g_sink.len == len);
    if (len != 0u) {
        CHECK(memcmp(g_sink.payload, payload, len) == 0);
    }

    zb_get_stats(&rx, &stats);
    CHECK(stats.frames_ok == 1u);
    CHECK(stats.checksum_errors == 0u);
    CHECK(stats.overflow_errors == 0u);
    CHECK(stats.framing_errors == 0u);
}

/* 独立的参考校验实现，避免收发同源掩盖错误。 */
static uint8_t ref_sum8(const uint8_t *data, size_t len)
{
    uint8_t sum = 0u;
    size_t i;

    for (i = 0u; i < len; ++i) {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

static uint8_t ref_xor8(const uint8_t *data, size_t len)
{
    uint8_t value = 0u;
    size_t i;

    for (i = 0u; i < len; ++i) {
        value ^= data[i];
    }
    return value;
}

static uint8_t ref_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0u;
    size_t i;
    unsigned bit;

    for (i = 0u; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 0x80u) != 0u ? (uint8_t)((crc << 1) ^ 0x07u)
                                      : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t ref_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    size_t i;
    unsigned bit;

    for (i = 0u; i < len; ++i) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) != 0u ? (uint16_t)((crc << 1) ^ 0x1021u)
                                        : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/* ------------------------------------------------------------------------- */
/* 用例                                                                      */
/* ------------------------------------------------------------------------- */

static void test_default_config_and_golden(void)
{
    zb_config_t config;
    zb_link_t tx;
    zb_link_t rx;
    capture_t capture;
    zb_port_t port;
    const uint8_t payload[] = {0x01u, 0x02u, 0x03u};
    const uint8_t expected[] = {0xFAu, 0x03u, 0x01u, 0x02u, 0x03u, 0x09u,
                                0xFBu};

    zb_config_default(&config);
    CHECK(config.mode == ZB_MODE_FRAMED);
    CHECK(config.sop_len == 1u && config.sop[0] == 0xFAu);
    CHECK(config.eop_len == 1u && config.eop[0] == 0xFBu);
    CHECK(config.length_bytes == 1u);
    CHECK(config.checksum == ZB_CHECKSUM_SUM8);
    CHECK(!config.checksum_covers_header);
    CHECK(!config.escape_enabled);
    CHECK(config.escape_byte == 0x7Du);
    CHECK(config.escape_xor == 0x20u);

    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);
    CHECK(capture.len == sizeof(expected));
    CHECK(memcmp(capture.data, expected, sizeof(expected)) == 0);

    init_receiver(&rx, &config);
    CHECK(zb_feed(&rx, expected, sizeof(expected)) == ZB_OK);
    CHECK(g_sink.frames == 1);
    CHECK(g_sink.len == sizeof(payload));
    CHECK(memcmp(g_sink.payload, payload, sizeof(payload)) == 0);
}

static void test_sum8_roundtrip(void)
{
    zb_config_t config;
    const uint8_t payload[] = {0x10u, 0x20u, 0x30u, 0x40u};

    config_with_sink(&config);
    run_loopback(&config, payload, sizeof(payload));
}

static void test_sum8_covers_header(void)
{
    zb_config_t config;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    const uint8_t payload[] = {0x01u, 0x02u};
    uint8_t reference[1u + 1u + sizeof(payload)];

    config_with_sink(&config);
    config.checksum_covers_header = true;

    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);

    reference[0] = 0xFAu;
    reference[1] = (uint8_t)sizeof(payload);
    memcpy(&reference[2], payload, sizeof(payload));
    CHECK(capture.len == sizeof(payload) + 4u);
    CHECK(capture.data[2u + sizeof(payload)] ==
          ref_sum8(reference, sizeof(reference)));

    run_loopback(&config, payload, sizeof(payload));
}

static void test_checksum_error_and_recovery(void)
{
    zb_config_t config;
    zb_link_t rx;
    zb_stats_t stats;
    const uint8_t bad[] = {0xFAu, 0x01u, 0x42u, 0x00u, 0xFBu};
    const uint8_t good[] = {0xFAu, 0x01u, 0x42u, 0x43u, 0xFBu};

    config_with_sink(&config);
    init_receiver(&rx, &config);

    CHECK(zb_feed(&rx, bad, sizeof(bad)) == ZB_OK);
    CHECK(g_sink.frames == 0);
    CHECK(g_sink.errors == 1);
    CHECK(g_sink.last_error == ZB_ERROR_CHECKSUM);
    zb_get_stats(&rx, &stats);
    CHECK(stats.checksum_errors == 1u);
    CHECK(stats.frames_ok == 0u);

    CHECK(zb_feed(&rx, good, sizeof(good)) == ZB_OK);
    CHECK(g_sink.frames == 1);
    CHECK(g_sink.len == 1u);
    CHECK(g_sink.payload[0] == 0x42u);
}

static void test_noise_resync(void)
{
    zb_config_t config;
    zb_link_t rx;
    zb_stats_t stats;
    const uint8_t stream[] = {0x00u, 0x11u, 0xFAu, 0x01u, 0x42u,
                              0x43u, 0xFBu};

    config_with_sink(&config);
    init_receiver(&rx, &config);

    CHECK(zb_feed(&rx, stream, sizeof(stream)) == ZB_OK);
    CHECK(g_sink.frames == 1);
    CHECK(g_sink.len == 1u && g_sink.payload[0] == 0x42u);
    zb_get_stats(&rx, &stats);
    CHECK(stats.discarded_bytes == 2u);
}

static void test_multibyte_sop_resync(void)
{
    zb_config_t config;
    zb_link_t rx;
    const uint8_t stream[] = {0xAAu, 0xAAu, 0x55u, 0x01u, 0x42u,
                              0x0Du, 0x0Au};

    config_with_sink(&config);
    config.sop[0] = 0xAAu;
    config.sop[1] = 0x55u;
    config.sop_len = 2u;
    config.eop[0] = 0x0Du;
    config.eop[1] = 0x0Au;
    config.eop_len = 2u;
    config.checksum = ZB_CHECKSUM_NONE;

    init_receiver(&rx, &config);
    CHECK(zb_feed(&rx, stream, sizeof(stream)) == ZB_OK);
    CHECK(g_sink.frames == 1);
    CHECK(g_sink.len == 1u && g_sink.payload[0] == 0x42u);

    run_loopback(&config, stream + 3, 1u);
}

static void test_escape_roundtrip_and_golden(void)
{
    zb_config_t config;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    const uint8_t payload[] = {0xFAu, 0xFBu, 0x7Du, 0x00u};
    const uint8_t expected[] = {0xFAu, 0x04u, 0x7Du, 0xDAu, 0x7Du, 0xDBu,
                                0x7Du, 0x5Du, 0x00u, 0x76u, 0xFBu};

    config_with_sink(&config);
    config.escape_enabled = true;

    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);
    CHECK(capture.len == sizeof(expected));
    CHECK(memcmp(capture.data, expected, sizeof(expected)) == 0);

    run_loopback(&config, payload, sizeof(payload));
}

static void test_escape_length_byte(void)
{
    zb_config_t config;
    const uint8_t payload[] = {0x10u, 0x20u};

    config_with_sink(&config);
    config.escape_enabled = true;
    config.escape_byte = 0x02u;
    config.escape_xor = 0x20u;

    run_loopback(&config, payload, sizeof(payload));
}

static void test_checksum_none_without_eop(void)
{
    zb_config_t config;
    zb_link_t tx;
    zb_link_t rx;
    capture_t capture;
    zb_port_t port;
    const uint8_t payload[] = {0x11u, 0x22u};
    const uint8_t expected[] = {0xFAu, 0x02u, 0x11u, 0x22u};

    config_with_sink(&config);
    config.checksum = ZB_CHECKSUM_NONE;
    config.eop_len = 0u;

    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);
    CHECK(capture.len == sizeof(expected));
    CHECK(memcmp(capture.data, expected, sizeof(expected)) == 0);

    init_receiver(&rx, &config);
    CHECK(zb_feed(&rx, expected, sizeof(expected)) == ZB_OK);
    CHECK(g_sink.frames == 1);
    CHECK(g_sink.len == sizeof(payload));
    CHECK(memcmp(g_sink.payload, payload, sizeof(payload)) == 0);
}

static void test_zero_payload(void)
{
    zb_config_t config;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    const uint8_t expected[] = {0xFAu, 0x00u, 0x00u, 0xFBu};

    config_with_sink(&config);
    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, NULL, 0u) == ZB_OK);
    CHECK(capture.len == sizeof(expected));
    CHECK(memcmp(capture.data, expected, sizeof(expected)) == 0);

    run_loopback(&config, NULL, 0u);
    CHECK(g_sink.frames == 1);
    CHECK(g_sink.len == 0u);
}

static void test_two_byte_length_crc16(void)
{
    zb_config_t config;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    const uint8_t payload[] = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u};
    uint8_t reference[1u + 2u + sizeof(payload)];
    uint16_t crc;

    config_with_sink(&config);
    config.length_bytes = 2u;
    config.checksum = ZB_CHECKSUM_CRC16_CCITT;
    config.checksum_covers_header = true;
    config.eop[0] = 0x0Du;
    config.eop[1] = 0x0Au;
    config.eop_len = 2u;

    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);

    CHECK(capture.len == 1u + 2u + sizeof(payload) + 2u + 2u);
    CHECK(capture.data[0] == 0xFAu);
    CHECK(capture.data[1] == 0x00u);
    CHECK(capture.data[2] == (uint8_t)sizeof(payload));
    CHECK(capture.data[capture.len - 2u] == 0x0Du);
    CHECK(capture.data[capture.len - 1u] == 0x0Au);

    reference[0] = 0xFAu;
    reference[1] = 0x00u;
    reference[2] = (uint8_t)sizeof(payload);
    memcpy(&reference[3], payload, sizeof(payload));
    crc = ref_crc16(reference, sizeof(reference));
    CHECK(capture.data[3u + sizeof(payload)] == (uint8_t)(crc >> 8));
    CHECK(capture.data[4u + sizeof(payload)] == (uint8_t)(crc & 0xFFu));

    run_loopback(&config, payload, sizeof(payload));
}

static void test_crc16_without_header(void)
{
    zb_config_t config;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    const uint8_t payload[] = {0x11u};
    uint8_t reference[1u + sizeof(payload)];
    uint16_t crc;

    config_with_sink(&config);
    config.checksum = ZB_CHECKSUM_CRC16_CCITT;
    config.checksum_covers_header = false;
    config.eop_len = 0u;

    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);

    reference[0] = (uint8_t)sizeof(payload);
    reference[1] = payload[0];
    crc = ref_crc16(reference, sizeof(reference));
    CHECK(capture.len == 1u + 1u + 1u + 2u);
    CHECK(capture.data[3] == (uint8_t)(crc >> 8));
    CHECK(capture.data[4] == (uint8_t)(crc & 0xFFu));

    run_loopback(&config, payload, sizeof(payload));
}

static void test_xor8_and_crc8(void)
{
    zb_config_t config;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    const uint8_t xor_payload[] = {0xAAu, 0x55u, 0x01u};
    const uint8_t crc_payload[] = {0x01u, 0x02u, 0x03u};
    uint8_t reference[4];

    config_with_sink(&config);
    config.checksum = ZB_CHECKSUM_XOR8;
    reference[0] = (uint8_t)sizeof(xor_payload);
    memcpy(&reference[1], xor_payload, sizeof(xor_payload));
    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, xor_payload, sizeof(xor_payload)) == ZB_OK);
    CHECK(capture.data[2u + sizeof(xor_payload)] ==
          ref_xor8(reference, 1u + sizeof(xor_payload)));
    run_loopback(&config, xor_payload, sizeof(xor_payload));

    config_with_sink(&config);
    config.checksum = ZB_CHECKSUM_CRC8;
    reference[0] = (uint8_t)sizeof(crc_payload);
    memcpy(&reference[1], crc_payload, sizeof(crc_payload));
    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, crc_payload, sizeof(crc_payload)) == ZB_OK);
    CHECK(capture.data[2u + sizeof(crc_payload)] ==
          ref_crc8(reference, 1u + sizeof(crc_payload)));
    run_loopback(&config, crc_payload, sizeof(crc_payload));
}

static void test_overflow_error(void)
{
    zb_config_t config;
    zb_link_t rx;
    zb_stats_t stats;
    const uint8_t overflow[] = {0xFAu, 0xFFu, 0x00u};
    const uint8_t good[] = {0xFAu, 0x01u, 0x42u, 0x43u, 0xFBu};

    config_with_sink(&config);
    init_receiver(&rx, &config);

    CHECK(zb_feed(&rx, overflow, sizeof(overflow)) == ZB_OK);
    CHECK(g_sink.frames == 0);
    CHECK(g_sink.errors == 1);
    CHECK(g_sink.last_error == ZB_ERROR_OVERFLOW);
    zb_get_stats(&rx, &stats);
    CHECK(stats.overflow_errors == 1u);

    CHECK(zb_feed(&rx, good, sizeof(good)) == ZB_OK);
    CHECK(g_sink.frames == 1);
}

static void test_framing_error_and_resync(void)
{
    zb_config_t config;
    zb_link_t rx;
    zb_stats_t stats;
    const uint8_t bad_eop[] = {0xFAu, 0x01u, 0x42u, 0x43u, 0x00u};
    const uint8_t good[] = {0xFAu, 0x01u, 0x43u, 0x44u, 0xFBu};

    config_with_sink(&config);
    init_receiver(&rx, &config);

    CHECK(zb_feed(&rx, bad_eop, sizeof(bad_eop)) == ZB_OK);
    CHECK(g_sink.frames == 0);
    CHECK(g_sink.errors == 1);
    CHECK(g_sink.last_error == ZB_ERROR_FRAMING);
    zb_get_stats(&rx, &stats);
    CHECK(stats.framing_errors == 1u);

    CHECK(zb_feed(&rx, good, sizeof(good)) == ZB_OK);
    CHECK(g_sink.frames == 1);
    CHECK(g_sink.payload[0] == 0x43u);
}

static void test_raw_mode(void)
{
    zb_config_t config;
    zb_link_t rx;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    zb_stats_t stats;
    const uint8_t payload[] = {0xDEu, 0xADu, 0xFAu};

    config_with_sink(&config);
    config.mode = ZB_MODE_RAW;

    init_receiver(&rx, &config);
    CHECK(zb_feed(&rx, payload, sizeof(payload)) == ZB_OK);
    CHECK(g_sink.raw_calls == 1);
    CHECK(g_sink.raw_len == sizeof(payload));
    CHECK(memcmp(g_sink.raw, payload, sizeof(payload)) == 0);
    CHECK(g_sink.frames == 0);
    zb_get_stats(&rx, &stats);
    CHECK(stats.raw_chunks == 1u);

    CHECK(zb_feed_byte(&rx, 0x55u) == ZB_OK);
    CHECK(g_sink.raw_calls == 2);
    CHECK(g_sink.raw_len == 1u);
    CHECK(g_sink.raw[0] == 0x55u);

    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);
    CHECK(capture.len == sizeof(payload));
    CHECK(memcmp(capture.data, payload, sizeof(payload)) == 0);

    CHECK(zb_send(&tx, NULL, 0u) == ZB_OK);
    CHECK(capture.len == sizeof(payload));
    CHECK(capture.writes == 2);
}

static void test_batch_frames(void)
{
    zb_config_t config;
    zb_link_t tx;
    zb_link_t rx;
    capture_t capture;
    zb_port_t port;
    zb_stats_t stats;
    const uint8_t first[] = {0x01u};
    const uint8_t second[] = {0x02u, 0x03u};

    config_with_sink(&config);
    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, first, sizeof(first)) == ZB_OK);
    CHECK(zb_send(&tx, second, sizeof(second)) == ZB_OK);

    init_receiver(&rx, &config);
    CHECK(zb_feed(&rx, capture.data, capture.len) == ZB_OK);
    CHECK(g_sink.frames == 2);
    CHECK(g_sink.len == sizeof(second));
    CHECK(memcmp(g_sink.payload, second, sizeof(second)) == 0);

    zb_get_stats(&rx, &stats);
    CHECK(stats.frames_ok == 2u);
}

static void test_byte_by_byte(void)
{
    zb_config_t config;
    zb_link_t tx;
    zb_link_t rx;
    capture_t capture;
    zb_port_t port;
    const uint8_t payload[] = {0x33u, 0x44u};
    size_t i;

    config_with_sink(&config);
    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);

    init_receiver(&rx, &config);
    for (i = 0u; i < capture.len; ++i) {
        CHECK(zb_feed_byte(&rx, capture.data[i]) == ZB_OK);
    }
    CHECK(g_sink.frames == 1);
    CHECK(g_sink.len == sizeof(payload));
    CHECK(memcmp(g_sink.payload, payload, sizeof(payload)) == 0);
}

static void test_stats_reset(void)
{
    zb_config_t config;
    zb_link_t rx;
    zb_stats_t stats;
    const uint8_t noise[] = {0x00u};
    const uint8_t good[] = {0xFAu, 0x01u, 0x42u, 0x43u, 0xFBu};

    config_with_sink(&config);
    init_receiver(&rx, &config);
    CHECK(zb_feed(&rx, noise, sizeof(noise)) == ZB_OK);
    CHECK(zb_feed(&rx, good, sizeof(good)) == ZB_OK);

    zb_get_stats(&rx, &stats);
    CHECK(stats.frames_ok == 1u);
    CHECK(stats.discarded_bytes == 1u);

    CHECK(zb_reset(&rx) == ZB_OK);
    zb_get_stats(&rx, &stats);
    CHECK(stats.frames_ok == 0u);
    CHECK(stats.discarded_bytes == 0u);

    CHECK(zb_feed(&rx, good, sizeof(good)) == ZB_OK);
    zb_get_stats(&rx, &stats);
    CHECK(stats.frames_ok == 1u);
}

static void test_payload_with_sop_bytes(void)
{
    zb_config_t config;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    const uint8_t payload[] = {0xFAu, 0xFBu};
    const uint8_t expected[] = {0xFAu, 0x02u, 0xFAu, 0xFBu,
                                0xF7u, 0xFBu};

    config_with_sink(&config);
    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;
    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    CHECK(zb_send(&tx, payload, sizeof(payload)) == ZB_OK);
    CHECK(capture.len == sizeof(expected));
    CHECK(memcmp(capture.data, expected, sizeof(expected)) == 0);

    run_loopback(&config, payload, sizeof(payload));
}

static void test_no_callbacks(void)
{
    zb_config_t config;
    zb_link_t rx;
    zb_stats_t stats;
    const uint8_t frame[] = {0xFAu, 0x02u, 0x01u, 0x02u};

    zb_config_default(&config);
    config.checksum = ZB_CHECKSUM_NONE;
    config.eop_len = 0u;

    CHECK(zb_init(&rx, &config, NULL) == ZB_OK);
    CHECK(zb_feed(&rx, frame, sizeof(frame)) == ZB_OK);
    zb_get_stats(&rx, &stats);
    CHECK(stats.frames_ok == 1u);
}

static void test_invalid_parameters(void)
{
    zb_config_t config;
    zb_config_t bad;
    zb_link_t link;
    zb_link_t uninitialized;
    zb_port_t bad_port;
    zb_stats_t stats;
    uint8_t big[ZB_MAX_PAYLOAD + 1u];

    zb_config_default(NULL); /* 不应崩溃。 */

    zb_config_default(&config);
    CHECK(zb_init(NULL, &config, NULL) == ZB_ERR_PARAM);
    CHECK(zb_init(&link, NULL, NULL) == ZB_ERR_PARAM);

    bad_port.write = NULL;
    bad_port.user = NULL;
    CHECK(zb_init(&link, &config, &bad_port) == ZB_ERR_PARAM);

    zb_config_default(&bad);
    bad.mode = (zb_mode_t)9;
    CHECK(zb_init(&link, &bad, NULL) == ZB_ERR_PARAM);

    zb_config_default(&bad);
    bad.sop_len = 0u;
    CHECK(zb_init(&link, &bad, NULL) == ZB_ERR_PARAM);
    zb_config_default(&bad);
    bad.sop_len = ZB_MAX_SOP_SIZE + 1u;
    CHECK(zb_init(&link, &bad, NULL) == ZB_ERR_PARAM);

    zb_config_default(&bad);
    bad.eop_len = ZB_MAX_EOP_SIZE + 1u;
    CHECK(zb_init(&link, &bad, NULL) == ZB_ERR_PARAM);

    zb_config_default(&bad);
    bad.length_bytes = 0u;
    CHECK(zb_init(&link, &bad, NULL) == ZB_ERR_PARAM);
    zb_config_default(&bad);
    bad.length_bytes = ZB_MAX_LENGTH_SIZE + 1u;
    CHECK(zb_init(&link, &bad, NULL) == ZB_ERR_PARAM);

    zb_config_default(&bad);
    bad.checksum = (zb_checksum_t)9;
    CHECK(zb_init(&link, &bad, NULL) == ZB_ERR_PARAM);

    memset(&uninitialized, 0, sizeof(uninitialized));
    CHECK(zb_reset(&uninitialized) == ZB_ERR_STATE);
    CHECK(zb_feed_byte(&uninitialized, 0u) == ZB_ERR_STATE);
    CHECK(zb_feed(&uninitialized, big, 1u) == ZB_ERR_STATE);
    CHECK(zb_send(&uninitialized, NULL, 0u) == ZB_ERR_STATE);

    CHECK(zb_feed(NULL, NULL, 0u) == ZB_ERR_STATE);
    CHECK(zb_feed_byte(NULL, 0u) == ZB_ERR_STATE);
    CHECK(zb_send(NULL, NULL, 0u) == ZB_ERR_STATE);
    CHECK(zb_reset(NULL) == ZB_ERR_STATE);

    zb_config_default(&config);
    CHECK(zb_init(&link, &config, NULL) == ZB_OK);
    CHECK(zb_feed(&link, NULL, 1u) == ZB_ERR_PARAM);
    CHECK(zb_feed(&link, NULL, 0u) == ZB_OK);
    CHECK(zb_send(&link, NULL, 1u) == ZB_ERR_PARAM);
    CHECK(zb_send(&link, NULL, 0u) == ZB_ERR_PARAM); /* 无发送端口 */

    memset(big, 0, sizeof(big));
    memset(&stats, 0, sizeof(stats));
    zb_get_stats(NULL, &stats);
    zb_get_stats(&link, NULL);
    zb_get_stats(&link, &stats);
}

static void test_send_errors(void)
{
    zb_config_t config;
    zb_link_t tx;
    capture_t capture;
    zb_port_t port;
    uint8_t big[ZB_MAX_PAYLOAD + 1u];

    config_with_sink(&config);
    memset(&capture, 0, sizeof(capture));
    port.write = capture_write;
    port.user = &capture;

    CHECK(zb_init(&tx, &config, &port) == ZB_OK);
    memset(big, 0x5A, sizeof(big));
    CHECK(zb_send(&tx, big, sizeof(big)) == ZB_ERR_NO_SPACE);
    CHECK(capture.len == 0u);

    capture.fail = true;
    CHECK(zb_send(&tx, big, 1u) == ZB_ERR_IO);
}

int main(void)
{
    test_default_config_and_golden();
    test_sum8_roundtrip();
    test_sum8_covers_header();
    test_checksum_error_and_recovery();
    test_noise_resync();
    test_multibyte_sop_resync();
    test_escape_roundtrip_and_golden();
    test_escape_length_byte();
    test_checksum_none_without_eop();
    test_zero_payload();
    test_two_byte_length_crc16();
    test_crc16_without_header();
    test_xor8_and_crc8();
    test_overflow_error();
    test_framing_error_and_resync();
    test_raw_mode();
    test_batch_frames();
    test_byte_by_byte();
    test_stats_reset();
    test_payload_with_sop_bytes();
    test_no_callbacks();
    test_invalid_parameters();
    test_send_errors();

    if (g_failures == 0) {
        printf("zigbee: all tests passed\n");
        return 0;
    }
    printf("zigbee: %d failure(s)\n", g_failures);
    return 1;
}
