/**
 * @file TcpChannel.h
 * @brief TcpChannel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TCP_CHANNEL_H
#define VISION_TCP_CHANNEL_H
#include "VisionCommExport.h"

#include "CommWin.h"
#include "ICommChannel.h"
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace Vision {

/** @class TcpClientChannel @brief TCP 客户端。本机去连对方 */
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

private:
    SOCKET m_sock;        ///< 已连接套接字
    std::string m_remote; ///< 对端地址文本
};

/**
 * @class TcpServerChannel
 * @brief TCP 服务端。监听一个端口，同时保留多个客户端（上限 FD_SETSIZE-1）
 */
class VISION_COMM_API TcpServerChannel : public ICommChannel {
public:
    /** @brief 构造未监听的服务端 */
    TcpServerChannel();
    /** @brief 析构时停止监听 */
    ~TcpServerChannel();

    /** @brief 传输种类为 TCP 服务端 */
    CommTransport transport() const;
    /** @brief 绑定端口并开始接入 */
    bool open(const ParamMap& params, std::string* err);
    /** @brief 停止监听并断开全部客户端 */
    void close();
    /** @brief 是否仍在监听 */
    bool isOpen() const;
    /** @brief 列出已接入的客户端 */
    std::vector<CommPeer> peers() const;
    /** @brief 向指定客户端写整段字节 */
    bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err);
    /** @brief 从指定客户端缓冲取字节，没有则等到超时 */
    CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);
    /** @brief 丢掉指定客户端或全部客户端的接收缓冲 */
    void drain(const std::string& peerId);
    /** @brief 等到任一客户端有数据，返回其 id */
    std::string waitIncoming(int timeoutMs);

private:
    /** @class Client @brief 已接入的一个客户端 */
    struct Client {
        Client() : sock(INVALID_SOCKET) {}
        SOCKET sock;       ///< 套接字
        std::string remote; ///< 对端地址文本
        CommBytes rx;      ///< 尚未取走的接收缓冲
    };

    /** @brief 接入新客户端，并把收到的字节放进缓冲 */
    void threadMain();
    /** @brief 关掉并移除一个客户端 */
    void dropClient(const std::string& id);

    mutable std::mutex m_mu; ///< 保护客户端表
    std::condition_variable m_cv; ///< 有数据或连接变化时唤醒
    SOCKET m_listen; ///< 监听套接字
    bool m_open;     ///< 是否在监听
    bool m_run;      ///< 接收线程是否继续
    int m_nextId;    ///< 下一个客户端编号
    std::map<std::string, Client> m_clients; ///< 已接入客户端
    std::thread m_thread; ///< 接入与收包线程
};

}

#endif
