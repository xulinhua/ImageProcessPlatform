/**
 * @file CommFrame.h
 * @brief 在字节通道上收发一整帧。节点链路用，Modbus 不用
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 帧布局：
 * magic 4 字节 "VPRT"
 * version 1 字节，第一版 = 1
 * type 1 字节
 * payloadLength 4 字节，大端
 * payload
 */

#ifndef VISION_COMM_FRAME_H
#define VISION_COMM_FRAME_H
#include "VisionCommExport.h"

#include "CommMgr.h"
#include "ObjectId.h"
#include <map>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CommFrame
 * @brief 半包留在自己的缓冲里，直到头和负载都齐
 */
class VISION_COMM_API CommFrame {
public:
    /** @brief 节点帧类型 */
    enum Type {
        TypeHello = 1,        ///< 客户端报到
        TypeHelloAck = 2,     ///< 接受或拒绝
        TypeHeartbeat = 3,    ///< 心跳
        TypeHeartbeatAck = 4, ///< 心跳应答
        TypeSubscribe = 5,    ///< 是否接收其它工站画面
        TypeSnapshot = 6,     ///< 最新一帧
        TypeBye = 7           ///< 主动关闭
    };

    /** @brief 收齐的一帧 */
    struct Frame {
        Type type;                              ///< 类型
        std::string peerId;                     ///< 服务端上的客户端 id
        std::vector<unsigned char> payload;     ///< 负载
        Frame() : type(TypeHello) {}
    };

    /** @brief 负载上限 8 MB。超过就视为这条连接不能再留 */
    static const uint32_t kMaxPayload = 8 * 1024 * 1024;

    /** @brief 空缓冲 */
    CommFrame();

    /**
     * @brief 按帧头把类型和负载交给通道的 write
     */
    bool writeFrame(const ObjectId& channelId, const std::string& peerId, Type type,
                    const std::vector<unsigned char>& payload, std::string* err);
    /**
     * @brief 反复 read，直到一帧齐
     * @return 超时且没有齐帧时 false，err 为空表示只是还没到
     */
    bool readFrame(const ObjectId& channelId, const std::string& peerId, int timeoutMs, Frame* out, std::string* err);
    /** @brief 丢掉某个对端的半包 */
    void clearPeer(const std::string& peerId);

private:
    /** @brief 从缓冲里拆出一帧。不够返回 false */
    bool takeFrame(const std::string& peerId, Frame* out, std::string* err);

    std::map<std::string, std::vector<unsigned char> > m_pending; ///< 每个对端的半包
};

}

#endif
