/**
 * @file StationLoop.h
 * @brief 工站循环线程。从界面挪到无 Qt 的运行库，由 Vision.Station 持有
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATIONLOOP_H
#define VISION_STATIONLOOP_H
#include "VisionRunExport.h"

#include "Station.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Vision {

struct Solution;
class ThreadPool;

/**
 * @class StationLoop
 * @brief 一个工站一条线程。连续节拍或等待 pulse
 */
class VISION_RUN_API StationLoop {
public:
    /** @brief 周期完成回调，在工站线程里调用 */
    typedef void (*CycleFn)(void* user, const StationRunRst& result);

    /************************************************** 生命周期 **************************************************/
    /** @brief 空闲，尚未绑定工站 */
    StationLoop();
    /** @brief 停止并回收线程 */
    ~StationLoop();

    /************************************************** 控制 **************************************************/
    /**
     * @brief 启动循环线程
     * @param station 工站实例，不拥有
     * @param sol 方案，跑期间不要改结构
     * @param pool 并行流程用的线程池，串行可空
     */
    void start(Station* station, Solution* sol, ThreadPool* pool, CycleFn onCycle, void* user);
    /** @brief 只置停止标志并唤醒，不 join */
    void requestStop();
    /** @brief 请求停止并 join */
    void stop();
    /** @brief 软件触发一次。连续模式下没有意义 */
    void pulse();

    /************************************************** 状态 **************************************************/
    /** @brief 是否在循环 */
    bool isRunning() const;
    /** @brief 当前是否连续节拍 */
    bool isContinuous() const;

private:
    /** @brief 禁止拷贝 */
    StationLoop(const StationLoop&);
    /** @brief 禁止赋值 */
    StationLoop& operator=(const StationLoop&);

    /************************************************** 线程内部 **************************************************/
    /** @brief 线程入口 */
    void threadMain();
    /** @brief 按触发方式和文件夹采集决定是否连续跑 */
    bool resolveContinuous() const;
    /** @brief 可被停止打断的短睡 */
    void sleepInterruptible(int ms);
    /** @brief 这个工具是不是文件夹循环采集 */
    bool toolIsFolderGrab(const ToolCfg& tool) const;
    /** @brief 流程里有没有文件夹循环采集 */
    bool jobHasFolderGrab(const JobCfg& job) const;

    Station* m_station;             ///< 工站
    Solution* m_sol;                ///< 方案
    ThreadPool* m_pool;             ///< 并行流程池
    CycleFn m_onCycle;              ///< 周期回调
    void* m_user;                   ///< 回调用户数据
    std::atomic<bool> m_running;    ///< 循环开关
    std::atomic<bool> m_stop;       ///< 停止请求
    std::atomic<bool> m_continuous; ///< 连续节拍
    std::atomic<bool> m_pulse;      ///< 待消费的一次触发
    std::mutex m_mu;                ///< 与条件变量配套
    std::condition_variable m_cv;   ///< 等触发或停止
    std::thread m_thread;           ///< 专属线程
};

}

#endif
