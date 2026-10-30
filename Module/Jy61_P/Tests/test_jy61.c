/**
 * @file  test_jy61.c
 * @brief JY61P 核心解析器的主机端单元测试。
 *
 * 不依赖任何单片机头文件，直接使用主机 gcc/clang 构建：
 *   cc -std=c11 -Wall -Wextra -Werror -I../Inc test_jy61.c ../Src/jy61.c
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "jy61.h"

static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            failures++;                                                      \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(actual, expected, eps)                                    \
    do {                                                                     \
        double a_ = (double)(actual);                                        \
        double e_ = (double)(expected);                                      \
        if (fabs(a_ - e_) > (eps)) {                                         \
            printf("FAIL %s:%d: %s = %.6f, expected %.6f\n", __FILE__,       \
                   __LINE__, #actual, a_, e_);                              \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static void put_i16_le(uint8_t *p, int16_t value)
{
    uint16_t raw = (uint16_t)value;
    p[0] = (uint8_t)(raw & 0xFFu);
    p[1] = (uint8_t)((raw >> 8) & 0xFFu);
}

static void make_packet(uint8_t packet[JY61_PACKET_SIZE], uint8_t type,
                        int16_t x, int16_t y, int16_t z, uint16_t timestamp)
{
    uint32_t sum = 0u;
    size_t i;

    memset(packet, 0, JY61_PACKET_SIZE);
    packet[0] = JY61_PACKET_HEADER;
    packet[1] = type;
    put_i16_le(&packet[2], x);
    put_i16_le(&packet[4], y);
    put_i16_le(&packet[6], z);
    put_i16_le(&packet[8], (int16_t)timestamp);

    for (i = 0u; i < (JY61_PACKET_SIZE - 1u); ++i) {
        sum += packet[i];
    }
    packet[JY61_PACKET_SIZE - 1u] = (uint8_t)(sum & 0xFFu);
}

/* ---- 回调与临界区探针 -------------------------------------------------- */

typedef struct {
    unsigned packets;
    jy61_packet_type_t last_type;
    float last_x;
    uint16_t last_timestamp;
} callback_probe_t;

static void on_packet(void *user, const jy61_data_t *data, jy61_packet_type_t type)
{
    callback_probe_t *probe = (callback_probe_t *)user;

    probe->packets++;
    probe->last_type = type;
    probe->last_x = data->accel_g.x;
    probe->last_timestamp = data->timestamp;
}

typedef struct {
    unsigned enter;
    unsigned exit;
} lock_probe_t;

static void lock_enter(void *user)
{
    ((lock_probe_t *)user)->enter++;
}

static void lock_exit(void *user)
{
    ((lock_probe_t *)user)->exit++;
}

/* ---- 测试用例 ---------------------------------------------------------- */

static void test_default_config(void)
{
    jy61_config_t config;

    jy61_config_default(&config);
    CHECK(config.accel_g_per_lsb > 0.0f);
    CHECK(config.gyro_dps_per_lsb > 0.0f);
    CHECK(config.angle_deg_per_lsb > 0.0f);
    CHECK(config.checksum_enabled == true);
    CHECK(config.on_packet == NULL);
}

static void test_decode_all_packet_types(void)
{
    jy61_t dev;
    jy61_data_t data;
    jy61_stats_t stats;
    uint8_t packet[JY61_PACKET_SIZE];

    CHECK(jy61_init(&dev, NULL) == JY61_OK);
    CHECK(jy61_get_data(&dev, &data) == JY61_ERR_NO_DATA);

    make_packet(packet, JY61_PACKET_TYPE_ACCEL, 8192, -8192, 0, 1000);
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 1u);
    CHECK(jy61_get_data(&dev, &data) == JY61_OK);
    CHECK_NEAR(data.accel_g.x, 4.0, 1e-4);
    CHECK_NEAR(data.accel_g.y, -4.0, 1e-4);
    CHECK_NEAR(data.accel_g.z, 0.0, 1e-4);
    CHECK(data.timestamp == 1000u);
    CHECK(data.packet_count == 1u);

    make_packet(packet, JY61_PACKET_TYPE_GYRO, 16384, 0, 0, 1001);
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 1u);
    CHECK(jy61_get_data(&dev, &data) == JY61_OK);
    CHECK_NEAR(data.gyro_dps.x, 1000.0, 1e-3);

    make_packet(packet, JY61_PACKET_TYPE_ANGLE, 16384, -16384, 0, 1002);
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 1u);
    CHECK(jy61_get_data(&dev, &data) == JY61_OK);
    CHECK_NEAR(data.angle_deg.x, 90.0, 1e-3);
    CHECK_NEAR(data.angle_deg.y, -90.0, 1e-3);
    CHECK(data.packet_count == 3u);

    jy61_get_stats(&dev, &stats);
    CHECK(stats.packets == 3u);
    CHECK(stats.checksum_errors == 0u);
}

static void test_noise_resync(void)
{
    jy61_t dev;
    jy61_data_t data;
    jy61_stats_t stats;
    uint8_t packet[JY61_PACKET_SIZE];
    const uint8_t noise[] = {0x00u, 0xAAu, 0x13u, 0xFFu};

    CHECK(jy61_init(&dev, NULL) == JY61_OK);
    CHECK(jy61_feed(&dev, noise, sizeof(noise)) == 0u);
    make_packet(packet, JY61_PACKET_TYPE_ANGLE, 16384, 0, 0, 7);
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 1u);
    CHECK(jy61_get_data(&dev, &data) == JY61_OK);
    CHECK_NEAR(data.angle_deg.x, 90.0, 1e-3);

    jy61_get_stats(&dev, &stats);
    CHECK(stats.discarded_bytes == sizeof(noise));
}

