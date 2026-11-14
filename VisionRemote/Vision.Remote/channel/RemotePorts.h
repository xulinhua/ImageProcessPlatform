/**
 * @file RemotePorts.h
 * @brief 本进程使用的监听、发现、直连、预览和中继端口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_PORTS_H
#define VISION_REMOTE_PORTS_H

namespace Vision {

/**
 * @class RemotePorts
 * @brief 进程内的端口。原来的控制端、被控端保持 19100 这一组；远程在启动时改成自己的一组
 * @note 要在打开套接字之前调用 useDesk()
 */
class RemotePorts {
public:
    /** @brief 控制端监听，被控端反连这个端口 */
    static int listen();
    /** @brief 局域网发现 UDP */
    static int discover();
    /** @brief 被控端直连 */
    static int direct();
    /** @brief 局域网查看 */
    static int preview();
    /** @brief 中继 TCP */
    static int relay();
    /** @brief 本机中继的识别码端口 */
    static int relayWs();
    /** @brief 识别码域名。远程这套默认空，不挂原来的域名 */
    static const char* accessHost();
    /** @brief 被控端是否默认打开识别码远程 */
    static bool accessOnByDefault();
    /** @brief 换成远程这套独立端口，和原来的控制端、被控端错开 */
    static void useDesk();

private:
    static int s_listen;
    static int s_discover;
    static int s_direct;
    static int s_preview;
    static int s_relay;
    static int s_relayWs;
    static const char* s_access;
    static bool s_accessOn;
};

}

#endif
