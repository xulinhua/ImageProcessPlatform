/**
 * @file LinkService.h
 * @brief Vision.Link：按角色听端口或去连接，经 CommFrame 收发节点帧
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LINK_SERVICE_H
#define VISION_LINK_SERVICE_H

#include "CommFrame.h"
#include "NodeSettings.h"
#include "ObjectId.h"
#include <map>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class LinkService
 * @brief 不跑工站，不存原图。单机直接返回
 */
class LinkService {
public:
    /** @brief 尚未打开通道 */
    LinkService();
    /** @brief 关掉节点通道 */
    ~LinkService();

    /** @brief 映射共享内存并进入收发循环 */
    int run();

private:
    /** @brief 一个已报到的客户端 */
    struct Peer {
        std::string nodeId;                 ///< 对端节点
        std::string nodeName;               ///< 对端显示名
        std::string solutionId;             ///< 对端方案标识
        std::vector<std::string> stations;  ///< 对端认领的工站
        unsigned long long lastBeatMs;      ///< 上次心跳
        bool subscribed;                    ///< 对端要看别人的画面
        /** @brief 未心跳、未订阅 */
        Peer() : lastBeatMs(0), subscribed(false) {}
    };

    /** @brief 组 Hello 负载 */
    std::vector<unsigned char> makeHello() const;
    /** @brief 解析 Hello。拒绝原因写在 reason 里 */
    bool acceptHello(const std::vector<unsigned char>& payload, Peer* peer, std::string* reason) const;
    /** @brief 处理已经收齐的一帧 */
    void onFrame(const CommFrame::Frame& frame);
    /** @brief 本机槽有新帧就发给仍在线的对端。每个对端只留最新一张 */
    void publishLatest();
    /** @brief 超过 3 秒没有心跳的对端标离线，并关掉对应 TCP */
    void dropStale();
    /** @brief 客户端断线重连后重新发 Hello */
    void ensureHello();

    NodeSettings m_node;   ///< 本机节点
    DeployTable m_deploy;  ///< 部署表
    ObjectId m_channel;    ///< 节点通道 id，不进设备台账
    CommFrame m_frames;    ///< 粘包
    std::map<std::string, Peer> m_peers; ///< peerId → 报到信息。客户端只有一条，键为空
    bool m_server;         ///< 本机是服务端
    bool m_helloSent;      ///< 客户端本轮连接是否已发 Hello
};

}

#endif
