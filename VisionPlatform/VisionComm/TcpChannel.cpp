/**
 * @file TcpChannel.cpp
 * @brief TcpChannel实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TcpChannel.h"
#include "TextEncoding.h"
#include "CommNet.h"
#include <chrono>
#include <cstdio>
#include <cstring>

namespace Vision {

namespace {

// 关闭 Nagle，小包尽快发出
void setNoDelay(SOCKET sock)
{
    BOOL on = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
}

// 非阻塞连接，在超时内等到成功或失败
bool connectWithTimeout(SOCKET sock, const sockaddr_in& addr, int timeoutMs, std::string* err)
{
    u_long nb = 1;
    ioctlsocket(sock, FIONBIO, &nb);
    const int rc = connect(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (rc == 0) {
        nb = 0;
        ioctlsocket(sock, FIONBIO, &nb);
        return true;
    }
    if (WSAGetLastError() != WSAEWOULDBLOCK && WSAGetLastError() != WSAEINPROGRESS) {
        setCommErr(err, Vision::acpToUtf8("TCP 连接失败"));
        return false;
    }
    const int w = commWaitSocket(sock, false, timeoutMs <= 0 ? 3000 : timeoutMs);
    if (w <= 0) {
        setCommErr(err, w == 0 ? "TCP 连接超时" : "TCP 连接失败");
        return false;
    }
    int soerr = 0;
    int len = sizeof(soerr);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soerr), &len);
    nb = 0;
    ioctlsocket(sock, FIONBIO, &nb);
    if (soerr != 0) {
        setCommErr(err, Vision::acpToUtf8("TCP 连接失败"));
        return false;
    }
    return true;
}

} // namespace

// 套接字置为未连接
TcpClientChannel::TcpClientChannel()
    : m_sock(INVALID_SOCKET)
{
}

// 析构时断开
TcpClientChannel::~TcpClientChannel()
{
    close();
}

// 连到主机和端口，并记下对端文本
bool TcpClientChannel::open(const ParamMap& params, std::string* err)
{
    close();
    commNetEnsure();
    const std::string host = params.value("host").toString();
    const int port = params.value("port").toInt(9000);
    unsigned long ip = 0;
    if (!commResolveIpv4(host, &ip, err)) {
        return false;
    }
    if (port <= 0 || port > 65535) {
        setCommErr(err, Vision::acpToUtf8("端口无效"));
        return false;
    }
    m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == INVALID_SOCKET) {
        setCommErr(err, Vision::acpToUtf8("创建套接字失败"));
        return false;
    }
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(static_cast<u_short>(port));
    const int timeout = params.value("timeoutMs").toInt(3000);
    if (!connectWithTimeout(m_sock, addr, timeout, err)) {
        close();
        return false;
    }
    setNoDelay(m_sock);
    char ipText[64];
    inet_ntop(AF_INET, &addr.sin_addr, ipText, sizeof(ipText));
    char buf[80];
    sprintf_s(buf, "%s:%d", ipText, port);
    m_remote = buf;
    return true;
}

// 断开并清对端文本
void TcpClientChannel::close()
{
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    m_remote.clear();
}

// 套接字是否有效
bool TcpClientChannel::isOpen() const
{
    return m_sock != INVALID_SOCKET;
}

// 已连接时返回对端地址
std::vector<CommPeer> TcpClientChannel::peers() const
{
    std::vector<CommPeer> list;
    if (isOpen()) {
        CommPeer p;
        p.remote = m_remote;
        list.push_back(p);
    }
    return list;
}

// 把整段字节发给已连接对端
bool TcpClientChannel::write(const std::string&, const CommBytes& data, int timeoutMs, std::string* err)
{
    if (!isOpen()) {
        setCommErr(err, Vision::acpToUtf8("通道未打开"));
        return false;
    }
    if (data.empty()) {
        return true;
    }
    return commSendAll(m_sock, data, timeoutMs <= 0 ? 1000 : timeoutMs, err);
}

// 读一批；对端关闭则断开
CommBytes TcpClientChannel::read(const std::string&, int maxBytes, int timeoutMs, std::string* err)
{
    CommBytes out;
    if (!isOpen()) {
        setCommErr(err, Vision::acpToUtf8("通道未打开"));
        return out;
    }
    const int rc = commRecvSome(m_sock, maxBytes, timeoutMs, &out, err);
    if (rc == -2) {
        setCommErr(err, Vision::acpToUtf8("对端已关闭"));
        close();
        out.clear();
    } else if (rc == 0 && timeoutMs > 0) {
        setCommErr(err, Vision::acpToUtf8("读超时"));
    }
    return out;
}

// 把当前可读数据丢掉
void TcpClientChannel::drain(const std::string&)
{
    if (!isOpen()) {
        return;
    }
    CommBytes junk;
    while (commRecvSome(m_sock, 4096, 0, &junk, 0) == 1) {
    }
}

// 客户端没有多连接，不等待接入
std::string TcpClientChannel::waitIncoming(int)
{
    return std::string();
}

// 监听套接字置为未打开
TcpServerChannel::TcpServerChannel()
    : m_listen(INVALID_SOCKET)
    , m_open(false)
    , m_run(false)
    , m_nextId(1)
{
}

// 析构时停止监听
TcpServerChannel::~TcpServerChannel()
{
    close();
}

// 绑定端口并拉起接入线程
bool TcpServerChannel::open(const ParamMap& params, std::string* err)
{
    close();
    commNetEnsure();
    const int port = params.value("port").toInt(9000);
    if (port <= 0 || port > 65535) {
        setCommErr(err, Vision::acpToUtf8("端口无效"));
        return false;
    }
    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) {
        setCommErr(err, Vision::acpToUtf8("创建套接字失败"));
        return false;
    }
    BOOL reuse = TRUE;
    setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<u_short>(port));
    if (bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        setCommErr(err, Vision::acpToUtf8("端口绑定失败"));
        close();
        return false;
    }
    if (listen(m_listen, 8) != 0) {
        setCommErr(err, Vision::acpToUtf8("监听失败"));
        close();
        return false;
    }
    m_open = true;
    m_run = true;
    m_thread = std::thread(&TcpServerChannel::threadMain, this);
    return true;
}

// 停线程、关监听、断开全部客户端
void TcpServerChannel::close()
{
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_run = false;
        m_open = false;
    }
    m_cv.notify_all();
    if (m_listen != INVALID_SOCKET) {
        closesocket(m_listen);
        m_listen = INVALID_SOCKET;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    std::lock_guard<std::mutex> lock(m_mu);
    for (std::map<std::string, Client>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it->second.sock != INVALID_SOCKET) {
            closesocket(it->second.sock);
        }
    }
    m_clients.clear();
}

// 是否仍在监听
bool TcpServerChannel::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_open;
}

// 列出已接入的客户端
std::vector<CommPeer> TcpServerChannel::peers() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    std::vector<CommPeer> list;
    for (std::map<std::string, Client>::const_iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
        CommPeer p;
        p.id = it->first;
        p.remote = it->second.remote;
        list.push_back(p);
    }
    return list;
}

// 关掉并移除一个客户端
void TcpServerChannel::dropClient(const std::string& id)
{
    std::map<std::string, Client>::iterator it = m_clients.find(id);
    if (it == m_clients.end()) {
        return;
    }
    if (it->second.sock != INVALID_SOCKET) {
        closesocket(it->second.sock);
    }
    m_clients.erase(it);
}

// 接入新客户端，并把收到的字节放进缓冲
void TcpServerChannel::threadMain()
{
    while (true) {
        SOCKET listenSock = INVALID_SOCKET;
        std::vector<std::pair<std::string, SOCKET> > socks;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (!m_run) {
                break;
            }
            listenSock = m_listen;
            for (std::map<std::string, Client>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
                socks.push_back(std::make_pair(it->first, it->second.sock));
            }
        }
        if (listenSock == INVALID_SOCKET) {
            break;
        }
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listenSock, &rfds);
        for (size_t i = 0; i < socks.size(); ++i) {
            FD_SET(socks[i].second, &rfds);
        }
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000;
        const int rc = select(0, &rfds, 0, 0, &tv);
        if (rc < 0) {
            std::lock_guard<std::mutex> lock(m_mu);
            if (!m_run) {
                break;
            }
            continue;
        }
        std::lock_guard<std::mutex> lock(m_mu);
        if (!m_run) {
            break;
        }
        if (FD_ISSET(listenSock, &rfds)) {
            sockaddr_in remote;
            int len = sizeof(remote);
            SOCKET client = accept(listenSock, reinterpret_cast<sockaddr*>(&remote), &len);
            if (client != INVALID_SOCKET) {
                if (m_clients.size() + 1 >= FD_SETSIZE) {
                    closesocket(client);
                } else {
                    setNoDelay(client);
                    u_long nb = 1;
                    ioctlsocket(client, FIONBIO, &nb);
                    char idbuf[32];
                    sprintf_s(idbuf, "%d", m_nextId++);
                    Client c;
                    c.sock = client;
                    c.remote = commEndpointText(remote);
                    m_clients[idbuf] = c;
                    m_cv.notify_all();
                }
            }
        }
        for (size_t i = 0; i < socks.size(); ++i) {
            if (!FD_ISSET(socks[i].second, &rfds)) {
                continue;
            }
            std::map<std::string, Client>::iterator it = m_clients.find(socks[i].first);
            if (it == m_clients.end()) {
                continue;
            }
            char buf[2048];
            const int n = recv(it->second.sock, buf, sizeof(buf), 0);
            if (n <= 0) {
                dropClient(it->first);
                m_cv.notify_all();
                continue;
            }
            it->second.rx.insert(it->second.rx.end(), buf, buf + n);
            m_cv.notify_all();
        }
    }
}

// 向指定客户端写整段字节
bool TcpServerChannel::write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err)
{
    SOCKET sock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        if (!m_open) {
            setCommErr(err, Vision::acpToUtf8("通道未打开"));
            return false;
        }
        std::map<std::string, Client>::iterator it = m_clients.find(peerId);
        if (it == m_clients.end()) {
            setCommErr(err, Vision::acpToUtf8("未选择客户端"));
            return false;
        }
        sock = it->second.sock;
    }
    if (data.empty()) {
        return true;
    }
    return commSendAll(sock, data, timeoutMs <= 0 ? 1000 : timeoutMs, err);
}

// 从指定客户端的缓冲取字节，没有则等到超时
CommBytes TcpServerChannel::read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err)
{
    CommBytes out;
    if (maxBytes <= 0) {
        maxBytes = 4096;
    }
    std::unique_lock<std::mutex> lock(m_mu);
    if (!m_open) {
        setCommErr(err, Vision::acpToUtf8("通道未打开"));
        return out;
    }
    std::map<std::string, Client>::iterator it = m_clients.find(peerId);
    if (it == m_clients.end()) {
        setCommErr(err, Vision::acpToUtf8("未选择客户端"));
        return out;
    }
    if (it->second.rx.empty() && timeoutMs > 0) {
        m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&]() {
            std::map<std::string, Client>::iterator cur = m_clients.find(peerId);
            return !m_open || cur == m_clients.end() || !cur->second.rx.empty();
        });
        it = m_clients.find(peerId);
    }
    if (!m_open) {
        setCommErr(err, Vision::acpToUtf8("通道未打开"));
        return out;
    }
    if (it == m_clients.end()) {
        setCommErr(err, Vision::acpToUtf8("客户端已断开"));
        return out;
    }
    if (it->second.rx.empty()) {
        if (timeoutMs > 0) {
            setCommErr(err, Vision::acpToUtf8("读超时"));
        }
        return out;
    }
    const int n = static_cast<int>(it->second.rx.size());
    const int take = n < maxBytes ? n : maxBytes;
    out.assign(it->second.rx.begin(), it->second.rx.begin() + take);
    it->second.rx.erase(it->second.rx.begin(), it->second.rx.begin() + take);
    return out;
}

// 丢掉指定客户端或全部客户端的接收缓冲
void TcpServerChannel::drain(const std::string& peerId)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (peerId.empty()) {
        for (std::map<std::string, Client>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
            it->second.rx.clear();
        }
        return;
    }
    std::map<std::string, Client>::iterator it = m_clients.find(peerId);
    if (it != m_clients.end()) {
        it->second.rx.clear();
    }
}

// 等到任一客户端缓冲里有数据，返回其 id
std::string TcpServerChannel::waitIncoming(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mu);
    const int waitMs = timeoutMs < 0 ? 0 : timeoutMs;
    m_cv.wait_for(lock, std::chrono::milliseconds(waitMs), [&]() {
        if (!m_open) {
            return true;
        }
        for (std::map<std::string, Client>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (!it->second.rx.empty()) {
                return true;
            }
        }
        return false;
    });
    for (std::map<std::string, Client>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it->second.rx.empty()) {
            return it->first;
        }
    }
    return std::string();
}

// 传输类型
CommTransport TcpClientChannel::transport() const
{
    return TransportTcpClient;
}

// 传输类型
CommTransport TcpServerChannel::transport() const
{
    return TransportTcpServer;
}



}
