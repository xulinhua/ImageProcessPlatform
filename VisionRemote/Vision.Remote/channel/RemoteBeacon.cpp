/**
 * @file RemoteBeacon.cpp
 * @brief UDP 扫描。控制端广播探测，被控端把本机共享情况回给询问方
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteBeacon.h"
#include "Frame.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>

namespace Vision {

struct RemoteBeacon::Impl {
    /** @brief 未监听 */
    Impl() : sock(INVALID_SOCKET), run(false), wsa(false) {}
    std::mutex mu;          ///< 保护 sock 与 info
    SOCKET sock;            ///< UDP 套接字，未监听为 INVALID_SOCKET
    std::atomic<bool> run;  ///< 应答线程是否继续，listen 置 true、close 置 false，应答线程读
    bool wsa;               ///< listen 已调过 WSAStartup 尚未配对 WSACleanup；只在 listen / close 的调用线程读写
    std::thread thread;     ///< 应答线程，运行 threadMain
    HelloMsg info;          ///< 回给扫描方的本机信息，setInfo 更新
};

bool RemoteBeacon::startup(std::string* err)
{
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        if (err) {
            *err = "网络没有起来";
        }
        return false;
    }
    return true;
}

RemoteBeacon::RemoteBeacon()
    : m_impl(new Impl())
{
}

RemoteBeacon::~RemoteBeacon()
{
    close();
    delete m_impl;
}

bool RemoteBeacon::listen(std::string* err)
{
    close();
    if (!startup(err)) {
        return false;
    }
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        if (err) {
            *err = "扫描端口没有打开";
        }
        return false;
    }
    BOOL reuse = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<u_short>(RemoteConst::kDiscoverPort));
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(sock);
        WSACleanup();
        if (err) {
            *err = "扫描端口被占用";
        }
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        m_impl->sock = sock;
    }
    m_impl->wsa = true;
    m_impl->run = true;
    m_impl->thread = std::thread(&RemoteBeacon::threadMain, this);
    return true;
}

void RemoteBeacon::setInfo(const HelloMsg& info)
{
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->info = info;
}

void RemoteBeacon::close()
{
    m_impl->run = false;
    SOCKET sock = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        sock = m_impl->sock;
        m_impl->sock = INVALID_SOCKET;
    }
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    if (m_impl->thread.joinable()) {
        m_impl->thread.join();
    }
    if (m_impl->wsa) {
        m_impl->wsa = false;
        WSACleanup();
    }
}

void RemoteBeacon::threadMain()
{
    while (m_impl->run) {
        SOCKET sock = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> lock(m_impl->mu);
            sock = m_impl->sock;
        }
        if (sock == INVALID_SOCKET) {
            break;
        }
        char buf[2048];
        sockaddr_in from;
        int fromLen = sizeof(from);
        const int n = recvfrom(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (!m_impl->run) {
            break;
        }
        if (n <= 0) {
            continue;
        }
        CommBytes chunk(buf, buf + n);
        Frame frame;
        std::vector<Frame::Packet> packets;
        std::string err;
        if (!frame.push(chunk, &packets, &err) || packets.empty() || packets[0].type != Frame::TypeProbe) {
            continue;
        }
        HelloMsg info;
        {
            std::lock_guard<std::mutex> lock(m_impl->mu);
            info = m_impl->info;
        }
        const CommBytes reply = Messages::packAnnounce(info, &err);
        if (!reply.empty()) {
            sendto(sock, reinterpret_cast<const char*>(reply.data()), static_cast<int>(reply.size()), 0,
                reinterpret_cast<sockaddr*>(&from), fromLen);
        }
    }
}

std::vector<HelloMsg> RemoteBeacon::scan(int timeoutMs)
{
    std::vector<HelloMsg> found;
    std::string err;
    if (!startup(&err)) {
        return found;
    }
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return found;
    }
    // 同时发到本机回环和全网广播，本机上的被控端也能被扫到
    BOOL broadcast = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));
    DWORD tv = timeoutMs <= 0 ? 800 : static_cast<DWORD>(timeoutMs);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    const CommBytes probe = Messages::packEmpty(Frame::TypeProbe, &err);
    const char* targets[] = { "127.0.0.1", "255.255.255.255" };
    for (int i = 0; i < 2; ++i) {
        sockaddr_in to;
        std::memset(&to, 0, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port = htons(static_cast<u_short>(RemoteConst::kDiscoverPort));
        inet_pton(AF_INET, targets[i], &to.sin_addr);
        sendto(sock, reinterpret_cast<const char*>(probe.data()), static_cast<int>(probe.size()), 0,
            reinterpret_cast<sockaddr*>(&to), sizeof(to));
    }
    // 收应答直到超时；recvfrom 超时即提前结束
    const DWORD begin = GetTickCount();
    while (GetTickCount() - begin < tv) {
        char buf[2048];
        sockaddr_in from;
        int fromLen = sizeof(from);
        const int n = recvfrom(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n <= 0) {
            break;
        }
        CommBytes chunk(buf, buf + n);
        Frame frame;
        std::vector<Frame::Packet> packets;
        if (!frame.push(chunk, &packets, &err)) {
            continue;
        }
        for (size_t i = 0; i < packets.size(); ++i) {
            if (packets[i].type != Frame::TypeAnnounce) {
                continue;
            }
            HelloMsg msg;
            if (!Messages::unpackAnnounce(packets[i].payload, &msg)) {
                continue;
            }
            // 地址取自应答来源。本机同时收 127.0.0.1 与广播应答时，按设备 ID（或指纹）合并成一条，优先留局域网地址
            char ip[64];
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
            msg.address = ip;
            const bool loopback = msg.address == "127.0.0.1" || msg.address == "::1";
            size_t hit = found.size();
            for (size_t k = 0; k < found.size(); ++k) {
                const bool sameId = !msg.deviceId.empty() && found[k].deviceId == msg.deviceId;
                const bool sameFp = !msg.fingerprint.empty() && found[k].fingerprint == msg.fingerprint;
                const bool sameAddr = found[k].address == msg.address && found[k].name == msg.name
                    && found[k].process == msg.process && found[k].title == msg.title;
                if (sameId || sameFp || sameAddr) {
                    hit = k;
                    break;
                }
            }
            if (hit < found.size()) {
                HelloMsg& old = found[hit];
                const bool oldLoop = old.address == "127.0.0.1" || old.address == "::1";
                if (oldLoop && !loopback) {
                    old.address = msg.address;
                }
                if (old.deviceId.empty() && !msg.deviceId.empty()) {
                    old.deviceId = msg.deviceId;
                }
                if (old.fingerprint.empty() && !msg.fingerprint.empty()) {
                    old.fingerprint = msg.fingerprint;
                }
            } else {
                found.push_back(msg);
            }
        }
    }
    closesocket(sock);
    WSACleanup();
    return found;
}

}
