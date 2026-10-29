/**
 * @file LogMgr.h
 * @brief LogMgr头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_VISIONLOG_H
#define VISION_VISIONLOG_H
#include "VisionLogExport.h"

#include <string>
#include <vector>

namespace Vision {

/** @brief 日志种类：操作、调试、运行诊断 */
enum LogKind {
    LogKindOp = 0, ///< 操作（登录、开始停止、改权限等）
    LogKindDebug,  ///< 调试（运行本工具、本流程）
    LogKindRun     ///< 运行诊断（工站、流程、工具耗时）
};

/** @brief 日志通道 */
enum LogChannel {
    LogChannelSystem = 0, ///< 启动、插件、配置
    LogChannelRun,        ///< 工站 / 流程 / 工具
    LogChannelAlgo,       ///< 算法细节
    LogChannelAudit       ///< 操作审计
};

/** @brief 级别；可比较（Trace < … < Error） */
enum LogLevel {
    LogLevelTrace = 0, ///< 跟踪
    LogLevelDebug,     ///< 调试
    LogLevelInfo,      ///< 信息
    LogLevelWarn,      ///< 警告
    LogLevelError      ///< 错误
};

/**
 * @class LogRecord
 * @brief 一条日志。写入 SQLite，不弹窗
 */
struct LogRecord {
    std::string timeIso; ///< 本地时间
    LogKind kind;        ///< 种类（旧查询）
    LogChannel channel;  ///< 通道（异步输出端）
    LogLevel level;      ///< 级别
    std::string action;  ///< 动作键，如 login、debug_tool
    std::string user;    ///< 登录名
    std::string role;    ///< 角色键
    std::string target;  ///< 对象（方案路径、工具 id 等）
    std::string result;  ///< ok、fail、denied
    std::string detail;  ///< 补充
    std::string stationId; ///< 工站 id
    std::string jobId;     ///< 流程 id
    std::string toolId;    ///< 工具 id
    int elapsedMs;         ///< 耗时毫秒
    LogRecord()
        : kind(LogKindOp)
        , channel(LogChannelAudit)
        , level(LogLevelInfo)
        , elapsedMs(0)
    {
    }
};

/**
 * @class LogFileInfo
 * @brief 库中某一日、某一种类的日志。path 为删除用的键，不是文件路径
 */
struct LogFileInfo {
    std::string path;     ///< 完整路径
    std::string fileName; ///< 文件名
    std::string day;      ///< YYYY-MM-DD
    LogKind kind;         ///< 操作或调试
    LogFileInfo()
        : kind(LogKindOp)
    {
    }
};

/**
 * @class LogQuery
 * @brief 查询条件。日期空则不限制该端
 */
struct LogQuery {
    LogKind kind;        ///< 查哪一类
    std::string dayFrom; ///< 起始日 YYYY-MM-DD，空=不限
    std::string dayTo;   ///< 结束日，空=不限
    int maxRows;         ///< 最多返回条数
    LogQuery()
        : kind(LogKindOp)
        , maxRows(500)
    {
    }
};

/**
 * @class LogMgr
 * @brief 操作日志与调试日志：异步写入数据库、按日查询、按日删除。无 Qt
 */
class VISION_LOG_API LogMgr {
public:
    /** @brief 进程内唯一日志管理 */
    static LogMgr& instance();
    /** @brief 启动异步写入线程 */
    static void startup();
    /** @brief 停线程并刷完队列 */
    static void shutdown();
    /** @brief 系统通道 */
    static void writeSystem(LogLevel level, const std::string& action, const std::string& detail);

    /** @brief 追加一条（异步入库） */
    static void append(const LogRecord& rec);
    /**
     * @brief 写操作日志
     * @param result 默认 ok
     */
    static void appendOp(const std::string& user, const std::string& role,
                         const std::string& action, const std::string& target,
                         const std::string& detail, const std::string& result);
    /** @brief 写调试日志 */
    static void appendDebug(const std::string& user, const std::string& role,
                            const std::string& action, const std::string& target,
                            const std::string& detail, const std::string& result);
    /** @brief 按条件读日志，新的在前 */
    static std::vector<LogRecord> query(const LogQuery& q);
    /** @brief 列出库中有日志的日期 */
    static std::vector<LogFileInfo> listFiles();
    /**
     * @brief 删除指定日期（path 为 listFiles 返回的键）
     * @return 成功删除的日期个数
     */
    static int deleteFiles(const std::vector<std::string>& paths);
    /** @brief 动作键中文名，供界面表格 */
    static std::string actionTitle(const std::string& action);
};

}

#endif
