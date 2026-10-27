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
 * @brief 日志表 app_log。与用户库共用 data\vision.db，文本列为 UTF-8
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
    /** @brief 删掉早于保留天数的记录 */
    void pruneOlderThanDays(int days);

private:
    /** @brief 分配实现 */
    LogDb();
    /** @brief 释放实现 */
    ~LogDb();
    /** @brief 禁止拷贝 */
    LogDb(const LogDb&);
    /** @brief 禁止赋值 */
    LogDb& operator=(const LogDb&);

    /** @brief 打开共用库并确保 app_log 表存在 */
    bool ensureOpen();

    struct Impl;
    Impl* m; ///< 连接与临界区
};

}

#endif
