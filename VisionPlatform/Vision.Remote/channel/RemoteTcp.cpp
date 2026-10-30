/**
 * @file RemoteTcp.cpp
 * @brief 远程控制自己的 TCP
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteTcp.h"
#include "Frame.h"
#include "RemoteIdentity.h"
#include "TlsPipe.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace Vision {

namespace {

struct TlsReadJob {
    TlsPipe* tls;
    CommBytes* out;
    int maxBytes;
    int timeoutMs;
    std::string* err;
};

struct TlsWriteJob {
    TlsPipe* tls;
    const CommBytes* data;
    int timeoutMs;
    std::string* err;
    bool ok;
};

void tlsReadJob(void* raw)
{
    TlsReadJob* job = static_cast<TlsReadJob*>(raw);
    *job->out = job->tls->read(job->maxBytes, job->timeoutMs, job->err);
}

void tlsWriteJob(void* raw)
{
    TlsWriteJob* job = static_cast<TlsWriteJob*>(raw);
    job->ok = job->tls->write(*job->data, job->timeoutMs, job->err);
}

/** @brief 仅此函数含 __try，避免与 C++ 对象析构冲突；Schannel 异常时返回异常码 */
int sehInvoke(void (*fn)(void*), void* arg)
{
    __try {
        fn(arg);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int>(GetExceptionCode());
    }
}

/**
 * @class WsaProcess
 * @brief 进程内只做一次 WSAStartup，不配对 WSACleanup
 */
class WsaProcess {
public:
    /** @brief 首次调用时初始化 winsock；失败返回 false 并写 err */
    static bool acquire(std::string* err)
    {
        static int rc = 1;
        static std::once_flag once;
        std::call_once(once, []() {
            WSADATA data;
            rc = WSAStartup(MAKEWORD(2, 2), &data);
        });
        if (rc != 0) {
            if (err) {
                *err = "套接字初始化失败";
            }
            return false;
        }
        return true;
    }
};

/** @brief 关 Nagle，小报文（输入、心跳）立即发出 */
void setNoDelay(SOCKET sock)
{
    BOOL on = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
}

