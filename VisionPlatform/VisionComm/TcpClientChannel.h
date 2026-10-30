/**
 * @file TcpClientChannel.h
 * @brief TCP 客户端通道
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TCP_CLIENT_CHANNEL_H
#define VISION_TCP_CLIENT_CHANNEL_H
#include "VisionCommExport.h"

#include "CommWin.h"
#include "ICommChannel.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace Vision {

/**
 * @class TcpClientChannel
 * @brief TCP 客户端。本机去连对方
 */
class VISION_COMM_API TcpClientChannel : public ICommChannel {
public:
    /** @brief 构造未连接的客户端 */
    TcpClientChannel();
    /** @brief 析构时断开 */
    ~TcpClientChannel();

    /** @brief 传输种类为 TCP 客户端 */
    CommTransport transport() const;
    /** @brief 连到参数里的主机和端口 */
    bool open(const ParamMap& params, std::string* err);
    /** @brief 断开连接 */
    void close();
    /** @brief 套接字是否有效 */
    bool isOpen() const;
    /** @brief 已连接时返回对端地址 */
    std::vector<CommPeer> peers() const;
    /** @brief 把整段字节发给已连接对端 */
    bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err);
    /** @brief 读一批；对端关闭则断开 */
    CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);
    /** @brief 丢掉当前可读数据 */
    void drain(const std::string& peerId);
    /** @brief 客户端没有多连接，始终返回空 */
    std::string waitIncoming(int timeoutMs);
    /** @brief 断开当前套接字。自动重连开着时，重连线程会再连 */
    void closePeer(const std::string& peerId);

private:
    /** @brief 按已记下的主机和端口连一次。自动重连线程调用 */
    bool connectOnce(std::string* err);
    /** @brief 非阻塞连接，在超时内等到成功或失败 */
    bool connectWithTimeout(SOCKET sock, const sockaddr_in& addr, int timeoutMs, std::string* err);
    /** @brief 自动重连循环。默认不启动，节点客户端才开 */
    void reconnectMain();

    mutable std::mutex m_mu;       ///< 保护套接字，重连和收发不要同时关
    SOCKET m_sock;                 ///< 已连接套接字
    std::string m_remote;          ///< 对端地址文本
    std::string m_host;            ///< 上次 open 的主机
    int m_port;                    ///< 上次 open 的端口
    int m_timeoutMs;               ///< 连接超时
    bool m_autoReconnect;          ///< 断开后按退避重连。PLC 默认关
    std::atomic<bool> m_stopReconnect; ///< 停止重连线程
    std::thread m_reconnectThread; ///< 重连线程
};

}

#endif
