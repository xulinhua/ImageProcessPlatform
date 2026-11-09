/**
 * @file RemoteBeacon.h
 * @brief 被控端应答扫描。控制端用同一端口发出探测
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_BEACON_H
#define VISION_REMOTE_BEACON_H

#include "Messages.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class RemoteBeacon
 * @brief 局域网发现：被控端在 UDP 19101 上应答 Probe，控制端用 scan 广播探测并收集 Announce
 * @note listen / setInfo / close 由被控端用；scan 为静态函数，由控制端用
 */
class RemoteBeacon {
public:
    /** @brief 未监听 */
    RemoteBeacon();
    /** @brief 停止监听 */
    ~RemoteBeacon();

    /**
     * @brief 先关旧的，再绑定 UDP kDiscoverPort 并起应答线程
     * @return 端口被占用等失败时返回 false 并写 err
     */
    bool listen(std::string* err);
    /** @brief 更新应答内容，之后收到的 Probe 都回这份信息 */
    void setInfo(const HelloMsg& info);
    /** @brief 关套接字打断 recvfrom，等待应答线程退出 */
    void close();

    /**
     * @brief 向本机和 255.255.255.255 发 Probe，在 timeoutMs 内收集 Announce
     * @param timeoutMs 收集时长，<=0 按 800
     * @return 按地址、名称、进程、标题去重后的被控端，address 填应答来源 IP
     */
    static std::vector<HelloMsg> scan(int timeoutMs);

private:
    struct Impl;        ///< 套接字、应答线程与应答内容，隔离 winsock 头文件
    Impl* m_impl;       ///< 实现体，构造时创建、析构时释放，非空

    /** @brief 应答线程：收到 Probe 就把当前信息打成 Announce 回给发送方 */
    void threadMain();
    /** @brief 调用 WSAStartup；listen 成功后由 close 配对 WSACleanup，失败路径与 scan 在返回前配对 */
    static bool startup(std::string* err);
};

}

#endif