/** @brief 循环 send 直到发完；timeoutMs<=0 按 3000 设发送超时，出错返回 false */
bool sendAll(SOCKET sock, const CommBytes& data, int timeoutMs, std::string* err)
{
    DWORD tv = timeoutMs <= 0 ? 3000 : static_cast<DWORD>(timeoutMs);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    size_t sent = 0;
    while (sent < data.size()) {
        const int n = send(sock, reinterpret_cast<const char*>(&data[sent]), static_cast<int>(data.size() - sent), 0);
        if (n <= 0) {
            if (err) {
                *err = "发送失败";
            }
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

}

struct RemoteTcpClient::Impl {
    /** @brief 未连接 */
    Impl() : sock(INVALID_SOCKET), tls(nullptr) {}
    std::mutex mu;          ///< 保护 sock 与 peer
    SOCKET sock;            ///< 当前连接，未连接为 INVALID_SOCKET
    CommPeer peer;          ///< 对端信息，id 为空，remote 为“host:port”
    TlsPipe* tls;           ///< TCP 升级 TLS 后非空
};

struct RemoteTcpServer::Impl {
    /** @brief 一条被控端连接 */
    struct Client {
        /** @brief 无套接字 */
        Client() : sock(INVALID_SOCKET), tls(nullptr) {}
        SOCKET sock;        ///< 连接套接字
        CommPeer peer;      ///< id 为递增编号，remote 为“ip:port”
        CommBytes rx;       ///< 收包线程写入、read 取走的接收缓存
        TlsPipe* tls;       ///< 开启服务端 TLS 时非空
        std::string peerFp; ///< 对端证书指纹
    };

    /** @brief 未监听，编号从 1 开始 */
    Impl() : listenSock(INVALID_SOCKET), run(false), nextId(1), identity(nullptr) {}
    std::mutex mu;                          ///< 保护 listenSock、clients、nextId
    std::condition_variable cv;             ///< 有新数据、新连接或连接移除时通知 read
    SOCKET listenSock;                      ///< 监听套接字，未监听为 INVALID_SOCKET
    std::map<std::string, Client> clients;  ///< 按 peer id 索引的连接表，最多 16 条
    std::thread thread;                     ///< 收包线程，运行 threadMain
    std::atomic<bool> run;                  ///< 收包线程是否继续，close 时置 false
    int nextId;                             ///< 下一条连接的编号
    const RemoteIdentity* identity;         ///< 非空则对新连接做 TLS 握手
};

void RemoteTcpClient::fail(std::string* err, const std::string& msg) const
{
    if (err) {
        *err = msg;
    }
}

bool RemoteTcpClient::ensureWsa(std::string* err)
{
    return WsaProcess::acquire(err);
}

RemoteTcpClient::RemoteTcpClient()
    : m_impl(new Impl())
{
}

RemoteTcpClient::~RemoteTcpClient()
{
    close();
    delete m_impl;
}

int RemoteTcpClient::transport() const
{
    return 1;
}

bool RemoteTcpClient::open(const std::string& host, int port, int timeoutMs, std::string* err)
{
    close();
    if (!ensureWsa(err)) {
        return false;
    }
    if (host.empty() || port <= 0 || port > 65535) {
        fail(err, "地址或端口不对");
        return false;
    }
    // 只解析 IPv4，取第一个结果
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* result = 0;
    char portText[16];
    sprintf_s(portText, "%d", port);
    if (getaddrinfo(host.c_str(), portText, &hints, &result) != 0 || !result) {
        fail(err, "解析地址失败");
        return false;
    }
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        fail(err, "创建套接字失败");
        return false;
    }
    // 非阻塞 connect，再用 WSAPoll 等可写，实现连接超时
    u_long nb = 1;
    ioctlsocket(sock, FIONBIO, &nb);
    const int rc = connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen));
    freeaddrinfo(result);
    if (rc != 0 && WSAGetLastError() != WSAEWOULDBLOCK && WSAGetLastError() != WSAEINPROGRESS) {
        closesocket(sock);
        fail(err, "TCP 连接失败");
        return false;
    }
    if (rc != 0) {
        WSAPOLLFD fd;
        fd.fd = sock;
        fd.events = POLLOUT;
        fd.revents = 0;
        const int wait = timeoutMs <= 0 ? 3000 : timeoutMs;
        if (WSAPoll(&fd, 1, wait) <= 0) {
            closesocket(sock);
            fail(err, "TCP 连接超时");
            return false;
        }
        int soerr = 0;
        int len = sizeof(soerr);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soerr), &len);
        if (soerr != 0) {
            closesocket(sock);
            fail(err, "TCP 连接失败");
            return false;
        }
    }
    // 连上后切回阻塞，收发靠 SO_RCVTIMEO / SO_SNDTIMEO 控制超时
    nb = 0;
    ioctlsocket(sock, FIONBIO, &nb);
    setNoDelay(sock);
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->sock = sock;
    m_impl->peer.id.clear();
    m_impl->peer.remote = host + ":" + portText;
    return true;
}

bool RemoteTcpClient::attach(uintptr_t sock, const std::string& remote, std::string* err)
{
    close();
    if (!ensureWsa(err)) {
        closesocket(static_cast<SOCKET>(sock));
        return false;
    }
    if (sock == 0 || sock == static_cast<uintptr_t>(INVALID_SOCKET)) {
        fail(err, "套接字无效");
        return false;
    }
    SOCKET owned = static_cast<SOCKET>(sock);
    setNoDelay(owned);
    u_long block = 0;
    ioctlsocket(owned, FIONBIO, &block);
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->sock = owned;
    m_impl->peer.id.clear();
    m_impl->peer.remote = remote;
    return true;
}

bool RemoteTcpClient::startTls(const RemoteIdentity& identity, std::string* peerFingerprintOut, std::string* err)
{
    SOCKET sock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        sock = m_impl->sock;
        if (sock == INVALID_SOCKET) {
            fail(err, "尚未连接");
            return false;
        }
        if (m_impl->tls) {
            fail(err, "已经是 TLS");
            return false;
        }
    }
    TlsPipe* tls = new TlsPipe();
    std::string fp;
    if (!tls->handshake(static_cast<uintptr_t>(sock), false, identity, &fp, err)) {
        delete tls;
        close();
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->tls = tls;
    if (peerFingerprintOut) {
        *peerFingerprintOut = fp;
    }
    return true;
}

