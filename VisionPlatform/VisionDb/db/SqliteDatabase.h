/**
 * @file SqliteDatabase.h
 * @brief SqliteDatabase头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SQLITEDATABASE_H
#define VISION_SQLITEDATABASE_H
#include "VisionDbExport.h"

#include "IDatabase.h"

struct sqlite3;

namespace Vision {

/**
 * @class SqliteDatabase
 * @brief IDatabase 的 SQLite 实现（WAL、忙等待超时）
 */
class VISION_DB_API SqliteDatabase : public IDatabase {
public:
    /** @brief 尚未打开的连接 */
    SqliteDatabase();
    /** @brief 关闭连接 */
    ~SqliteDatabase();

    /** @brief 打开或创建库，并启用 WAL */
    bool open(const std::string& path, std::string* error);
    /** @brief 关闭连接 */
    void close();
    bool isOpen() const;
    /** @brief 执行不返回行的语句 */
    bool exec(const std::string& sql, std::string* error);
    /** @brief 带绑定参数执行，不返回行 */
    bool exec(const std::string& sql, const std::vector<DbValue>& args, std::string* error);
    /** @brief 查询全部结果行 */
    bool query(const std::string& sql, const std::vector<DbValue>& args,
               std::vector<std::vector<DbValue> >* rows, std::string* error);
    /** @brief 先计数，再追加排序与 LIMIT/OFFSET */
    bool queryPage(const std::string& countSql, const std::string& dataSql,
                   const std::vector<DbValue>& args, const PageQuery& page,
                   PageRst* out, std::string* error);
    /** @brief 开始立即事务 */
    bool begin(std::string* error);
    /** @brief 提交事务 */
    bool commit(std::string* error);
    /** @brief 回滚事务 */
    bool rollback(std::string* error);
    /** @brief 最近一次插入的 rowid */
    int64_t lastInsertId() const;
    std::string engineId() const;

private:
    /** @brief 禁止拷贝 */
    SqliteDatabase(const SqliteDatabase&);
    /** @brief 禁止赋值 */
    SqliteDatabase& operator=(const SqliteDatabase&);
    /**
     * @brief 按顺序绑定参数，从 ?1 起
     * @param stmt sqlite3_stmt 指针
     */
    bool bindArgs(void* stmt, const std::vector<DbValue>& args, std::string* error);
    /** @brief 把引擎错误写到 error */
    void fail(std::string* error, const char* what) const;

    sqlite3* m_db; ///< 已打开的连接；空表示未打开
};

}

#endif
