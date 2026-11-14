/**
 * @file RelayHub.h
 * @brief 中继服务。中控台在进程内打开，单独的中继程序也用同一份
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RELAY_HUB_H
#define VISION_RELAY_HUB_H

namespace Vision {

/**
 * @class RelayHub
 * @brief 设备登记与一次会话的字节转发
 * @note 局域网口走裸 TCP。识别码口只绑本机，做 WebSocket 升级后的 REG/OPEN。口令不对就拒绝
 */
class RelayHub {
public:
    /** @brief 还没监听 */
    RelayHub();

    /** @brief 停止监听并释放 */
    ~RelayHub();

    /**
     * @brief 在全部网卡上听 port
     * @return 绑定或 listen 失败返回 false
     */
    bool start(int port);

    /**
     * @brief 再听一个本机端口，给隧道把识别码连进来
     * @return 绑定失败返回 false，局域网中继不受影响
     */
    bool startWs(int port);

    /** @brief 关掉监听口并等接受线程退出。已经在转发的会话不在这里收 */
    void stop();

    /** @brief 局域网口正在听 */
    bool listening() const;

    /** @brief 局域网端口。没在听时为 0 */
    int port() const;

    /** @brief 识别码端口。没在听时为 0 */
    int wsPort() const;

private:
    RelayHub(const RelayHub&) = delete;
    RelayHub& operator=(const RelayHub&) = delete;

    struct Impl;
    Impl* m_impl;
};

} // namespace Vision

#endif