bool RemoteTcpClient::tlsReady() const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->tls != nullptr && m_impl->tls->ready();
}

std::string RemoteTcpClient::peerFingerprint() const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->tls ? m_impl->tls->peerFingerprint() : std::string();
}

void RemoteTcpClient::close()
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    if (m_impl->tls) {
        delete m_impl->tls;
        m_impl->tls = nullptr;
    }
    if (m_impl->sock != INVALID_SOCKET) {
        closesocket(m_impl->sock);
        m_impl->sock = INVALID_SOCKET;
    }
    m_impl->peer = CommPeer();
}

bool RemoteTcpClient::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->sock != INVALID_SOCKET;
}

std::vector<CommPeer> RemoteTcpClient::peers() const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::vector<CommPeer> out;
    if (m_impl->sock != INVALID_SOCKET) {
        out.push_back(m_impl->peer);
    }
    return out;
}

bool RemoteTcpClient::write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err)
{
    (void)peerId;
    std::lock_guard<std::mutex> lock(m_impl->mu);
    SOCKET sock = m_impl->sock;
    TlsPipe* tls = m_impl->tls;
    if (sock == INVALID_SOCKET) {
        fail(err, "尚未连接");
        return false;
    }
    bool ok = false;
    if (tls) {
        ok = tls->write(data, timeoutMs, err);
    } else {
        ok = sendAll(sock, data, timeoutMs, err);
    }
    if (!ok) {
        if (m_impl->tls) {
            delete m_impl->tls;
            m_impl->tls = nullptr;
        }
        if (m_impl->sock != INVALID_SOCKET) {
            closesocket(m_impl->sock);
            m_impl->sock = INVALID_SOCKET;
        }
        m_impl->peer = CommPeer();
        return false;
    }
    return true;
}

CommBytes RemoteTcpClient::read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err)
{
    (void)peerId;
    CommBytes out;
    std::lock_guard<std::mutex> lock(m_impl->mu);
    SOCKET sock = m_impl->sock;
    TlsPipe* tls = m_impl->tls;
    if (sock == INVALID_SOCKET || maxBytes <= 0) {
        return out;
    }
    if (tls) {
        out = tls->read(maxBytes, timeoutMs, err);
        if (err && !err->empty() && *err != "超时") {
            delete m_impl->tls;
            m_impl->tls = nullptr;
            closesocket(m_impl->sock);
            m_impl->sock = INVALID_SOCKET;
            m_impl->peer = CommPeer();
        }
        return out;
    }
    DWORD tv = timeoutMs <= 0 ? 200 : static_cast<DWORD>(timeoutMs);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    out.resize(static_cast<size_t>(maxBytes));
    const int n = recv(sock, reinterpret_cast<char*>(&out[0]), maxBytes, 0);
    if (n > 0) {
        out.resize(static_cast<size_t>(n));
        return out;
    }
    out.clear();
    if (n == 0) {
        fail(err, "连接已断开");
    } else {
        const int wsa = WSAGetLastError();
        if (wsa == WSAETIMEDOUT || wsa == WSAEWOULDBLOCK) {
            return out;
        }
        fail(err, "接收失败");
    }
    if (m_impl->tls) {
        delete m_impl->tls;
        m_impl->tls = nullptr;
    }
    closesocket(m_impl->sock);
    m_impl->sock = INVALID_SOCKET;
    m_impl->peer = CommPeer();
    return out;
}

void RemoteTcpClient::drain(const std::string& peerId)
{
    (void)peerId;
}

void RemoteTcpClient::closePeer(const std::string& peerId)
{
    (void)peerId;
    close();
}

void RemoteTcpServer::fail(std::string* err, const std::string& msg) const
{
    if (err) {
        *err = msg;
    }
}

bool RemoteTcpServer::ensureWsa(std::string* err)
{
    return WsaProcess::acquire(err);
}

