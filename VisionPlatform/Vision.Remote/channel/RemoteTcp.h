/**
 * @file RemoteTcp.h
 * @brief 远程控制专用 TCP。方法与 ICommChannel 同形，但不链接 VisionComm
 *
 * open 直接收 host 和 port，键名与 TcpChannel 的 host、port 相同。
 * 不调用 ParamMap，避免链接 VisionCommon.dll。
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_TCP_H
#define VISION_REMOTE_TCP_H

#include "CommPeer.h"
#include <stdint.h>
#include <string>

namespace Vision {

class RemoteIdentity;
class TlsPipe;

/**
 * @class RemoteTcpClient
 * @brief 被控端连出。同一时刻一条连接，peer id 为空
 */
class RemoteTcpClient {
public:
    /** @brief 未连接 */
    RemoteTcpClient();
    /** @brief 关闭连接 */
    ~RemoteTcpClient();

    /** @brief 与 CommTransport::TransportTcpClient 相同 */
    int transport() const;

    /************************************************** 连接 **************************************************/
    /**
     * @brief 先关旧连接，再非阻塞连 host:port，连上后切回阻塞并开 TCP_NODELAY
     * @param host IPv4 地址或主机名
     * @param port 1 ~ 65535
     * @param timeoutMs 连接超时，<=0 按 3000
     * @return 连上返回 true；失败写 err
     */
    bool open(const std::string& host, int port, int timeoutMs, std::string* err);
    /**
     * @brief 接管一条已经 accept 或中继对接好的套接字，不再 connect
     * @note 调用后套接字归本对象，失败时本对象会关掉它
     */
    bool attach(uintptr_t sock, const std::string& remote, std::string* err);
    /**
     * @brief TCP 连上后升级为 TLS 客户端。identity 须已 ensure
     * @param peerFingerprintOut 对端证书指纹，可空
     */
    bool startTls(const RemoteIdentity& identity, std::string* peerFingerprintOut, std::string* err);
    /** @brief 是否已完成 TLS 握手 */
    bool tlsReady() const;
    /** @brief 对端证书指纹；未 TLS 为空 */
    std::string peerFingerprint() const;
    /** @brief 关闭套接字，可在其他线程调用以打断阻塞中的 read */
    void close();
    /** @brief 套接字是否还开着。对端断开要等下一次 read / write 出错后才变 false */
    bool isOpen() const;
    /** @brief 已连接时返回一个 id 为空、remote 为“host:port”的对端，否则为空 */
    std::vector<CommPeer> peers() const;

    /************************************************** 收发 **************************************************/
    /**
     * @brief 阻塞发完整段数据。peerId 忽略
     * @param timeoutMs 发送超时，<=0 按 3000
     * @return 失败时关闭连接并返回 false
     */
    bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err);
    /**
     * @brief 收一次，最多 maxBytes 字节。peerId 忽略
     * @param timeoutMs 接收超时，<=0 按 200
     * @return 超时返回空且连接保持；对端关闭或出错时关闭连接、写 err 并返回空
     */
    CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);
    /** @brief 客户端没有接收缓存，什么都不做 */
    void drain(const std::string& peerId);
    /** @brief 等同 close()，peerId 忽略 */
    void closePeer(const std::string& peerId);

private:
    struct Impl;        ///< 套接字与对端信息，隔离 winsock 头文件
    Impl* m_impl;       ///< 实现体，构造时创建、析构时释放，非空

    /** @brief err 非空时写入 msg */
    void fail(std::string* err, const std::string& msg) const;
    /** @brief 确保本进程已 WSAStartup（只做一次） */
    bool ensureWsa(std::string* err);
};

/**
 * @class RemoteTcpServer
 * @brief 控制端监听。多台被控端同时连着
 * @note 最多 16 条连接，超出的新连接直接关掉；单条接收缓存超过 8 MB + 64 KB 时断开该连接
 */
class RemoteTcpServer {
public:
    /** @brief 未监听 */
    RemoteTcpServer();
    /** @brief 停止监听并断开全部被控端 */
    ~RemoteTcpServer();

    /** @brief 与 CommTransport::TransportTcpServer 相同 */
    int transport() const;

    /************************************************** 监听 **************************************************/
    /**
     * @brief 先关旧监听，再绑定 host:port 并起后台收包线程
     * @param host 监听地址，空或“0.0.0.0”为全部网卡
     * @param port 1 ~ 65535
     * @return 成功返回 true；失败写 err
     */
    bool open(const std::string& host, int port, std::string* err);
    /**
     * @brief 开启服务端 TLS。新连接 accept 后立刻握手；identity 生命周期须覆盖监听期
     */
    void setTlsIdentity(const RemoteIdentity* identity);
    /** @brief 某条连接的对端证书指纹；未 TLS 或找不到为空 */
    std::string peerFingerprint(const std::string& peerId) const;
    /** @brief 停线程、关监听、断开全部连接，等待线程退出 */
    void close();
    /** @brief 是否在监听 */
    bool isOpen() const;
    /** @brief 当前连着的被控端，id 为递增编号字符串，remote 为“ip:port” */
    std::vector<CommPeer> peers() const;

    /************************************************** 收发 **************************************************/
    /**
     * @brief 阻塞发给指定连接
     * @param timeoutMs 发送超时，<=0 按 3000
     * @return 连接不在或发送失败返回 false；发送失败时断开该连接
     */
    bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err);
    /**
     * @brief 从该连接的接收缓存取最多 maxBytes 字节，缓存为空时等待
     * @param timeoutMs 等待上限，<=0 按 200
     * @return 超时、连接不在或服务停止时返回空；err 不写
     */
    CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);
    /** @brief 丢掉该连接已收未取的字节 */
    void drain(const std::string& peerId);
    /** @brief 关闭并移除该连接，唤醒等待中的 read */
    void closePeer(const std::string& peerId);
    /**
     * @brief 把已经连上的套接字收进监听表，并按服务端做 TLS
     * @note 直连和中继：控制端主动连上之后，这条连接仍走现有收发
     */
    bool adopt(uintptr_t sock, const std::string& remote, std::string* err);

private:
    struct Impl;        ///< 监听套接字、连接表、收包线程，隔离 winsock 头文件
    Impl* m_impl;       ///< 实现体，构造时创建、析构时释放，非空

    /** @brief err 非空时写入 msg */
    void fail(std::string* err, const std::string& msg) const;
    /** @brief 确保本进程已 WSAStartup（只做一次） */
    bool ensureWsa(std::string* err);
    /** @brief 收包线程：轮询监听口与各连接，接新连接、收数据进各自缓存，断开的移除 */
    void threadMain();
    /** @brief 对新套接字做服务端 TLS（若已配置）并放进连接表 */
    bool finishAccept(uintptr_t sock, const std::string& remote, std::string* err);
    /** @brief 关闭并清空全部连接。调用方须持有 m_impl->mu */
    void dropClient();
};

}

#endif
