/**
 * @file StationLoop.h
 * @brief StationLoop头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATIONLOOP_H
#define VISION_STATIONLOOP_H
#include "VisionUiExport.h"

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
 * @brief 运行态工站专属循环线程
 * @note 连续节拍（TriggerLoop / 文件夹采集）：上一周期结束后立即下一轮
 * @note 等触发（软件/硬件/上游）：阻塞等待 pulse()，再跑一轮
 */
class VISION_UI_API StationLoop {
public:
    /** @brief 周期完成回调（在工站循环线程调用） */
    typedef void (*CycleFn)(void* user, const StationRunRst& result);

    /** @brief 空闲循环，尚未绑定工站 */
    StationLoop();
    /** @brief 停止并回收循环线程 */
    ~StationLoop();

    /**
     * @brief 启动循环线程
     * @param station 工站实例（不拥有）
     * @param sol 方案（不拥有；跑期间勿改结构）
     * @param pool 并行 Job 用线程池；串行可空
     * @param onCycle 周期回调
     * @param user 回调用户指针
     */
    void start(Station* station, Solution* sol, ThreadPool* pool,
               CycleFn onCycle, void* user);
    /** @brief 仅置停止标志并唤醒（不 join） */
    void requestStop();
    /** @brief 请求停止并 join */
    void stop();
    /** @brief 软件/外部触发：唤醒一次等待中的周期（连续模式下无意义） */
    void pulse();
    /** @brief 是否循环中 */
    bool isRunning() const;
    /** @brief 当前是否连续节拍（供 UI 提示） */
    bool isContinuous() const;

private:
    /** @brief 禁止拷贝 */
    StationLoop(const StationLoop&);
    /** @brief 禁止赋值 */
    StationLoop& operator=(const StationLoop&);
    /** @brief 循环线程入口：连续节拍或等待触发 */
    void threadMain();
    /** @brief 根据工站 Job 触发方式与文件夹采集判定是否连续跑 */
    bool resolveContinuous() const;
    /** @brief 可打断短睡，便于 stop */
    void sleepInterruptible(int ms);

    Station* m_station;          ///< 工站
    Solution* m_sol;             ///< 方案
    ThreadPool* m_pool;          ///< 并行 Job 池
    CycleFn m_onCycle;           ///< 回调
    void* m_user;                ///< 回调用户数据
    std::atomic<bool> m_running; ///< 循环开关
    std::atomic<bool> m_stop;    ///< 停止请求
    std::atomic<bool> m_continuous; ///< 连续节拍
    std::atomic<bool> m_pulse;   ///< 待消费的一次触发
    std::mutex m_mu;             ///< 与条件变量配套
    std::condition_variable m_cv; ///< 等触发 / 停止唤醒
    std::thread m_thread;        ///< 专属线程
};

}

#endif