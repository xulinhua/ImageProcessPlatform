/**
 * @file Beacon.h
 * @brief 局域网发现应答。中控台向 UDP 19101 广播 Probe，这里回 Announce
 */

#ifndef REMOTE_CLIENT_BEACON_H
#define REMOTE_CLIENT_BEACON_H

#include "Messages.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace Vision {

class Beacon {
public:
    Beacon();
    ~Beacon();

    /** @brief 绑定 19101。端口被占用时返回 false，调用方仍可直连 */
    bool listen(std::string* err);
    /** @brief 之后的 Probe 都回这份身份 */
    void setInfo(const HelloMsg& info);
    /** @brief 最近一次 Probe 的来源地址。中控台扫到本机时，用它反连 19100 */
    std::string consoleHost();
    /** @brief 关掉套接字并等应答线程结束 */
    void close();

private:
    /** @brief 收到 Probe 就按来源地址回 Announce */
    void serve();

    std::mutex m_mu;
    HelloMsg m_info;
    std::string m_console;      ///< 最近一次探测包的来源 IP
    int m_fd;
    std::atomic<bool> m_run;
    std::thread m_thread;
};

}

#endif
