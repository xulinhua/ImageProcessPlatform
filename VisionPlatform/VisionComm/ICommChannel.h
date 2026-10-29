/**
 * @file ICommChannel.h
 * @brief ICommChannel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ICOMM_CHANNEL_H
#define VISION_ICOMM_CHANNEL_H
#include "VisionCommExport.h"

#include "ParamValue.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 传输种类 */
enum CommTransport {
    TransportSerial = 0,    ///< 串口
    TransportTcpClient = 1, ///< TCP 客户端
    TransportTcpServer = 2, ///< TCP 服务端
    TransportUdp = 3,       ///< UDP
    TransportHttp = 4,      ///< HTTP 客户端
    TransportWebSocket = 5  ///< WebSocket 客户端
};

/** @class CommPeer @brief 通道上的一个对端。串口 / TCP 客户端 / UDP 只有一个，id 为空 */
struct CommPeer {
    std::string id;     ///< 端点 id；单连接通道为空
    std::string remote; ///< 对端描述，如 192.168.0.2:502
};

typedef std::vector<unsigned char> CommBytes;

/**
 * @class ICommChannel
 * @brief 传输基类：只负责字节进出，不解释 Modbus
 */
class ICommChannel {
public:
    virtual ~ICommChannel() {}

    /** @brief 传输种类 */
    virtual CommTransport transport() const = 0;
    /** @brief 按参数打开；失败写入 err */
    virtual bool open(const ParamMap& params, std::string* err) = 0;
    /** @brief 关闭通道 */
    virtual void close() = 0;
    /** @brief 是否已打开 */
    virtual bool isOpen() const = 0;

    /** @brief 当前对端。服务端为已接入的客户端 */
    virtual std::vector<CommPeer> peers() const = 0;

    /**
     * @brief 写字节
     * @param peerId 服务端必填；其它通道忽略
     */
    virtual bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err) = 0;
    /**
     * @brief 读字节。缓冲里已有数据则立刻返回，否则等到超时
     */
    virtual CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err) = 0;
    /** @brief 丢掉接收缓冲，避免旧数据串进下一次请求 */
    virtual void drain(const std::string& peerId) = 0;
    /**
     * @brief 等到任一端点收到数据
     * @return 端点 id；超时或本通道不是服务端时为空
     */
    virtual std::string waitIncoming(int timeoutMs) = 0;
    /** @brief 关掉一个对端。服务端只断该客户端；客户端只断当前套接字，自动重连仍可再连 */
    virtual void closePeer(const std::string& peerId) { (void)peerId; }
};

/** @brief 写入错误文本；err 为空则忽略 */
VISION_COMM_API void setCommErr(std::string* err, const std::string& msg);

}

#endif
