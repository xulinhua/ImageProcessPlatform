/**
 * @file StationLoop.cpp
 * @brief StationLoop实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StationLoop.h"
#include "JobEnums.h"
#include "Solution.h"
#include "ThreadPool.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

// 工具是否文件夹循环采集（非单文件）
bool toolIsFolderGrab(const ToolCfg& t)
{
    if (t.type != ToolType::Acquire) {
        return false;
    }
    const std::string mode = t.params.value("sourceMode").toString();
    return mode != "file";
}

// Job 是否含文件夹循环采集
bool jobHasFolderGrab(const JobCfg& job)
{
    for (size_t i = 0; i < job.tools.size(); ++i) {
        if (toolIsFolderGrab(job.tools[i])) {
            return true;
        }
    }
    return false;
}

} // namespace

// 默认空闲
StationLoop::StationLoop()
    : m_station(nullptr)
    , m_sol(nullptr)
    , m_pool(nullptr)
    , m_onCycle(0)
    , m_user(nullptr)
    , m_running(false)
    , m_stop(true)
    , m_continuous(true)
    , m_pulse(false)
{
}

// 确保停止
StationLoop::~StationLoop()
{
    stop();
}

// 启动专属线程
void StationLoop::start(Station* station, Solution* sol, ThreadPool* pool,
                        CycleFn onCycle, void* user)
{
    stop();
    if (!station || !sol || !onCycle) {
        return;
    }
    m_station = station;
    m_sol = sol;
    m_pool = pool;
    m_onCycle = onCycle;
    m_user = user;
    m_pulse.store(false);
    m_stop.store(false);
    m_running.store(true);
    m_continuous.store(resolveContinuous());
    m_thread = std::thread(&StationLoop::threadMain, this);
}

// 停止并等待当前周期结束
// 仅请求停止，不阻塞调用方
void StationLoop::requestStop()
{
    m_stop.store(true);
    m_cv.notify_all();
}

// 停止并等待当前周期结束
void StationLoop::stop()
{
    requestStop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_running.store(false);
    m_station = 0;
    m_sol = 0;
    m_pool = 0;
    m_onCycle = 0;
    m_user = 0;
    m_pulse.store(false);
}

// 软件/外部触发一次
void StationLoop::pulse()
{
    m_pulse.store(true);
    m_cv.notify_all();
}

// TriggerLoop 或文件夹采集 → 连续；否则等触发
bool StationLoop::resolveContinuous() const
{
    if (!m_station || !m_station->config()) {
        return true;
    }
    const StationCfg* config = m_station->config();
    bool anyEnabled = false;
    bool anyLoop = false;
    bool anyWait = false;
    bool anyFolder = false;
    for (size_t i = 0; i < config->jobs.size(); ++i) {
        const JobCfg& j = config->jobs[i];
        if (!j.enabled) {
            continue;
        }
        anyEnabled = true;
        if (jobHasFolderGrab(j)) {
            anyFolder = true;
        }
        if (j.trigger == TriggerLoop) {
            anyLoop = true;
        } else {
            anyWait = true;
        }
    }
    if (!anyEnabled) {
        return true;
    }
    // 文件夹循环采集：按产线节奏连续跑（采下一张）
    if (anyFolder) {
        return true;
    }
    // 全是循环触发，或存在循环且无等待类
    if (anyLoop && !anyWait) {
        return true;
    }
    if (anyLoop && anyWait) {
        // 混合时以连续为主（循环 Job 需要节拍），等待类由上游/手动另议
        return true;
    }
    return false;
}

// 可打断睡眠
void StationLoop::sleepInterruptible(int ms)
{
    const int slice = 20;
    int left = ms > 0 ? ms : 0;
    while (left > 0 && !m_stop.load()) {
        const int step = left > slice ? slice : left;
        Sleep(static_cast<DWORD>(step));
        left -= step;
    }
}

// 循环：连续则周期结束即下一轮；等触发则阻塞至 pulse/stop
void StationLoop::threadMain()
{
    // 启动时再判定一次（配置可能刚绑好）
    m_continuous.store(resolveContinuous());

    while (!m_stop.load()) {
        // 工站未使能：不 tick，短睡以免占满 CPU
        if (m_station && m_station->config() && !m_station->config()->enabled) {
            sleepInterruptible(50);
            continue;
        }
        const bool continuous = resolveContinuous();
        m_continuous.store(continuous);

        if (!continuous) {
            std::unique_lock<std::mutex> lock(m_mu);
            m_cv.wait(lock, [this]() {
                return m_stop.load() || m_pulse.load();
            });
            if (m_stop.load()) {
                break;
            }
            m_pulse.store(false);
        }

        Station* st = m_station;
        Solution* sol = m_sol;
        if (!st || !sol) {
            break;
        }
        const StationRunRst r = st->tick(sol, m_pool);
        if (m_onCycle) {
            m_onCycle(m_user, r);
        }

        if (m_stop.load()) {
            break;
        }
        if (continuous) {
            // 避免空转占满 CPU；正常采图+算法耗时远大于此
            sleepInterruptible(1);
        }
    }
    m_running.store(false);
}

// 是否运行中
bool StationLoop::isRunning() const
{
    return m_running.load();
}

// 是否连续运行
bool StationLoop::isContinuous() const
{
    return m_continuous.load();
}

}