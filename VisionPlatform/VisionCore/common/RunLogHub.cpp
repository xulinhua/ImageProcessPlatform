/**
 * @file RunLogHub.cpp
 * @brief RunLogHub实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RunLogHub.h"
#include <cstdio>
#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

// 进入临界区
void enterCs(void* cs)
{
    if (cs) {
        EnterCriticalSection(static_cast<CRITICAL_SECTION*>(cs));
    }
}

// 离开临界区
void leaveCs(void* cs)
{
    if (cs) {
        LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(cs));
    }
}

} // namespace

// 构造并初始化锁
RunLogHub::RunLogHub()
    : m_cs(nullptr)
{
    CRITICAL_SECTION* cs = new CRITICAL_SECTION;
    InitializeCriticalSection(cs);
    m_cs = cs;
}

// 释放锁
RunLogHub::~RunLogHub()
{
    clearSinks();
    if (m_cs) {
        DeleteCriticalSection(static_cast<CRITICAL_SECTION*>(m_cs));
        delete static_cast<CRITICAL_SECTION*>(m_cs);
        m_cs = nullptr;
    }
}

// 追加 sink
void RunLogHub::addSink(IRunListener* sink)
{
    if (!sink) {
        return;
    }
    enterCs(m_cs);
    if (std::find(m_sinks.begin(), m_sinks.end(), sink) == m_sinks.end()) {
        m_sinks.push_back(sink);
    }
    leaveCs(m_cs);
}

// 移除 sink
void RunLogHub::removeSink(IRunListener* sink)
{
    enterCs(m_cs);
    m_sinks.erase(std::remove(m_sinks.begin(), m_sinks.end(), sink), m_sinks.end());
    leaveCs(m_cs);
}

// 清空
void RunLogHub::clearSinks()
{
    enterCs(m_cs);
    m_sinks.clear();
    leaveCs(m_cs);
}

// sink 个数
int RunLogHub::sinkCount() const
{
    enterCs(m_cs);
    const int n = static_cast<int>(m_sinks.size());
    leaveCs(m_cs);
    return n;
}

// 扇出：先拷贝再回调，避免持锁调用业务
void RunLogHub::onRunLog(const RunLogEvent& ev)
{
    std::vector<IRunListener*> copy;
    enterCs(m_cs);
    copy = m_sinks;
    leaveCs(m_cs);
    for (size_t i = 0; i < copy.size(); ++i) {
        if (copy[i]) {
            copy[i]->onRunLog(ev);
        }
    }
}

// 级别短标签
const char* RunLogEvent::levelTag(RunLogLevel level)
{
    switch (level) {
    case RunLogDebug:
        return "DBG";
    case RunLogInfo:
        return "INF";
    case RunLogWarn:
        return "WRN";
    case RunLogError:
        return "ERR";
    default:
        return "INF";
    }
}

// 界面正文：只留消息与耗时
std::string RunLogEvent::formatMessage() const
{
    std::string s = message;
    if (elapsedMs >= 0) {
        char buf[32];
        sprintf(buf, "  %dms", elapsedMs);
        s += buf;
    }
    return s;
}

// 拼完整诊断行（落盘）
std::string RunLogEvent::formatLine() const
{
    std::string s("[");
    s += RunLogEvent::levelTag(level);
    s += "] ";
    if (!stationId.isNull()) {
        s += "S:";
        s += stationId.toString();
        s += " ";
    }
    if (!jobId.isNull()) {
        s += "J:";
        s += jobId.toString();
        s += " ";
    }
    if (!toolId.isNull()) {
        s += "T:";
        s += toolId.toString();
        s += " ";
    }
    s += formatMessage();
    return s;
}

}
