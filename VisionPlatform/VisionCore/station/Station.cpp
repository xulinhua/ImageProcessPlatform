/**
 * @file Station.cpp
 * @brief Station实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Station.h"
#include "Solution.h"
#include "ThreadPool.h"
#include "Translate.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "StageTimer.h"

namespace Vision {


// 绑定配置并创建下属流程
Station::Station(StationCfg* config)
    : m_config(config)
    , m_listener(nullptr)
{
    rebuildJobsUnlocked();
}

// 释放下属流程
Station::~Station()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (size_t i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i]) {
            delete m_jobs[i];
            m_jobs[i] = nullptr;
        }
    }
    m_jobs.clear();
    m_config = nullptr;
    m_listener = nullptr;
}

// 写日志（Info）
void Station::log(const std::string& line)
{
    log(RunLogInfo, line);
}

// 结构化工站日志
void Station::log(RunLogLevel level, const std::string& message)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_listener) {
        return;
    }
    RunLogEvent ev;
    ev.level = level;
    ev.message = message;
    if (m_config) {
        ev.stationId = m_config->id;
    }
    m_listener->onRunLog(ev);
}

// 设置监听并下发到各 Job
void Station::setListener(IRunListener* listener)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_listener = listener;
    // 清空监听时不碰 config->id，避免 stations 扩容后悬空指针触发 bad_alloc
    ObjectId sid;
    if (listener && m_config) {
        sid = m_config->id;
    }
    for (size_t i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i]) {
            m_jobs[i]->setListener(listener);
            if (listener) {
                m_jobs[i]->setStationId(sid);
            }
        }
    }
}

// 按当前配置重建 Job 列表
void Station::rebuildJobs()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    rebuildJobsUnlocked();
}

// 重建（已持锁）
void Station::rebuildJobsUnlocked()
{
    std::map<std::string, Job*> kept;
    for (size_t i = 0; i < m_jobs.size(); ++i) {
        Job* job = m_jobs[i];
        if (!job) {
            continue;
        }
        const std::string id = job->config() ? job->config()->id.toString() : std::string();
        if (id.empty() || kept.find(id) != kept.end()) {
            delete job;
            continue;
        }
        kept[id] = job;
    }
    m_jobs.clear();
    if (!m_config) {
        for (std::map<std::string, Job*>::iterator it = kept.begin(); it != kept.end(); ++it) {
            delete it->second;
        }
        return;
    }
    for (size_t i = 0; i < m_config->jobs.size(); ++i) {
        JobCfg& cfg = m_config->jobs[i];
        const std::string id = cfg.id.toString();
        Job* job = 0;
        std::map<std::string, Job*>::iterator old = kept.find(id);
        if (old != kept.end()) {
            job = old->second;
            kept.erase(old);
            job->setConfig(&cfg);
        } else {
            job = new Job(&cfg);
        }
        job->setListener(m_listener);
        job->setStationId(m_config->id);
        m_jobs.push_back(job);
    }
    for (std::map<std::string, Job*>::iterator it = kept.begin(); it != kept.end(); ++it) {
        delete it->second;
    }
}
void Station::rebindConfig(StationCfg* config)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_config = config;
    rebuildJobsUnlocked();
}

// 按 id 查找流程实例
Job* Station::findJob(const ObjectId& id) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (size_t i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i] && m_jobs[i]->config() && m_jobs[i]->config()->id == id) {
            return m_jobs[i];
        }
    }
    return 0;
}

// 查找包含该工具的流程实例
Job* Station::findJobOfTool(const ObjectId& toolId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (size_t i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i] && m_jobs[i]->config() && m_jobs[i]->config()->findTool(toolId)) {
            return m_jobs[i];
        }
    }
    return 0;
}

// 按下标取流程
Job* Station::jobAt(int index) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (index < 0 || index >= static_cast<int>(m_jobs.size())) {
        return 0;
    }
    return m_jobs[static_cast<size_t>(index)];
}

// 开始循环
void Station::start()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_runtime.running = true;
}

// 停止循环
void Station::stop()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_runtime.running = false;
}

// 清空全部流程运行态与总线
void Station::clearAllRuntimes()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (size_t i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i]) {
            m_jobs[i]->clearRuntime();
        }
    }
    bus.clear();
}

// 跑一轮启用中的流程（串行）
StationRunRst Station::tick(Solution* sol)
{
    return tick(sol, 0);
}

// 跑一轮；并行时用线程池（持锁至整周期结束，禁止中途 rebuild）
StationRunRst Station::tick(Solution* sol, ThreadPool* pool)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    StationRunRst sr;
    StageTimer watch;
    watch.begin("cycle");
    if (!m_config) {
        sr.summary = std::string(Translate::tr("没有工站"));
        return sr;
    }
    if (!m_config->enabled) {
        sr.summary = std::string(Translate::tr("工站未使能"));
        sr.ok = true;
        return sr;
    }
    bus.clear();
    sr.ok = true;

    std::vector<Job*> enabled;
    for (size_t i = 0; i < m_jobs.size(); ++i) {
        Job* job = m_jobs[i];
        if (job && job->config() && job->config()->enabled) {
            enabled.push_back(job);
        }
    }

    // 各 Job 已持独立 Tool/Alg 实例；ScheduleParallel 时线程池并行跑流程
    const bool parallel = (m_config->jobSchedule == ScheduleParallel && pool && enabled.size() > 1);
    std::vector<ToolOut> outs(enabled.size());

    if (parallel) {
        std::mutex waitMu;
        std::condition_variable waitCv;
        std::atomic<int> left(static_cast<int>(enabled.size()));
        for (size_t i = 0; i < enabled.size(); ++i) {
            Job* job = enabled[i];
            pool->enqueue([sol, job, &outs, i, &left, &waitCv]() {
                job->initAutoParas();
                outs[i] = job->run(sol);
                if (left.fetch_sub(1) == 1) {
                    waitCv.notify_one();
                }
            });
        }
        std::unique_lock<std::mutex> waitLock(waitMu);
        waitCv.wait(waitLock, [&left]() { return left.load() == 0; });
        for (size_t i = 0; i < enabled.size(); ++i) {
            const ToolOut& r = outs[i];
            Job* job = enabled[i];
            ++sr.jobsRun;
            sr.image = r.image;
            sr.objects = r.objects;
            if (sr.source.isNull() && !r.source.isNull()) {
                sr.source = r.source;
            }
            bus.insert(job->config()->id.toString() + ".ok", r.ok);
            bus.insert(job->config()->id.toString() + ".fromJobId", job->config()->id.toString());
            if (!r.ok) {
                sr.ok = false;
            }
        }
    } else {
        for (size_t i = 0; i < enabled.size(); ++i) {
            Job* job = enabled[i];
            job->initAutoParas();
            const ToolOut r = job->run(sol);
            ++sr.jobsRun;
            sr.image = r.image;
            sr.objects = r.objects;
            if (sr.source.isNull() && !r.source.isNull()) {
                sr.source = r.source;
            }
            bus.insert(job->config()->id.toString() + ".ok", r.ok);
            bus.insert(job->config()->id.toString() + ".fromJobId", job->config()->id.toString());
            if (!r.ok) {
                sr.ok = false;
                if (m_config->failPolicy == FailStopStation
                    || job->config()->failPolicy == FailStopStation) {
                    if (m_config->jobSchedule == ScheduleSequence) {
                        break;
                    }
                }
            }
        }
    }

    watch.end("cycle");
    sr.cycleMs = static_cast<int>(watch.ms("cycle"));
    sr.summary = Translate::format("{0}  {1} 条流程  {2}ms",
                                   sr.ok ? std::string("OK") : std::string("NG"),
                                   std::to_string(sr.jobsRun),
                                   std::to_string(sr.cycleMs));
    m_runtime.lastOk = sr.ok;
    m_runtime.lastCycleMs = sr.cycleMs;
    m_runtime.lastSummary = sr.summary;
    log(sr.ok ? RunLogInfo : RunLogWarn,
        Translate::format("工站结束 {0}  {1}", m_config->name, sr.summary));
    return sr;
}

// 当前配置
StationCfg* Station::config() const
{
    return m_config;
}

// 运行态
StationRuntime& Station::runtime()
{
    return m_runtime;
}

// 运行态（只读）
const StationRuntime& Station::runtime() const
{
    return m_runtime;
}

// 流程数
int Station::jobCount() const
{
    return static_cast<int>(m_jobs.size());
}

// 是否运行中
bool Station::isRunning() const
{
    return m_runtime.running;
}



}
