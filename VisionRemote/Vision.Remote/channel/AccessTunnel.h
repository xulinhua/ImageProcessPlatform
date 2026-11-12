/**
 * @file AccessTunnel.h
 * @brief 识别码通道：经 wss 连到固定域名，本机回环一条套接字给现有 TCP/TLS 用
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_ACCESS_TUNNEL_H
#define VISION_REMOTE_ACCESS_TUNNEL_H

#include <string>

namespace Vision {

class RemoteIdentity;

/**
 * @class AccessTunnel
 * @brief 把 Cloudflare 上的 WebSocket 收成一条本机套接字
 * @note 套接字上先写 REG/OPEN 文本行，读到 GO 后再跑原来的 TLS。局域网反连不走这里
 */
class AccessTunnel {
public:
    static const char* token();
    static const char* defaultHost();

    /**
     * @brief 连上 wss://host:port/vision 后，把本机一端套接字交出来
     * @param localSock 成功时为已连接的本机套接字，调用方负责关闭
     * @return 隧道没通返回 false，不改 localSock
     */
    static bool open(const std::string& host, int port, const RemoteIdentity& identity,
        uintptr_t* localSock, std::string* err);
    /** @brief 往本机套接字写一行。失败时套接字仍由调用方关 */
    static bool writeLine(uintptr_t sock, const std::string& line);
    /**
     * @brief 读到换行或超时
     * @param waitMs 整行最长等待
     */
    static bool readLine(uintptr_t sock, std::string* line, int waitMs);
    /** @brief 关掉本机套接字。0 忽略 */
    static void discard(uintptr_t sock);
};

}

#endif
