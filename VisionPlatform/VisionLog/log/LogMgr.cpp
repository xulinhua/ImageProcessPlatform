/**
 * @file LogMgr.cpp
 * @brief LogMgr实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LogMgr.h"
#include "LogDb.h"
#include "SpdlogFileSink.h"
#include "Translate.h"
#include <cstdio>
#include <ctime>

namespace Vision {

namespace {

// 本地时间的 ISO 文本
std::string nowIso()
{
    time_t t = time(0);
    struct tm lt;
    localtime_s(&lt, &t);
    char buf[32];
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d",
            lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
            lt.tm_hour, lt.tm_min, lt.tm_sec);
    return std::string(buf);
}

} // namespace

// 启动异步写库线程
void LogMgr::startup()
{
    detail::logSink().start();
}

// 停线程并刷完队列
void LogMgr::shutdown()
{
    detail::logSink().stop();
}

// 写一条系统通道日志
void LogMgr::writeSystem(LogLevel level, const std::string& action, const std::string& detail)
{
    LogRecord rec;
    rec.channel = LogChannelSystem;
    rec.level = level;
    rec.kind = LogKindOp;
    rec.action = action;
    rec.detail = detail;
    rec.result = "ok";
    detail::logSink().enqueue(rec);
}

// 补上时间后异步入库
void LogMgr::append(const LogRecord& rec)
{
    LogRecord r = rec;
    if (r.timeIso.empty()) {
        r.timeIso = nowIso();
    }
    if (r.result.empty()) {
        r.result = "ok";
    }
    detail::logSink().enqueue(r);
}

// 写一条操作日志
void LogMgr::appendOp(const std::string& user, const std::string& role,
                         const std::string& action, const std::string& target,
                         const std::string& detail, const std::string& result)
{
    LogRecord r;
    r.kind = LogKindOp;
    r.channel = LogChannelAudit;
    r.level = LogLevelInfo;
    r.user = user;
    r.role = role;
    r.action = action;
    r.target = target;
    r.detail = detail;
    r.result = result.empty() ? std::string("ok") : result;
    append(r);
}

// 写一条调试日志
void LogMgr::appendDebug(const std::string& user, const std::string& role,
                            const std::string& action, const std::string& target,
                            const std::string& detail, const std::string& result)
{
    LogRecord r;
    r.kind = LogKindDebug;
    r.channel = LogChannelRun;
    r.level = LogLevelInfo;
    r.user = user;
    r.role = role;
    r.action = action;
    r.target = target;
    r.detail = detail;
    r.result = result.empty() ? std::string("ok") : result;
    append(r);
}

// 先刷队列，再按条件查询
std::vector<LogRecord> LogMgr::query(const LogQuery& q)
{
    detail::logSink().flush();
    return LogDb::instance().query(q);
}

// 先刷队列，再列出有记录的日期
std::vector<LogFileInfo> LogMgr::listFiles()
{
    detail::logSink().flush();
    return LogDb::instance().listDays();
}

// 先刷队列，再按日期键删除
int LogMgr::deleteFiles(const std::vector<std::string>& paths)
{
    detail::logSink().flush();
    return LogDb::instance().deleteDays(paths);
}

// 动作键的中文名，供界面表格
std::string LogMgr::actionTitle(const std::string& action)
{
    if (action == "login") {
        return Translate::tr("登录");
    }
    if (action == "login_fail") {
        return Translate::tr("登录失败");
    }
    if (action == "logout") {
        return Translate::tr("注销");
    }
    if (action == "start_station") {
        return Translate::tr("开始工站");
    }
    if (action == "stop_station") {
        return Translate::tr("停止工站");
    }
    if (action == "reset_count") {
        return Translate::tr("清零产量");
    }
    if (action == "load_solution") {
        return Translate::tr("打开方案");
    }
    if (action == "save_solution") {
        return Translate::tr("保存方案");
    }
    if (action == "debug_tool") {
        return Translate::tr("调试本工具");
    }
    if (action == "debug_job") {
        return Translate::tr("调试本流程");
    }
    if (action == "run_log") {
        return Translate::tr("运行日志");
    }
    if (action == "perm_save") {
        return Translate::tr("保存控件权限");
    }
    if (action == "user_create") {
        return Translate::tr("新建用户");
    }
    if (action == "user_edit") {
        return Translate::tr("编辑用户");
    }
    if (action == "user_delete") {
        return Translate::tr("删除用户");
    }
    if (action == "user_reset_pwd") {
        return Translate::tr("重置口令");
    }
    if (action == "user_change_pwd") {
        return Translate::tr("修改口令");
    }
    if (action == "log_delete") {
        return Translate::tr("删除日志");
    }
    return Translate::tr("其他");
}

}
