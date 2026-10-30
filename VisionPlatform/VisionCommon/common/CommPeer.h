/**
 * @file CommPeer.h
 * @brief 通道上的字节块和对端。头文件即可，不进 VisionCommon.dll 的导出
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMM_PEER_H
#define VISION_COMM_PEER_H

#include <string>
#include <vector>

namespace Vision {

/** @brief 通道上的一段原始字节 */
typedef std::vector<unsigned char> CommBytes;

/**
 * @class CommPeer
 * @brief 通道上的一个对端。单连接通道的 id 为空
 */
struct CommPeer {
    std::string id;     ///< 端点 id
    std::string remote; ///< 对端描述，如 192.168.0.2:502
};

}

#endif
