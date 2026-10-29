/**
 * @file LogDb.h
 * @brief LogDb头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LOGDB_H
#define VISION_LOGDB_H
#include "VisionLogExport.h"

#include "LogMgr.h"
#include <vector>

namespace Vision {

/**
 * @class LogDb
 * @brief 日志库 data\log.db。按种类和月份分表，如 op_log_2026_10
 */
class LogDb {
public:
    /** @brief 进程内唯一日志库 */
    static LogDb& instance();

    /** @brief 批量插入；空列表视为成功 */
    bool insertMany(const std::vector<LogRecord>& rows);
    /** @brief 按种类与日期查，新的在前 */
    std::vector<LogRecord> query(const LogQuery& q);
    /** @brief 有记录的日期。path 为 "op|YYYY-MM-DD" 或 "debug|YYYY-MM-DD" */
    std::vector<LogFileInfo> listDays();
    /**
     * @brief 按 listDays 给出的 path 删除该日该种类
     * @return 成功删除的日期个数
     */
    int deleteDays(const std::vector<std::string>& keys);
    /** @brief 按种类丢掉过期月表：操作 180 天，运行诊断 30 天，调试 7 天 */
    void prune();

private:
    /** @brief 分配实现 */
    LogDb();
    /** @brief 释放实现 */
    ~LogDb();
    /** @brief 禁止拷贝 */
    LogDb(const LogDb&);
    /** @brief 禁止赋值 */
    LogDb& operator=(const LogDb&);

    /** @brief 打开日志库，并把旧 vision.db 的 app_log 迁进来一次 */
    bool ensureOpen();
    /** @brief 没有这张月表就创建 */
    bool ensureTable(const std::string& table);
    /** @brief 这一条日志落在哪张物理表 */
    std::string tableFor(const LogRecord& rec) const;
    /** @brief 这个种类、这段日期要查的物理表 */
    std::vector<std::string> tablesFor(LogKind kind, const std::string& dayFrom, const std::string& dayTo);
    /** @brief 从旧 vision.db 拷出 app_log。文件不在则跳过 */
    bool importLegacy();

    struct Impl;
    Impl* m; ///< 连接与临界区
};

}

#endif
