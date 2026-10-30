/**
 * @file CommNet.cpp
 * @brief CommNet实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CommNet.h"
#include "TextEncoding.h"
#include <cstdio>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace Vision {

namespace {

bool wsaStarted = false;

} // namespace

// 把错误文本写入 err；空指针则忽略
void setCommErr(std::string* err, const std::string& msg)
{
    if (err) {
        *err = msg;
    }
}

// 进程内初始化 Winsock，已初始化则直接返回
void commNetEnsure()
{
    if (wsaStarted) {
        return;
    }
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) == 0) {
        wsaStarted = true;
    }
}

// 把主机名或点分地址解析成 IPv4
bool commResolveIpv4(const std::string& host, unsigned long* addr, std::string* err)
{
    if (!addr || host.empty()) {
        setCommErr(err, "未填写主机");
        return false;
    }
    commNetEnsure();
    in_addr parsed;
    if (inet_pton(AF_INET, host.c_str(), &parsed) == 1) {
        *addr = parsed.s_addr;
        return true;
    }
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), 0, &hints, &res) != 0 || !res) {
        setCommErr(err, "主机名无法解析");
        return false;
    }
    *addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    return true;
}

// 等到套接字可读或可写，超时返回 0
int commWaitSocket(SOCKET sock, bool wantRead, int timeoutMs)
{
    if (timeoutMs < 0) {
        timeoutMs = 0;
    }
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    fd_set* rd = wantRead ? &fds : nullptr;
    fd_set* wr = wantRead ? 0 : &fds;
    const int rc = select(0, rd, wr, 0, &tv);
    if (rc == 0) {
        return 0;
    }
    if (rc < 0) {
        return -1;
    }
    return 1;
}

// 在超时内把整段字节发出去
bool commSendAll(SOCKET sock, const CommBytes& data, int timeoutMs, std::string* err)
{
    int off = 0;
    const int n = static_cast<int>(data.size());
    while (off < n) {
        const int w = commWaitSocket(sock, false, timeoutMs);
        if (w == 0) {
            setCommErr(err, "写超时");
            return false;
        }
        if (w < 0) {
            setCommErr(err, "写失败");
            return false;
        }
        const int k = send(sock, reinterpret_cast<const char*>(&data[0]) + off, n - off, 0);
        if (k <= 0) {
            setCommErr(err, "写失败");
            return false;
        }
        off += k;
    }
    return true;
}

// 读一批字节；超时、失败、对端关闭分别返回
int commRecvSome(SOCKET sock, int maxBytes, int timeoutMs, CommBytes* out, std::string* err)
{
    if (!out) {
        setCommErr(err, "参数无效");
        return -1;
    }
    out->clear();
    if (maxBytes <= 0) {
        maxBytes = 4096;
    }
    const int w = commWaitSocket(sock, true, timeoutMs);
    if (w == 0) {
        return 0;
    }
    if (w < 0) {
        setCommErr(err, "读失败");
        return -1;
    }
    out->resize(static_cast<size_t>(maxBytes));
    const int k = recv(sock, reinterpret_cast<char*>(&(*out)[0]), maxBytes, 0);
    if (k == 0) {
        out->clear();
        return -2;
    }
    if (k < 0) {
        out->clear();
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return 0;
        }
        setCommErr(err, "读失败");
        return -1;
    }
    out->resize(static_cast<size_t>(k));
    return 1;
}

// 把地址格式化成 IP:端口
std::string commEndpointText(const sockaddr_in& addr)
{
    char ip[64];
    ip[0] = 0;
    inet_ntop(AF_INET, const_cast<in_addr*>(&addr.sin_addr), ip, sizeof(ip));
    char buf[80];
    sprintf_s(buf, "%s:%u", ip, static_cast<unsigned>(ntohs(addr.sin_port)));
    return std::string(buf);
}

// 关闭 Nagle，小包尽快发出
void commTcpSetNoDelay(SOCKET sock)
{
    BOOL on = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
}

}
