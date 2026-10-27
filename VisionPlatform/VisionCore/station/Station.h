/**
 * @file Station.h
 * @brief Station头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATION_H
#define VISION_STATION_H
#include "VisionCoreExport.h"

#include "Job.h"
#include "RunListener.h"
#include "StationCfg.h"
#include <mutex>
#include <string>
#include <vector>

namespace Vision {

class ThreadPool;

/**
 * @class StationRunRst
 * @brief 跑完一个工站周期的汇总（无 Qt）
 */
struct StationRunRst {
    bool ok;                                ///< 全部流程成功
    int cycleMs;                            ///< 周期（毫秒）
    int jobsRun;                            ///< 实际跑的流程数
    std::string summary;                    ///< 摘要文本
    HsCommData::HsImage image;             ///< 末帧输出图
    std::vector<HsCommData::HsObject> objects; ///< 末帧结果几何
    HsCommData::HsImage source;            ///< 末帧源图
    StationRunRst()
        : ok(false)
        , cycleMs(0)
        , jobsRun(0)
    {}
};

/**
 * @class StationRuntime
 * @brief 工站运行态（不进配方 JSON）
 */
struct StationRuntime {
    bool running;              ///< 是否处于循环运行
    bool lastOk;               ///< 上一周期 OK
    int lastCycleMs;           ///< 上一周期耗时
    std::string lastSummary;   ///< 上一周期摘要
    StationRuntime()
        : running(false)
        , lastOk(false)
        , lastCycleMs(0)
    {}
};

/**
 * @class Station
 * @brief 工站综合体：配置 + 运行态 + 管理/执行
 * @note config 不拥有；下属 Job 由本类创建与释放
 * @note 不持有/不访问上级方案综合体；tick(Solution*) 由调用方注入
 */
class VISIONCORE_API Station {
public:
    /************************************************** 配置 **************************************************/
    /**
     * @brief 绑定工站配置并创建下属流程实例
     * @param config 方案内 StationCfg，不拥有
     */
    explicit Station(StationCfg* config);
    /** @brief 释放下属流程 */
    ~Station();

    /** @brief 当前配置 */
    StationCfg* config() const;
    /** @brief 运行态 */
    StationRuntime& runtime();
    /** @brief 运行态（只读） */
    const StationRuntime& runtime() const;
    /** @brief 设置日志监听（转发给各 Job），不拥有；建议传入 RunLogHub */
    void setListener(IRunListener* listener);

    /************************************************** 流程 **************************************************/
    /** @brief 按配置重建下属 Job（增删改流程后调用） */
    void rebuildJobs();
    /**
     * @brief 重新绑定配置指针并重建 Job（stations 扩容或流程增删后调用）
     * @param config 方案内 StationCfg，不拥有
     */
    void rebindConfig(StationCfg* config);
    /** @brief 查找流程实例 */
    Job* findJob(const ObjectId& id) const;
    /** @brief 查找包含该工具的流程实例 */
    Job* findJobOfTool(const ObjectId& toolId) const;
    /** @brief 流程实例个数 */
    int jobCount() const;
    /** @brief 按下标取流程 */
    Job* jobAt(int index) const;

    /************************************************** 运行 **************************************************/
    /** @brief 标记开始循环 */
    void start();
    /** @brief 标记停止循环 */
    void stop();
    /** @brief 是否循环中 */
    bool isRunning() const;
    /** @brief 跑一轮启用中的流程（原 Engine::runStation）；串行在本线程 */
    StationRunRst tick(Solution* sol);
    /**
     * @brief 跑一轮；并行时用 pool 调度各 Job（pool 为空则退回串行语义）
     * @param sol 方案
     * @param pool C++ 线程池；ScheduleParallel 时使用
     */
    StationRunRst tick(Solution* sol, ThreadPool* pool);
    /** @brief 清空全部下属流程运行态与总线 */
    void clearAllRuntimes();

    /************************************************** 成员 **************************************************/
    ParamMap bus; ///< 工站级总线参数（预留）

private:
    /************************************************** 日志 **************************************************/
    /** @brief 写日志（Info） */
    void log(const std::string& line);
    /** @brief 写结构化日志 */
    void log(RunLogLevel level, const std::string& message);

    /************************************************** 流程 **************************************************/
    /** @brief 重建下属 Job（调用方已持 m_mutex） */
    void rebuildJobsUnlocked();

    /************************************************** 成员 **************************************************/
    mutable std::recursive_mutex m_mutex;  ///< 保护 config/jobs 与 tick（可重入，供 tick→log）
    StationCfg* m_config;                  ///< 配置，不拥有
    StationRuntime m_runtime;              ///< 工站运行态
    std::vector<Job*> m_jobs;              ///< 下属流程（拥有）
    IRunListener* m_listener;              ///< 日志（常为 RunLogHub）
};

}

#endif