RemoteTcpServer::RemoteTcpServer()
    : m_impl(new Impl())
{
}

RemoteTcpServer::~RemoteTcpServer()
{
    close();
    delete m_impl;
}

int RemoteTcpServer::transport() const
{
    return 2;
}

bool RemoteTcpServer::open(const std::string& host, int port, std::string* err)
{
    close();
    if (!ensureWsa(err)) {
        return false;
    }
    if (port <= 0 || port > 65535) {
        fail(err, "端口不对");
        return false;
    }
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        fail(err, "创建套接字失败");
        return false;
    }
    BOOL reuse = TRUE;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    if (host.empty() || host == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            closesocket(listenSock);
            fail(err, "监听地址不对");
            return false;
        }
    }
    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0
        || listen(listenSock, 16) != 0) {
        closesocket(listenSock);
        fail(err, "监听失败");
        return false;
    }
    m_impl->listenSock = listenSock;
    m_impl->run = true;
    m_impl->thread = std::thread(&RemoteTcpServer::threadMain, this);
    return true;
}

void RemoteTcpServer::setTlsIdentity(const RemoteIdentity* identity)
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->identity = identity;
}

std::string RemoteTcpServer::peerFingerprint(const std::string& peerId) const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::map<std::string, Impl::Client>::const_iterator it = m_impl->clients.find(peerId);
    if (it == m_impl->clients.end()) {
        return std::string();
    }
    return it->second.peerFp;
}

void RemoteTcpServer::dropClient()
{
    for (std::map<std::string, Impl::Client>::iterator it = m_impl->clients.begin(); it != m_impl->clients.end(); ++it) {
        if (it->second.tls) {
            delete it->second.tls;
            it->second.tls = nullptr;
        }
        if (it->second.sock != INVALID_SOCKET) {
            closesocket(it->second.sock);
        }
    }
    m_impl->clients.clear();
    m_impl->cv.notify_all();
}

bool RemoteTcpServer::finishAccept(uintptr_t sock, const std::string& remote, std::string* err)
{
    SOCKET neu = static_cast<SOCKET>(sock);
    if (neu == INVALID_SOCKET) {
        fail(err, "套接字无效");
        return false;
    }
    setNoDelay(neu);
    const RemoteIdentity* identity = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        identity = m_impl->identity;
        if (m_impl->clients.size() >= 16) {
            closesocket(neu);
            fail(err, "连接已满");
            return false;
        }
    }
    TlsPipe* tls = nullptr;
    std::string peerFp;
    if (identity) {
        tls = new TlsPipe();
        std::string e;
        bool ok = false;
        try {
            ok = identity->ready() && identity->certificate()
                && tls->handshake(static_cast<uintptr_t>(neu), true, *identity, &peerFp, &e);
        } catch (const std::exception& ex) {
            e = ex.what();
            ok = false;
        } catch (...) {
            e = "TLS 握手异常";
            ok = false;
        }
        if (!ok) {
            delete tls;
            closesocket(neu);
            fail(err, e.empty() ? "TLS 握手失败" : e);
            return false;
        }
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    if (m_impl->clients.size() >= 16) {
        if (tls) {
            delete tls;
        }
        closesocket(neu);
        fail(err, "连接已满");
        return false;
    }
    Impl::Client client;
    client.sock = neu;
    client.peer.id = std::to_string(m_impl->nextId++);
    client.peer.remote = remote;
    client.tls = tls;
    client.peerFp = peerFp;
    m_impl->clients[client.peer.id] = client;
    m_impl->cv.notify_all();
    return true;
}

bool RemoteTcpServer::adopt(uintptr_t sock, const std::string& remote, std::string* err)
{
    if (!m_impl->run) {
        closesocket(static_cast<SOCKET>(sock));
        fail(err, "还没开始监听");
        return false;
    }
    return finishAccept(sock, remote, err);
}

