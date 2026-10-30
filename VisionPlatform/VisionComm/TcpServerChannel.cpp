/**
 * @file TcpServerChannel.cpp
 * @brief TcpServerChannel 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TcpServerChannel.h"
#include "CommNet.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Vision {

/************************************************************
 * 构造与生命周期
 ************************************************************/

TcpServerChannel::TcpServerChannel()
    : m_listen(INVALID_SOCKET)
    , m_open(false)
    , m_run(false)
    , m_nextId(1)
    , m_maxClients(FD_SETSIZE - 1)
    , m_rxLimit(8 * 1024 * 1024)
{
}

TcpServerChannel::~TcpServerChannel()
{
    close();
}

CommTransport TcpServerChannel::transport() const
{
    return TransportTcpServer;
}

/************************************************************
 * 监听
 ************************************************************/

bool TcpServerChannel::open(const ParamMap& params, std::string* err)
{
    close();
    commNetEnsure();
    const int port = params.value("port").toInt(9000);
    if (port <= 0 || port > 65535) {
        setCommErr(err, "端口无效");
        return false;
    }
    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) {
        setCommErr(err, "创建套接字失败");
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
        setCommErr(err, "端口绑定失败");
        close();
        return false;
    }
    const int requested = params.value("maxClients").toInt(FD_SETSIZE - 1);
    m_maxClients = requested < 1 ? 1 : requested;
    const int rxLimit = params.value("rxLimit").toInt(8 * 1024 * 1024);
    m_rxLimit = rxLimit < 64 * 1024 ? 64 * 1024 : rxLimit;
    if (listen(m_listen, m_maxClients) != 0) {
        setCommErr(err, "监听失败");
        close();
        return false;
    }
    m_open = true;
    m_run = true;
    m_thread = std::thread(&TcpServerChannel::threadMain, this);
    return true;
}

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

bool TcpServerChannel::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_open;
}

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

/************************************************************
 * 接入与收包线程
 ************************************************************/

void TcpServerChannel::threadMain()
{
    while (true) {
        // 锁内取监听口和客户端快照，锁外 poll，最长等 100ms
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
        std::vector<WSAPOLLFD> fds;
        WSAPOLLFD listenFd;
        listenFd.fd = listenSock;
        listenFd.events = POLLIN;
        listenFd.revents = 0;
        fds.push_back(listenFd);
        for (size_t i = 0; i < socks.size(); ++i) {
            WSAPOLLFD one;
            one.fd = socks[i].second;
            one.events = POLLIN;
            one.revents = 0;
            fds.push_back(one);
        }
        const int rc = WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), 100);
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
        // 新连接：超上限直接关掉，否则设非阻塞并分配递增 id
        if (!fds.empty() && (fds[0].revents & POLLIN)) {
            sockaddr_in remote;
            int len = sizeof(remote);
            SOCKET client = accept(listenSock, reinterpret_cast<sockaddr*>(&remote), &len);
            if (client != INVALID_SOCKET) {
                if (static_cast<int>(m_clients.size()) >= m_maxClients) {
                    closesocket(client);
                } else {
                    commTcpSetNoDelay(client);
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
        // 收数据进各自缓冲；对端关闭或缓冲超限即踢掉
        for (size_t i = 0; i < socks.size(); ++i) {
            if (i + 1 >= fds.size() || (fds[i + 1].revents & POLLIN) == 0) {
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
            if (static_cast<int>(it->second.rx.size()) > m_rxLimit) {
                dropClient(it->first);
            }
            m_cv.notify_all();
        }
    }
}

/************************************************************
 * 收发
 ************************************************************/

bool TcpServerChannel::write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err)
{
    SOCKET sock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        if (!m_open) {
            setCommErr(err, "通道未打开");
            return false;
        }
        std::map<std::string, Client>::iterator it = m_clients.find(peerId);
        if (it == m_clients.end()) {
            setCommErr(err, "未选择客户端");
            return false;
        }
        sock = it->second.sock;
    }
    if (data.empty()) {
        return true;
    }
    return commSendAll(sock, data, timeoutMs <= 0 ? 1000 : timeoutMs, err);
}

CommBytes TcpServerChannel::read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err)
{
    CommBytes out;
    if (maxBytes <= 0) {
        maxBytes = 4096;
    }
    std::unique_lock<std::mutex> lock(m_mu);
    if (!m_open) {
        setCommErr(err, "通道未打开");
        return out;
    }
    std::map<std::string, Client>::iterator it = m_clients.find(peerId);
    if (it == m_clients.end()) {
        setCommErr(err, "未选择客户端");
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
        setCommErr(err, "通道未打开");
        return out;
    }
    if (it == m_clients.end()) {
        setCommErr(err, "客户端已断开");
        return out;
    }
    if (it->second.rx.empty()) {
        if (timeoutMs > 0) {
            setCommErr(err, "读超时");
        }
        return out;
    }
    const int n = static_cast<int>(it->second.rx.size());
    const int take = n < maxBytes ? n : maxBytes;
    out.assign(it->second.rx.begin(), it->second.rx.begin() + take);
    it->second.rx.erase(it->second.rx.begin(), it->second.rx.begin() + take);
    return out;
}

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

void TcpServerChannel::closePeer(const std::string& peerId)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (peerId.empty()) {
        std::vector<std::string> ids;
        for (std::map<std::string, Client>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
            ids.push_back(it->first);
        }
        for (size_t i = 0; i < ids.size(); ++i) {
            dropClient(ids[i]);
        }
    } else {
        dropClient(peerId);
    }
    m_cv.notify_all();
}

}
