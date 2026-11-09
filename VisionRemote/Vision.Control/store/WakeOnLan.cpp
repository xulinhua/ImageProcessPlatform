/**
 * @file WakeOnLan.cpp
 * @brief 魔术包
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "WakeOnLan.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace Vision {

bool WakeOnLan::parse(const QString& mac, unsigned char out[6]) const
{
    QString text = mac.trimmed();
    text.replace('-', ':');
    text.remove(':');
    text.remove(' ');
    if (text.size() != 12) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        const int value = text.mid(i * 2, 2).toInt(&ok, 16);
        if (!ok || value < 0 || value > 255) {
            return false;
        }
        out[i] = static_cast<unsigned char>(value);
    }
    return true;
}

bool WakeOnLan::send(const QString& mac, QString* err) const
{
    unsigned char bytes[6];
    if (!parse(mac, bytes)) {
        if (err) {
            *err = QString::fromUtf8("MAC 格式应为 00:11:22:33:44:55");
        }
        return false;
    }
    unsigned char packet[102];
    std::memset(packet, 0xFF, 6);
    for (int i = 0; i < 16; ++i) {
        std::memcpy(packet + 6 + i * 6, bytes, 6);
    }
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        if (err) {
            *err = QString::fromUtf8("网络未就绪");
        }
        return false;
    }
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        if (err) {
            *err = QString::fromUtf8("创建 UDP 套接字失败");
        }
        return false;
    }
    BOOL broadcast = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9);
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    const int n = sendto(sock, reinterpret_cast<const char*>(packet), sizeof(packet), 0,
        reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    closesocket(sock);
    WSACleanup();
    if (n != sizeof(packet)) {
        if (err) {
            *err = QString::fromUtf8("魔术包发送失败");
        }
        return false;
    }
    return true;
}

}
