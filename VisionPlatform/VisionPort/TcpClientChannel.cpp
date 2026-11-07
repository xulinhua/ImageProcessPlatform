/**
 * @file TcpClientChannel.cpp
 * @brief TcpClientChannel 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TcpClientChannel.h"
#include "CommNet.h"
#include <cstdio>
#include <cstring>

namespace Vision {

/************************************************************
 * 构造与生命周期
 ************************************************************/

TcpClientChannel::TcpClientChannel()
    : m_sock(INVALID_SOCKET)
    , m_port(0)
    , m_timeoutMs(3000)
    , m_autoReconnect(false)
    , m_stopReconnect(false)
{
}

TcpClientChannel::~TcpClientChannel()
{
    close();
}

CommTransport TcpClientChannel::transport() const
{
    return TransportTcpClient;
}

/************************************************************
 * 连接
 ************************************************************/

bool TcpClientChannel::connectWithTimeout(SOCKET sock, const sockaddr_in& addr, int timeoutMs, std::string* err)
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
        setCommErr(err, "TCP 连接失败");
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
        setCommErr(err, "TCP 连接失败");
        return false;
    }
    return true;
}

bool TcpClientChannel::connectOnce(std::string* err)
{
    unsigned long ip = 0;
    if (!commResolveIpv4(m_host, &ip, err)) {
        return false;
    }
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        setCommErr(err, "创建套接字失败");
        return false;
    }
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(static_cast<u_short>(m_port));
    if (!connectWithTimeout(sock, addr, m_timeoutMs, err)) {
        closesocket(sock);
        return false;
    }
    commTcpSetNoDelay(sock);
    char ipText[64];
    inet_ntop(AF_INET, &addr.sin_addr, ipText, sizeof(ipText));
    char buf[80];
    sprintf_s(buf, "%s:%d", ipText, m_port);
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_stopReconnect.load() && m_autoReconnect) {
        closesocket(sock);
        return false;
    }
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
    }
    m_sock = sock;
    m_remote = buf;
    return true;
}

void TcpClientChannel::reconnectMain()
{
    int backoff = 1000;
    while (!m_stopReconnect.load()) {
        bool need = false;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            need = m_sock == INVALID_SOCKET;
        }
        if (!need) {
            backoff = 1000;
            Sleep(200);
            continue;
        }
        std::string err;
        if (connectOnce(&err)) {
            backoff = 1000;
        } else {
            Sleep(static_cast<DWORD>(backoff));
            if (backoff < 10000) {
                backoff *= 2;
            }
            if (backoff > 10000) {
                backoff = 10000;
            }
        }
    }
}

bool TcpClientChannel::open(const ParamMap& params, std::string* err)
{
    close();
    commNetEnsure();
    m_host = params.value("host").toString();
    m_port = params.value("port").toInt(9000);
    m_timeoutMs = params.value("timeoutMs").toInt(3000);
    m_autoReconnect = params.value("autoReconnect").toBool(false);
    if (m_port <= 0 || m_port > 65535) {
        setCommErr(err, "端口无效");
        return false;
    }
    m_stopReconnect.store(false);
    const bool ok = connectOnce(err);
    if (m_autoReconnect) {
        m_reconnectThread = std::thread(&TcpClientChannel::reconnectMain, this);
        return true;
    }
    return ok;
}

void TcpClientChannel::close()
{
    m_stopReconnect.store(true);
    if (m_reconnectThread.joinable() && m_reconnectThread.get_id() != std::this_thread::get_id()) {
        m_reconnectThread.join();
    }
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    m_remote.clear();
    m_autoReconnect = false;
}

bool TcpClientChannel::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_sock != INVALID_SOCKET;
}

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

/************************************************************
 * 收发
 ************************************************************/

bool TcpClientChannel::write(const std::string&, const CommBytes& data, int timeoutMs, std::string* err)
{
    SOCKET sock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        sock = m_sock;
    }
    if (sock == INVALID_SOCKET) {
        setCommErr(err, "通道未打开");
        return false;
    }
    if (data.empty()) {
        return true;
    }
    if (!commSendAll(sock, data, timeoutMs <= 0 ? 1000 : timeoutMs, err)) {
        std::lock_guard<std::mutex> lock(m_mu);
        if (m_sock == sock) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_remote.clear();
        }
        return false;
    }
    return true;
}

CommBytes TcpClientChannel::read(const std::string&, int maxBytes, int timeoutMs, std::string* err)
{
    CommBytes out;
    SOCKET sock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        sock = m_sock;
    }
    if (sock == INVALID_SOCKET) {
        setCommErr(err, "通道未打开");
        return out;
    }
    const int rc = commRecvSome(sock, maxBytes, timeoutMs, &out, err);
    if (rc == -2) {
        setCommErr(err, "对端已关闭");
        std::lock_guard<std::mutex> lock(m_mu);
        if (m_sock == sock) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_remote.clear();
        }
        out.clear();
    } else if (rc == 0 && timeoutMs > 0) {
        setCommErr(err, "读超时");
    }
    return out;
}

void TcpClientChannel::drain(const std::string&)
{
    if (!isOpen()) {
        return;
    }
    CommBytes junk;
    while (commRecvSome(m_sock, 4096, 0, &junk, 0) == 1) {
    }
}

std::string TcpClientChannel::waitIncoming(int)
{
    return std::string();
}

void TcpClientChannel::closePeer(const std::string&)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    m_remote.clear();
}

}