void RemoteTcpServer::threadMain()
{
    try {
    while (m_impl->run) {
        // 锁内取一份套接字快照，poll 和 recv 在锁外做
        SOCKET listenSock = INVALID_SOCKET;
        std::vector<std::pair<std::string, SOCKET> > sockets;
        {
            std::lock_guard<std::mutex> lock(m_impl->mu);
            listenSock = m_impl->listenSock;
            for (std::map<std::string, Impl::Client>::iterator it = m_impl->clients.begin(); it != m_impl->clients.end(); ++it) {
                sockets.push_back(std::make_pair(it->first, it->second.sock));
            }
        }
        if (listenSock == INVALID_SOCKET) {
            break;
        }
        // fds[0] 为监听口，其后最多 16 条连接；200 ms 超时以便及时看到 run 变化
        WSAPOLLFD fds[17];
        fds[0].fd = listenSock;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        const int limit = sockets.size() > 16 ? 16 : static_cast<int>(sockets.size());
        for (int i = 0; i < limit; ++i) {
            fds[i + 1].fd = sockets[static_cast<size_t>(i)].second;
            fds[i + 1].events = POLLIN;
            fds[i + 1].revents = 0;
        }
        const int rc = WSAPoll(fds, limit + 1, 200);
        if (!m_impl->run) {
            break;
        }
        if (rc <= 0) {
            continue;
        }
        // 新连接：满 16 条直接关掉；若配置了身份则先做 TLS 握手
        if (fds[0].revents & POLLIN) {
            sockaddr_in from;
            int fromLen = sizeof(from);
            SOCKET neu = accept(listenSock, reinterpret_cast<sockaddr*>(&from), &fromLen);
            if (neu != INVALID_SOCKET) {
                char ip[64];
                inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
                char text[80];
                sprintf_s(text, "%s:%d", ip, ntohs(from.sin_port));
                std::string acceptErr;
                finishAccept(static_cast<uintptr_t>(neu), text, &acceptErr);
            }
        }
        // 已有连接：明文 recv 或 TLS 解密进缓存（TLS 与 write() 必须同锁，Schannel 非线程安全）
        for (int i = 0; i < limit; ++i) {
            if ((fds[i + 1].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                continue;
            }
            const SOCKET sock = sockets[static_cast<size_t>(i)].second;
            const std::string peerId = sockets[static_cast<size_t>(i)].first;
            std::lock_guard<std::mutex> lock(m_impl->mu);
            std::map<std::string, Impl::Client>::iterator it = m_impl->clients.find(peerId);
            if (it == m_impl->clients.end() || it->second.sock != sock) {
                continue;
            }
            CommBytes chunk;
            if (it->second.tls) {
                std::string e;
                TlsReadJob job = { it->second.tls, &chunk, 8192, 50, &e };
                const int seh = sehInvoke(&tlsReadJob, &job);
                if (seh != 0) {
                    char line[96];
                    sprintf_s(line, "Vision.Remote TLS read SEH 0x%08X\n", static_cast<unsigned>(seh));
                    OutputDebugStringA(line);
                    e = "TLS 读异常";
                    chunk.clear();
                }
                if (chunk.empty() && !e.empty() && e != "超时") {
                    delete it->second.tls;
                    closesocket(it->second.sock);
                    m_impl->clients.erase(it);
                    m_impl->cv.notify_all();
                    continue;
                }
                if (chunk.empty()) {
                    continue;
                }
            } else {
                char buf[8192];
                const int n = recv(sock, buf, sizeof(buf), 0);
                if (n <= 0) {
                    closesocket(it->second.sock);
                    m_impl->clients.erase(it);
                    m_impl->cv.notify_all();
                    continue;
                }
                chunk.assign(buf, buf + n);
            }
            it->second.rx.insert(it->second.rx.end(), chunk.begin(), chunk.end());
            if (it->second.rx.size() > Frame::kMaxPayload + 65536) {
                if (it->second.tls) {
                    delete it->second.tls;
                }
                closesocket(it->second.sock);
                m_impl->clients.erase(it);
            }
            m_impl->cv.notify_all();
        }
    }
    } catch (const std::exception& ex) {
        char line[256];
        sprintf_s(line, "Vision.Remote server thread exception: %s\n", ex.what());
        OutputDebugStringA(line);
    } catch (...) {
        OutputDebugStringA("Vision.Remote server thread unknown exception\n");
    }
}

void RemoteTcpServer::close()
{
    m_impl->run = false;
    SOCKET listenSock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        listenSock = m_impl->listenSock;
        m_impl->listenSock = INVALID_SOCKET;
        dropClient();
    }
    if (listenSock != INVALID_SOCKET) {
        closesocket(listenSock);
    }
    if (m_impl->thread.joinable()) {
        m_impl->thread.join();
    }
}

