/**
 * @file LinkDial.h
 * @brief 控制端按顺序连被控端：先直连，再中继
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_LINK_DIAL_H
#define VISION_CONTROL_LINK_DIAL_H

#include <string>

namespace Vision {

class RemoteTcpServer;

/**
 * @class LinkDial
 * @brief 连上后把套接字交给控制端监听表，后面的收发和反连相同
 */
class LinkDial {
public:
    /**
     * @brief 直连 2 秒，失败再按中继口令配对
     * @param deviceId 被控端设备 ID，中继登记用，空格会被去掉
     * @return 已经放进 server 时返回 true
     */
    bool reach(RemoteTcpServer* server, const std::string& address, int directPort,
        const std::string& relayHost, int relayPort, const std::string& token,
        const std::string& deviceId, std::string* err) const;

private:
    /** @brief 带超时的 TCP 连接，成功返回套接字，失败返回 0 */
    uintptr_t openTcp(const std::string& host, int port, int timeoutMs, std::string* err) const;
    /** @brief 写一行并读回一行。读到换行就停，避免吃掉后面的 TLS */
    bool exchange(uintptr_t sock, const std::string& line, std::string* reply, std::string* err) const;
};

}

#endif
