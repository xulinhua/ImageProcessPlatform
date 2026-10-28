/**
 * @file StationLoop.cpp
 * @brief 工站循环线程
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

StationLoop::StationLoop()
    : m_station(0)
    , m_sol(0)
    , m_pool(0)
    , m_onCycle(0)
    , m_user(0)
    , m_running(false)
    , m_stop(true)
    , m_continuous(true)
    , m_pulse(false)
{
}

StationLoop::~StationLoop()
{
    stop();
}

void StationLoop::start(Station* station, Solution* sol, ThreadPool* pool, CycleFn onCycle, void* user)
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

void StationLoop::requestStop()
{
    m_stop.store(true);
    m_cv.notify_all();
}

void StationLoop::stop()
{
    requestStop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_station && m_sol) {
        m_station->runUnloadOnce(m_sol);
    }
    m_running.store(false);
    m_station = 0;
    m_sol = 0;
    m_pool = 0;
    m_onCycle = 0;
    m_user = 0;
    m_pulse.store(false);
}

void StationLoop::pulse()
{
    m_pulse.store(true);
    m_cv.notify_all();
}

bool StationLoop::toolIsFolderGrab(const ToolCfg& tool) const
{
    if (tool.type != ToolType::Acquire) {
        return false;
    }
    const std::string mode = tool.params.value("sourceMode").toString();
    return mode != "file";
}

bool StationLoop::jobHasFolderGrab(const JobCfg& job) const
{
    for (size_t i = 0; i < job.tools.size(); ++i) {
        if (toolIsFolderGrab(job.tools[i])) {
            return true;
        }
    }
    return false;
}

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
        const JobCfg& job = config->jobs[i];
        if (!job.enabled) {
            continue;
        }
        anyEnabled = true;
        if (jobHasFolderGrab(job)) {
            anyFolder = true;
        }
        if (job.execMode == ExecActive) {
            anyLoop = true;
        } else {
            anyWait = true;
        }
    }
    if (!anyEnabled || anyFolder) {
        return true;
    }
    if (anyLoop) {
        return true;
    }
    return false;
}

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

void StationLoop::threadMain()
{
    m_continuous.store(resolveContinuous());
    while (!m_stop.load()) {
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
        Station* station = m_station;
        Solution* sol = m_sol;
        if (!station || !sol) {
            break;
        }
        const StationRunRst result = station->tick(sol, m_pool, !continuous);
        if (m_onCycle) {
            m_onCycle(m_user, result);
        }
        if (m_stop.load()) {
            break;
        }
        if (continuous) {
            sleepInterruptible(1);
        }
    }
    m_running.store(false);
}

bool StationLoop::isRunning() const
{
    return m_running.load();
}

bool StationLoop::isContinuous() const
{
    return m_continuous.load();
}

}