static void test_checksum_error_and_recovery(void)
{
    jy61_t dev;
    jy61_stats_t stats;
    uint8_t packet[JY61_PACKET_SIZE];

    CHECK(jy61_init(&dev, NULL) == JY61_OK);

    make_packet(packet, JY61_PACKET_TYPE_ANGLE, 0, 0, 0, 0);
    packet[JY61_PACKET_SIZE - 1u] ^= 0xFFu;
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 0u);

    jy61_get_stats(&dev, &stats);
    CHECK(stats.packets == 0u);
    CHECK(stats.checksum_errors == 1u);

    make_packet(packet, JY61_PACKET_TYPE_ANGLE, 16384, 0, 0, 0);
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 1u);

    jy61_get_stats(&dev, &stats);
    CHECK(stats.packets == 1u);
}

static void test_checksum_disabled(void)
{
    jy61_t dev;
    jy61_config_t config;
    uint8_t packet[JY61_PACKET_SIZE];

    jy61_config_default(&config);
    config.checksum_enabled = false;
    CHECK(jy61_init(&dev, &config) == JY61_OK);

    make_packet(packet, JY61_PACKET_TYPE_ACCEL, 32767, 0, 0, 0);
    packet[JY61_PACKET_SIZE - 1u] = 0x00u;
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 1u);
}

static void test_callback(void)
{
    jy61_t dev;
    jy61_config_t config;
    callback_probe_t probe;
    uint8_t packet[JY61_PACKET_SIZE];

    memset(&probe, 0, sizeof(probe));
    jy61_config_default(&config);
    config.on_packet = on_packet;
    config.user = &probe;
    CHECK(jy61_init(&dev, &config) == JY61_OK);

    make_packet(packet, JY61_PACKET_TYPE_ACCEL, 8192, 0, 0, 42);
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 1u);
    CHECK(probe.packets == 1u);
    CHECK(probe.last_type == JY61_PACKET_ACCEL);
    CHECK_NEAR(probe.last_x, 4.0, 1e-4);
    CHECK(probe.last_timestamp == 42u);
}

static void test_critical_section(void)
{
    jy61_t dev;
    jy61_config_t config;
    jy61_data_t data;
    lock_probe_t lock;
    uint8_t packet[JY61_PACKET_SIZE];

    memset(&lock, 0, sizeof(lock));
    jy61_config_default(&config);
    config.critical.enter = lock_enter;
    config.critical.exit = lock_exit;
    config.critical.user = &lock;
    CHECK(jy61_init(&dev, &config) == JY61_OK);

    make_packet(packet, JY61_PACKET_TYPE_ANGLE, 0, 0, 0, 0);
    CHECK(jy61_feed(&dev, packet, sizeof(packet)) == 1u);
    CHECK(jy61_get_data(&dev, &data) == JY61_OK);
    CHECK(lock.enter == 1u);
    CHECK(lock.exit == 1u);
}

static void test_batch_feed_and_reset(void)
{
    jy61_t dev;
    jy61_stats_t stats;
    uint8_t stream[2u * JY61_PACKET_SIZE];

    CHECK(jy61_init(&dev, NULL) == JY61_OK);
    make_packet(&stream[0], JY61_PACKET_TYPE_ACCEL, 0, 0, 0, 0);
    make_packet(&stream[JY61_PACKET_SIZE], JY61_PACKET_TYPE_GYRO, 0, 0, 0, 0);
    CHECK(jy61_feed(&dev, stream, sizeof(stream)) == 2u);

    CHECK(jy61_reset(&dev) == JY61_OK);
    jy61_get_stats(&dev, &stats);
    CHECK(stats.packets == 0u);
}

static void test_invalid_parameters(void)
{
    jy61_t dev;
    jy61_config_t config;
    jy61_data_t data;

    CHECK(jy61_init(NULL, NULL) == JY61_ERR_PARAM);
    CHECK(jy61_reset(NULL) == JY61_ERR_PARAM);
    CHECK(jy61_feed(NULL, NULL, 0) == 0u);
    CHECK(jy61_feed_byte(NULL, 0u) == 0u);
    CHECK(jy61_get_data(NULL, &data) == JY61_ERR_PARAM);
    CHECK(jy61_get_data(&dev, NULL) == JY61_ERR_PARAM);

    jy61_config_default(&config);
    config.angle_deg_per_lsb = 0.0f;
    CHECK(jy61_init(&dev, &config) == JY61_ERR_PARAM);

    /* enter/exit 必须成对提供。 */
    jy61_config_default(&config);
    config.critical.enter = lock_enter;
    CHECK(jy61_init(&dev, &config) == JY61_ERR_PARAM);

    jy61_config_default(&config);
    config.critical.exit = lock_exit;
    CHECK(jy61_init(&dev, &config) == JY61_ERR_PARAM);
}

int main(void)
{
    test_default_config();
    test_decode_all_packet_types();
    test_noise_resync();
    test_checksum_error_and_recovery();
    test_checksum_disabled();
    test_callback();
    test_critical_section();
    test_batch_feed_and_reset();
    test_invalid_parameters();

    if (failures != 0) {
        printf("jy61: %d check(s) failed\n", failures);
        return 1;
    }

    printf("jy61: all tests passed\n");
    return 0;
}
