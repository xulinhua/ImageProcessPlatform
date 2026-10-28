/**
 * @file StationService.h
 * @brief Vision.Station 服务：等命令，只跑本机工站
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATION_SERVICE_H
#define VISION_STATION_SERVICE_H

#include "NodeSettings.h"
#include "RunHost.h"
#include <string>

namespace Vision {

class Solution;

/**
 * @class StationService
 * @brief 服务启动后不擅自跑工站，堵在命令事件上
 */
class StationService {
public:
    /** @brief 尚未加载方案 */
    StationService();
    /** @brief 停止工站 */
    ~StationService();

    /** @brief 映射共享内存、加载方案和插件，然后等命令 */
    int run();

private:
    /** @brief 读 node.json、设置里的上次方案和 deploy.json */
    bool loadContext(std::string* err);
    /** @brief 执行一条命令 */
    void handle(IpcCommand cmd);
    /** @brief 许可无效时停掉正在跑的工站，并不再启动 */
    bool licenseAllowsRun();

    NodeSettings m_node;     ///< 本机节点
    DeployTable m_deploy;    ///< 部署表
    Solution* m_sol;         ///< 已加载方案，拥有
    RunHost m_host;          ///< 本机工站
    bool m_stop;             ///< 服务停止
    int m_licenseTicks;      ///< 距上次许可复查的循环次数
};

}

#endif
