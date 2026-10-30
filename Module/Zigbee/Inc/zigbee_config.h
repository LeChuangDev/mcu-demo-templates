/**
 * @file  zigbee_config.h
 * @brief Zigbee 串口链路编译期容量与功能开关。
 *
 * 所有宏均可通过 -D 覆盖。缓冲区大小在编译期确定，模块不进行动态内存分配。
 */
#ifndef ZIGBEE_CONFIG_H
#define ZIGBEE_CONFIG_H

/* 单帧最大负载（字节）。增大时注意 RAM 占用：
 * zb_link_t 内含 RX 载荷缓冲与 TX 组帧缓冲。 */
#ifndef ZB_MAX_PAYLOAD
#define ZB_MAX_PAYLOAD 128u
#endif

/* 帧格式上限（协议本身固定，一般不需要改）。 */
#define ZB_MAX_SOP_SIZE 2u
#define ZB_MAX_EOP_SIZE 2u
#define ZB_MAX_LENGTH_SIZE 2u
#define ZB_MAX_CHECKSUM_SIZE 2u
#define ZB_MAX_BODY_SIZE (ZB_MAX_PAYLOAD + ZB_MAX_CHECKSUM_SIZE)

/* 最坏情况：长度字段 + 负载 + 校验全部被转义（每个字节变成 2 字节）。 */
#define ZB_MAX_FRAME_SIZE                                                    \
    (ZB_MAX_SOP_SIZE + 2u * (ZB_MAX_LENGTH_SIZE + ZB_MAX_BODY_SIZE) +        \
     ZB_MAX_EOP_SIZE)

/* 功能裁剪：关闭后仍保留运行期配置字段，但对应逻辑不参与编译。 */
#ifndef ZB_ENABLE_ESCAPE
#define ZB_ENABLE_ESCAPE 1
#endif

#ifndef ZB_ENABLE_CHECKSUM
#define ZB_ENABLE_CHECKSUM 1
#endif

#ifndef ZB_ENABLE_CRC16
#define ZB_ENABLE_CRC16 1
#endif

#ifndef ZB_ENABLE_RAW_MODE
#define ZB_ENABLE_RAW_MODE 1
#endif

#endif /* ZIGBEE_CONFIG_H */
