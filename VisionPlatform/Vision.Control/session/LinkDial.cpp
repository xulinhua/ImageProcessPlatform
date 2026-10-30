/**
 * @file LinkDial.cpp
 * @brief 直连与中继拨号
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "LinkDial.h"
#include "RemoteTcp.h"

#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace Vision {

uintptr_t LinkDial::openTcp(const std::string& host, int port, int timeoutMs, std::string* err) const
{
    if (host.empty() || port < 1) {
        if (err) {
            *err = "地址为空";
        }
        return 0;
    }
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = 0;
    char portText[16];
    sprintf_s(portText, "%d", port);
    if (getaddrinfo(host.c_str(), portText, &hints, &result) != 0 || !result) {
        if (err) {
            *err = "解析地址失败";
        }
        return 0;
    }
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        return 0;
    }
    u_long nb = 1;
    ioctlsocket(sock, FIONBIO, &nb);
    connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen));
    freeaddrinfo(result);
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sock, &set);
    timeval wait;
    wait.tv_sec = timeoutMs / 1000;
    wait.tv_usec = (timeoutMs % 1000) * 1000;
    if (select(0, 0, &set, 0, &wait) <= 0) {
        closesocket(sock);
        if (err) {
            *err = "连接超时";
        }
        return 0;
    }
    int so = 0;
    int len = sizeof(so);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so), &len);
    if (so != 0) {
        closesocket(sock);
        if (err) {
            *err = "连接失败";
        }
        return 0;
    }
    u_long block = 0;
    ioctlsocket(sock, FIONBIO, &block);
    return static_cast<uintptr_t>(sock);
}

bool LinkDial::exchange(uintptr_t sock, const std::string& line, std::string* reply, std::string* err) const
{
    int sent = 0;
    while (sent < static_cast<int>(line.size())) {
        const int n = send(static_cast<SOCKET>(sock), line.data() + sent, static_cast<int>(line.size()) - sent, 0);
        if (n <= 0) {
            if (err) {
                *err = "中继发送失败";
            }
            return false;
        }
        sent += n;
    }
    std::string got;
    for (int i = 0; i < 64; ++i) {
        char ch = 0;
        const int n = recv(static_cast<SOCKET>(sock), &ch, 1, 0);
        if (n <= 0) {
            if (err) {
                *err = "中继无响应";
            }
            return false;
        }
        got.push_back(ch);
        if (ch == '\n') {
            break;
        }
    }
    if (reply) {
        *reply = got;
    }
    return got.find("GO") != std::string::npos;
}

bool LinkDial::reach(RemoteTcpServer* server, const std::string& address, int directPort,
    const std::string& relayHost, int relayPort, const std::string& token,
    const std::string& deviceId, std::string* err) const
{
    if (!server) {
        return false;
    }
    if (!address.empty() && directPort > 0) {
        const uintptr_t sock = openTcp(address, directPort, 2000, err);
        if (sock != 0) {
            if (server->adopt(sock, address, err)) {
                return true;
            }
        }
    }
    if (relayHost.empty() || deviceId.empty()) {
        return false;
    }
    const uintptr_t sock = openTcp(relayHost, relayPort > 0 ? relayPort : 19200, 3000, err);
    if (sock == 0) {
        return false;
    }
    std::string id = deviceId;
    for (size_t i = 0; i < id.size();) {
        if (id[i] == ' ') {
            id.erase(i, 1);
        } else {
            ++i;
        }
    }
    const std::string line = "OPEN " + id + " " + token + "\n";
    std::string reply;
    if (!exchange(sock, line, &reply, err)) {
        closesocket(static_cast<SOCKET>(sock));
        return false;
    }
    return server->adopt(sock, relayHost, err);
}

}
