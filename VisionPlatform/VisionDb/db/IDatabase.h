/**
 * @file IDatabase.h
 * @brief IDatabase头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_IDATABASE_H
#define VISION_IDATABASE_H
#include "VisionDbExport.h"

#include "DbTypes.h"

namespace Vision {

/**
 * @class IDatabase
 * @brief 数据库抽象：打开、事务、执行、查询、分页。不含业务表。
 */
class IDatabase {
public:
    virtual ~IDatabase() {}
    /** @brief 打开或创建库文件 */
    virtual bool open(const std::string& path, std::string* error) = 0;
    /** @brief 关闭连接 */
    virtual void close() = 0;
    /** @brief 是否已打开 */
    virtual bool isOpen() const = 0;
    /** @brief 执行不返回行的语句 */
    virtual bool exec(const std::string& sql, std::string* error) = 0;
    /** @brief 带绑定参数执行，不返回行 */
    virtual bool exec(const std::string& sql, const std::vector<DbValue>& args, std::string* error) = 0;
    /** @brief 查询全部结果行 */
    virtual bool query(const std::string& sql, const std::vector<DbValue>& args,
                       std::vector<std::vector<DbValue> >* rows, std::string* error) = 0;
    /**
     * @brief 先计数，再给数据语句追加排序和分页
     * @param countSql 统计总行数
     * @param dataSql 取数据，不要自带 LIMIT
     */
    virtual bool queryPage(const std::string& countSql, const std::string& dataSql,
                           const std::vector<DbValue>& args, const PageQuery& page,
                           PageRst* out, std::string* error) = 0;
    /** @brief 开始事务 */
    virtual bool begin(std::string* error) = 0;
    /** @brief 提交事务 */
    virtual bool commit(std::string* error) = 0;
    /** @brief 回滚事务 */
    virtual bool rollback(std::string* error) = 0;
    /** @brief 最近一次插入的行 id */
    virtual int64_t lastInsertId() const = 0;
    /** @brief 引擎标识，如 sqlite */
    virtual std::string engineId() const = 0;
};

/**
 * @class TableRouter
 * @brief 逻辑表名映射到按月物理表，如 result → result_202609
 */
class VISION_DB_API TableRouter {
public:
    /** @brief 取六位年月；不足则用当前月 */
    static std::string monthSuffix(const std::string& yyyymm);
    /** @brief 逻辑表名加 _YYYYMM */
    static std::string physical(const std::string& logical, const std::string& yyyymm);
    /** @brief 当前月的物理表名 */
    static std::string currentMonthPhysical(const std::string& logical);
};

/**
 * @class SchemaMigrator
 * @brief 按 schema_version 升级
 */
class VISION_DB_API SchemaMigrator {
public:
    /** @brief 绑定要升级的库，不接管其生命周期 */
    explicit SchemaMigrator(IDatabase* db);
    /**
     * @brief 读取 schema 版本
     * @return 失败为 -1
     */
    int currentVersion(std::string* error);
    /** @brief 没有版本表则创建 */
    bool ensureVersionTable(std::string* error);
    /** @brief 写入当前 schema 版本 */
    bool setVersion(int v, std::string* error);

private:
    IDatabase* m_db; ///< 被升级的连接，不拥有
};

}

#endif
