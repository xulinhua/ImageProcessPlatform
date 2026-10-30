/**
 * @file WakeOnLan.h
 * @brief 向目标 MAC 发魔术包，唤醒同一局域网里的机器
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_WAKE_ON_LAN_H
#define VISION_CONTROL_WAKE_ON_LAN_H

#include <QString>

namespace Vision {

/**
 * @class WakeOnLan
 * @brief UDP 广播 255.255.255.255:9 的魔术包
 * @note MAC 写成 00:11:22:33:44:55 或 001122334455。机器要在 BIOS 里打开网络唤醒
 */
class WakeOnLan {
public:
    /** @brief 发送成功返回 true。err 可空 */
    bool send(const QString& mac, QString* err) const;

private:
    /** @brief 解析 6 字节 MAC，失败返回 false */
    bool parse(const QString& mac, unsigned char out[6]) const;
};

}

#endif
