/**
 * @file WebSocketClient.h
 * @brief WebSocket/WSS 客户端。长连接，不提供服务端
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_WEB_SOCKET_CLIENT_H
#define VISION_WEB_SOCKET_CLIENT_H

#include "ICommChannel.h"
#include <mutex>
#include <string>

namespace Vision {

/**
 * @class WebSocketClient
 * @brief 用 WinHTTP 完成升级。证书走系统信任库
 */
class WebSocketClient : public ICommChannel {
public:
    /** @brief 尚未连接 */
    WebSocketClient();
    /** @brief 关闭连接 */
    ~WebSocketClient();

    /** @brief 传输种类为 WebSocket */
    CommTransport transport() const;
    /** @brief 连到 url。token 非空时带 Authorization */
    bool open(const ParamMap& params, std::string* err);
    /** @brief 发关闭帧并释放句柄 */
    void close();
    /** @brief 升级是否成功 */
    bool isOpen() const;
    /** @brief 已连接时返回一条，id 为空 */
    std::vector<CommPeer> peers() const;
    /** @brief WebSocket 不按裸字节写，用 send */
    bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err);
    /** @brief WebSocket 不按裸字节读，用 recv */
    CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);
    /** @brief 无额外缓冲 */
    void drain(const std::string& peerId);
    /** @brief 没有多连接 */
    std::string waitIncoming(int timeoutMs);

    /** @brief 发一条完整消息。text 为真用文本帧，否则用二进制帧 */
    bool send(const CommBytes& data, bool text, std::string* err);
    /** @brief 收一条完整消息。分片会拼完再返回 */
    CommBytes recv(int timeoutMs, bool* text, std::string* err);

private:
    mutable std::mutex m_mu; ///< 保护句柄
    void* m_session;          ///< HINTERNET 会话
    void* m_connect;          ///< HINTERNET 连接
    void* m_socket;           ///< 升级后的 WebSocket
    std::string m_url;        ///< 原始 url
    int m_timeoutMs;          ///< 默认收发超时
};

}

#endif
