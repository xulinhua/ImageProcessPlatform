/**
 * @file SqliteDatabase.cpp
 * @brief SqliteDatabase实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SqliteDatabase.h"
#include "IDatabase.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "sqlite3.h"

namespace Vision {

namespace {

// 写入错误文本；error 为空则忽略
void setErr(std::string* error, const std::string& msg)
{
    if (error) {
        *error = msg;
    }
}

} // namespace

// 尚未打开的连接
SqliteDatabase::SqliteDatabase()
    : m_db(nullptr)
{
}

// 关闭连接
SqliteDatabase::~SqliteDatabase()
{
    close();
}

// 把引擎错误写到 error
void SqliteDatabase::fail(std::string* error, const char* what) const
{
    std::string msg = what ? what : "sqlite error";
    if (m_db) {
        msg += ": ";
        msg += sqlite3_errmsg(m_db);
    }
    setErr(error, msg);
}

// 打开或创建库，并启用 WAL 与忙等待
bool SqliteDatabase::open(const std::string& path, std::string* error)
{
    close();
    sqlite3* db = nullptr;
    const int rc = sqlite3_open_v2(path.c_str(), &db,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                                   0);
    if (rc != SQLITE_OK) {
        setErr(error, db ? sqlite3_errmsg(db) : "sqlite3_open failed");
        if (db) {
            sqlite3_close(db);
        }
        return false;
    }
    m_db = db;
    sqlite3_busy_timeout(m_db, 5000);
    exec("PRAGMA journal_mode=WAL;", 0);
    exec("PRAGMA synchronous=NORMAL;", 0);
    exec("PRAGMA foreign_keys=ON;", 0);
    return true;
}

// 关闭连接
void SqliteDatabase::close()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = 0;
    }
}

// 按顺序绑定参数，从 ?1 起
bool SqliteDatabase::bindArgs(void* stmtRaw, const std::vector<DbValue>& args, std::string* error)
{
    sqlite3_stmt* stmt = static_cast<sqlite3_stmt*>(stmtRaw);
    for (size_t i = 0; i < args.size(); ++i) {
        const int idx = static_cast<int>(i) + 1;
        const DbValue& v = args[i];
        int rc = SQLITE_OK;
        if (v.kind == DbValue::Null) {
            rc = sqlite3_bind_null(stmt, idx);
        } else if (v.kind == DbValue::Int) {
            rc = sqlite3_bind_int64(stmt, idx, v.i);
        } else if (v.kind == DbValue::Blob) {
            const void* p = v.blob.empty() ? 0 : &v.blob[0];
            rc = sqlite3_bind_blob(stmt, idx, p, static_cast<int>(v.blob.size()), SQLITE_TRANSIENT);
        } else {
            rc = sqlite3_bind_text(stmt, idx, v.text.c_str(), -1, SQLITE_TRANSIENT);
        }
        if (rc != SQLITE_OK) {
            fail(error, "bind");
            return false;
        }
    }
    return true;
}

// 执行不返回行的语句
bool SqliteDatabase::exec(const std::string& sql, std::string* error)
{
    return exec(sql, std::vector<DbValue>(), error);
}

// 带绑定参数执行，不返回行
bool SqliteDatabase::exec(const std::string& sql, const std::vector<DbValue>& args, std::string* error)
{
    if (!m_db) {
        setErr(error, "database not open");
        return false;
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        fail(error, "prepare");
        return false;
    }
    if (!bindArgs(stmt, args, error)) {
        sqlite3_finalize(stmt);
        return false;
    }
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        fail(error, "exec");
        return false;
    }
    return true;
}

// 查询全部结果行
bool SqliteDatabase::query(const std::string& sql, const std::vector<DbValue>& args,
                           std::vector<std::vector<DbValue> >* rows, std::string* error)
{
    if (!rows) {
        setErr(error, "rows is null");
        return false;
    }
    rows->clear();
    if (!m_db) {
        setErr(error, "database not open");
        return false;
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        fail(error, "prepare");
        return false;
    }
    if (!bindArgs(stmt, args, error)) {
        sqlite3_finalize(stmt);
        return false;
    }
    // 按列的存储类型取值；浮点等其余类型按文本读出
    const int cols = sqlite3_column_count(stmt);
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<DbValue> row;
        for (int c = 0; c < cols; ++c) {
            DbValue v;
            const int t = sqlite3_column_type(stmt, c);
            if (t == SQLITE_NULL) {
                v.kind = DbValue::Null;
            } else if (t == SQLITE_INTEGER) {
                v.kind = DbValue::Int;
                v.i = sqlite3_column_int64(stmt, c);
            } else if (t == SQLITE_BLOB) {
                v.kind = DbValue::Blob;
                const int n = sqlite3_column_bytes(stmt, c);
                const unsigned char* p = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, c));
                if (p && n > 0) {
                    v.blob.assign(p, p + n);
                }
            } else {
                v.kind = DbValue::Text;
                const unsigned char* p = sqlite3_column_text(stmt, c);
                v.text = p ? reinterpret_cast<const char*>(p) : "";
            }
            row.push_back(v);
        }
        rows->push_back(row);
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fail(error, "query");
        return false;
    }
    return true;
}

// 先计数，再追加排序与分页
bool SqliteDatabase::queryPage(const std::string& countSql, const std::string& dataSql,
                               const std::vector<DbValue>& args, const PageQuery& page,
                               PageRst* out, std::string* error)
{
    if (!out) {
        setErr(error, "out is null");
        return false;
    }
    out->total = 0;
    out->rows.clear();
    std::vector<std::vector<DbValue> > countRows;
    if (!query(countSql, args, &countRows, error)) {
        return false;
    }
    if (!countRows.empty() && !countRows[0].empty() && countRows[0][0].kind == DbValue::Int) {
        out->total = static_cast<int>(countRows[0][0].i);
    } else if (!countRows.empty() && !countRows[0].empty() && countRows[0][0].kind == DbValue::Text) {
        out->total = atoi(countRows[0][0].text.c_str());
    }
    int pageSize = page.pageSize > 0 ? page.pageSize : 50;
    int pageNo = page.page > 0 ? page.page : 1;
    const int offset = (pageNo - 1) * pageSize;
    std::string sql = dataSql;
    if (!page.orderBy.empty()) {
        sql += " ORDER BY ";
        sql += page.orderBy;
    }
    char lim[64];
    sprintf(lim, " LIMIT %d OFFSET %d", pageSize, offset);
    sql += lim;
    return query(sql, args, &out->rows, error);
}

// 开始立即事务
bool SqliteDatabase::begin(std::string* error)
{
    return exec("BEGIN IMMEDIATE;", error);
}

// 提交事务
bool SqliteDatabase::commit(std::string* error)
{
    return exec("COMMIT;", error);
}

// 回滚事务
bool SqliteDatabase::rollback(std::string* error)
{
    return exec("ROLLBACK;", error);
}

// 最近一次插入的 rowid
int64_t SqliteDatabase::lastInsertId() const
{
    return m_db ? sqlite3_last_insert_rowid(m_db) : 0;
}

// 表名字符是否可直接拼进 SQL
bool TableRouter::isSafeIdent(const std::string& name)
{
    if (name.empty() || name.size() > 64) {
        return false;
    }
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

namespace {

// 从文本里取出年、月。认 YYYY-MM、YYYY_MM、YYYYMM
bool readYearMonth(const std::string& text, int* year, int* month)
{
    if (!year || !month || text.size() < 6) {
        return false;
    }
    if (text.size() >= 7 && (text[4] == '-' || text[4] == '_')) {
        *year = atoi(text.substr(0, 4).c_str());
        *month = atoi(text.substr(5, 2).c_str());
        return *year >= 1970 && *month >= 1 && *month <= 12;
    }
    if (text[0] >= '0' && text[0] <= '9') {
        *year = atoi(text.substr(0, 4).c_str());
        *month = atoi(text.substr(4, 2).c_str());
        return *year >= 1970 && *month >= 1 && *month <= 12;
    }
    return false;
}

} // namespace

// 归一成 YYYY_MM。空串或无法识别时用当前月
std::string TableRouter::monthSuffix(const std::string& yyyymm)
{
    int year = 0;
    int month = 0;
    if (!readYearMonth(yyyymm, &year, &month)) {
        time_t t = time(0);
        struct tm lt;
        localtime_s(&lt, &t);
        year = lt.tm_year + 1900;
        month = lt.tm_mon + 1;
    }
    char buf[16];
    sprintf(buf, "%04d_%02d", year, month);
    return std::string(buf);
}

// 逻辑表名加 _YYYY_MM
std::string TableRouter::physical(const std::string& logical, const std::string& yyyymm)
{
    return logical + "_" + monthSuffix(yyyymm);
}

// 当前月的物理表名
std::string TableRouter::currentMonthPhysical(const std::string& logical)
{
    return physical(logical, std::string());
}

// 按批次的物理表。未跨月不带月份
std::string TableRouter::batchPhysical(const std::string& logical, int64_t batchId, const std::string& monthOrEmpty)
{
    char buf[32];
    sprintf(buf, "%lld", static_cast<long long>(batchId));
    std::string name = logical + "_" + buf;
    if (!monthOrEmpty.empty()) {
        name += "_";
        name += monthSuffix(monthOrEmpty);
    }
    return name;
}

// 绑定要升级的库
SchemaMigrator::SchemaMigrator(IDatabase* db)
    : m_db(db)
{
}

// 没有版本表则创建
bool SchemaMigrator::ensureVersionTable(std::string* error)
{
    if (!m_db) {
        setErr(error, "db is null");
        return false;
    }
    return m_db->exec(
        "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY);", error);
}

// 读取 schema 版本，失败返回 -1
int SchemaMigrator::currentVersion(std::string* error)
{
    if (!ensureVersionTable(error)) {
        return -1;
    }
    std::vector<std::vector<DbValue> > rows;
    if (!m_db->query("SELECT COALESCE(MAX(version),0) FROM schema_version;", std::vector<DbValue>(), &rows, error)) {
        return -1;
    }
    if (rows.empty() || rows[0].empty()) {
        return 0;
    }
    if (rows[0][0].kind == DbValue::Int) {
        return static_cast<int>(rows[0][0].i);
    }
    return 0;
}

// 写入当前 schema 版本
bool SchemaMigrator::setVersion(int v, std::string* error)
{
    if (!ensureVersionTable(error)) {
        return false;
    }
    std::vector<DbValue> args;
    args.push_back(DbValue::fromInt(v));
    return m_db->exec("INSERT OR REPLACE INTO schema_version(version) VALUES(?1);", args, error);
}

// 是否已打开
bool SqliteDatabase::isOpen() const
{
    return m_db != 0;
}



// 引擎标识
std::string SqliteDatabase::engineId() const
{
    return "sqlite";
}



}
