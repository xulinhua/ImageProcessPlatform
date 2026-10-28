/**
 * @file RunHost.h
 * @brief Station 服务里启动和停止本机工站。不打开节点 TCP
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RUN_HOST_H
#define VISION_RUN_HOST_H
#include "VisionRunExport.h"

#include "NodeSettings.h"
#include "FrameEncoder.h"
#include "RawArchive.h"
#include "SharedFrames.h"
#include "StationLoop.h"
#include <string>
#include <vector>

namespace Vision {

class ThreadPool;

/**
 * @class RunHost
 * @brief 只跑部署表里属于本节点、且已使能的工站。单机跑全部已使能工站
 */
class VISION_RUN_API RunHost {
public:
    /** @brief 尚未启动 */
    RunHost();
    /** @brief 停掉循环并释放工站实例 */
    ~RunHost();

    /** @brief 绑定方案、节点和部署表。不拥有这些对象 */
    void bind(Solution* sol, const NodeSettings* node, const DeployTable* deploy, ThreadPool* pool);
    /** @brief 按归属启动。同一工站被写了两个主人时写到头部并返回 */
    void startOwned();
    /** @brief 只停止这些工站 */
    void stop();
    /** @brief 是否还有循环在跑 */
    bool running() const;

private:
    /** @brief 一条循环和它的产量。回调用户指针就是它 */
    struct Slot {
        RunHost* host;          ///< 回调用
        Station* station;       ///< 拥有
        StationLoop* loop;      ///< 拥有
        int index;              ///< 共享内存里的本机下标
        std::string stationId;  ///< 工站 id
        int total;              ///< 产量
        int pass;               ///< OK
        int fail;               ///< NG
        Slot()
            : host(0)
            , station(0)
            , loop(0)
            , index(-1)
            , total(0)
            , pass(0)
            , fail(0)
        {}
    };

    /** @brief 循环线程回调 */
    static void onCycleThunk(void* user, const StationRunRst& result);
    /** @brief 先存原图，再编码进本机槽 */
    void onCycle(Slot* slot, const StationRunRst& result);
    /** @brief 这个工站该不该由本机跑 */
    bool mine(const StationCfg& station) const;

    Solution* m_sol;                 ///< 方案，不拥有
    const NodeSettings* m_node;      ///< 本机节点
    const DeployTable* m_deploy;     ///< 部署表
    ThreadPool* m_pool;              ///< 并行流程池
    RawArchive m_raw;                ///< 原图
    FrameEncoder m_encoder;          ///< 显示图
    std::vector<Slot*> m_slots;      ///< 正在跑的工站
    bool m_running;                  ///< 是否已启动
};

}

#endif
