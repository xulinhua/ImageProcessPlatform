/**
 * @file DirectListen.h
 * @brief 被控端直连监听。控制端连入后把套接字交给会话
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_DIRECT_LISTEN_H
#define VISION_CONTROLLED_DIRECT_LISTEN_H

#include <atomic>
#include <string>
#include <thread>

namespace Vision {

class AgentSession;

/**
 * @class DirectListen
 * @brief 在 0.0.0.0 上听一个端口。已有反连时拒绝新的直连
 */
class DirectListen {
public:
    /** @brief 未监听 */
    DirectListen();
    /** @brief 停监听并等线程退出 */
    ~DirectListen();

    /** @brief 开始监听。session 在 stop 之前必须有效 */
    bool start(AgentSession* session, int port, std::string* err);
    /** @brief 停监听并等线程退出 */
    void stop();
    /** @brief 正在监听 */
    bool running() const;

private:
    /** @brief accept 循环 */
    void serve();

    AgentSession* m_session;        ///< 接到的套接字交给它。stop 之前有效
    std::thread m_thread;           ///< accept 线程
    std::atomic<bool> m_stop;       ///< 线程是否该退出
    uintptr_t m_listen;             ///< 监听套接字，未监听为 INVALID_SOCKET
    int m_port;                     ///< 当前端口，未监听为 0
};

}

#endif
