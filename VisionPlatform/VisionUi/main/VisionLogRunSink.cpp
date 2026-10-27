/**
 * @file VisionLogRunSink.cpp
 * @brief VisionLogRunSink实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "VisionLogRunSink.h"
#include "RunLogHub.h"
#include "LogMgr.h"

namespace Vision {

// 默认 Info 起落盘
VisionLogRunSink::VisionLogRunSink()
    : m_minLevel(RunLogInfo)
{
}

// 刷新身份
void VisionLogRunSink::setIdentity(const std::string& user, const std::string& role)
{
    m_user = user;
    m_role = role;
}

// 落盘门槛
void VisionLogRunSink::setMinLevel(RunLogLevel minLevel)
{
    m_minLevel = minLevel;
}

// 写入 VisionLog debug
void VisionLogRunSink::onRunLog(const RunLogEvent& ev)
{
    if (ev.level < m_minLevel) {
        return;
    }
    std::string target;
    if (!ev.stationId.isNull()) {
        target += ev.stationId.toString();
    }
    if (!ev.jobId.isNull()) {
        if (!target.empty()) {
            target += "/";
        }
        target += ev.jobId.toString();
    }
    if (!ev.toolId.isNull()) {
        if (!target.empty()) {
            target += "/";
        }
        target += ev.toolId.toString();
    }
    std::string result = "ok";
    if (ev.level == RunLogWarn) {
        result = "warn";
    } else if (ev.level == RunLogError) {
        result = "fail";
    }
    LogMgr::appendDebug(m_user, m_role, "run_log", target, ev.formatLine(), result);
}

}