bool RemoteTcpServer::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->listenSock != INVALID_SOCKET;
}

std::vector<CommPeer> RemoteTcpServer::peers() const
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::vector<CommPeer> out;
    for (std::map<std::string, Impl::Client>::const_iterator it = m_impl->clients.begin(); it != m_impl->clients.end(); ++it) {
        out.push_back(it->second.peer);
    }
    return out;
}

bool RemoteTcpServer::write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err)
{
    // 持锁完成 TLS/明文发送，避免与收包线程并发操作同一 Schannel 上下文
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::map<std::string, Impl::Client>::iterator it = m_impl->clients.find(peerId);
    if (it == m_impl->clients.end()) {
        fail(err, "对端不在");
        return false;
    }
    SOCKET sock = it->second.sock;
    TlsPipe* tls = it->second.tls;
    bool ok = false;
    if (tls) {
        TlsWriteJob job = { tls, &data, timeoutMs, err, false };
        const int seh = sehInvoke(&tlsWriteJob, &job);
        if (seh != 0) {
            char line[96];
            sprintf_s(line, "Vision.Remote TLS write SEH 0x%08X\n", static_cast<unsigned>(seh));
            OutputDebugStringA(line);
            if (err) {
                *err = "TLS 写异常";
            }
            ok = false;
        } else {
            ok = job.ok;
        }
    } else {
        ok = sendAll(sock, data, timeoutMs, err);
    }
    if (!ok) {
        if (it->second.tls) {
            delete it->second.tls;
            it->second.tls = nullptr;
        }
        if (it->second.sock != INVALID_SOCKET) {
            closesocket(it->second.sock);
        }
        m_impl->clients.erase(it);
        m_impl->cv.notify_all();
        return false;
    }
    return true;
}

CommBytes RemoteTcpServer::read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err)
{
    (void)err;
    CommBytes out;
    std::unique_lock<std::mutex> lock(m_impl->mu);
    const int wait = timeoutMs <= 0 ? 200 : timeoutMs;
    m_impl->cv.wait_for(lock, std::chrono::milliseconds(wait), [&]() {
        std::map<std::string, Impl::Client>::iterator it = m_impl->clients.find(peerId);
        return !m_impl->run || it == m_impl->clients.end() || !it->second.rx.empty();
    });
    std::map<std::string, Impl::Client>::iterator it = m_impl->clients.find(peerId);
    if (it == m_impl->clients.end() || it->second.rx.empty() || maxBytes <= 0) {
        return out;
    }
    size_t n = it->second.rx.size();
    if (n > static_cast<size_t>(maxBytes)) {
        n = static_cast<size_t>(maxBytes);
    }
    out.assign(it->second.rx.begin(), it->second.rx.begin() + static_cast<std::ptrdiff_t>(n));
    it->second.rx.erase(it->second.rx.begin(), it->second.rx.begin() + static_cast<std::ptrdiff_t>(n));
    return out;
}

void RemoteTcpServer::drain(const std::string& peerId)
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::map<std::string, Impl::Client>::iterator it = m_impl->clients.find(peerId);
    if (it != m_impl->clients.end()) {
        it->second.rx.clear();
    }
}

void RemoteTcpServer::closePeer(const std::string& peerId)
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::map<std::string, Impl::Client>::iterator it = m_impl->clients.find(peerId);
    if (it == m_impl->clients.end()) {
        return;
    }
    if (it->second.tls) {
        delete it->second.tls;
    }
    if (it->second.sock != INVALID_SOCKET) {
        closesocket(it->second.sock);
    }
    m_impl->clients.erase(it);
    m_impl->cv.notify_all();
}

}
