/**
 * @file LogDb.cpp
 * @brief LogDb实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LogDb.h"
#include "AppPaths.h"
#include "SqliteDatabase.h"
#include "TextEncoding.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

// 从单元格取出文本
std::string asText(const DbValue& v)
{
    if (v.kind == DbValue::Text) {
        return v.text;
    }
    if (v.kind == DbValue::Int) {
        char buf[32];
        sprintf(buf, "%lld", static_cast<long long>(v.i));
        return std::string(buf);
    }
    return std::string();
}

// 从单元格取出整数
int64_t asInt(const DbValue& v)
{
    if (v.kind == DbValue::Int) {
        return v.i;
    }
    if (v.kind == DbValue::Text) {
        return atoi(v.text.c_str());
    }
    return 0;
}

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

// 保留天数之前的截止日期
std::string cutDay(int keepDays)
{
    time_t now = time(0);
    if (keepDays < 1) {
        keepDays = 1;
    }
    now -= static_cast<time_t>(keepDays) * 24 * 3600;
    struct tm lt;
    localtime_s(&lt, &now);
    char buf[16];
    sprintf(buf, "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    return std::string(buf);
}

// 种类对应的逻辑表和删除键前缀
const char* kindKey(LogKind k)
{
    if (k == LogKindDebug) {
        return "debug";
    }
    if (k == LogKindRun) {
        return "run";
    }
    return "op";
}

const char* logicalOf(LogKind k)
{
    if (k == LogKindDebug) {
        return "debug_log";
    }
    if (k == LogKindRun) {
        return "run_log";
    }
    return "op_log";
}

int keepDaysOf(LogKind k)
{
    if (k == LogKindDebug) {
        return 7;
    }
    if (k == LogKindRun) {
        return 30;
    }
    return 180;
}

// 拆开 op|日期、debug|日期 或 run|日期
bool splitKey(const std::string& key, LogKind* kind, std::string* day)
{
    const size_t bar = key.find('|');
    if (!kind || !day || bar == std::string::npos || bar + 1 >= key.size()) {
        return false;
    }
    const std::string ks = key.substr(0, bar);
    *day = key.substr(bar + 1);
    if (day->size() != 10) {
        return false;
    }
    if (ks == "debug") {
        *kind = LogKindDebug;
    } else if (ks == "run") {
        *kind = LogKindRun;
    } else if (ks == "op") {
        *kind = LogKindOp;
    } else {
        return false;
    }
    return true;
}

// 查询行还原成一条日志
LogRecord rowToRecord(const std::vector<DbValue>& r)
{
    LogRecord rec;
    if (r.size() < 14) {
        return rec;
    }
    rec.timeIso = asText(r[0]);
    rec.kind = static_cast<LogKind>(asInt(r[1]));
    rec.channel = static_cast<LogChannel>(asInt(r[2]));
    rec.level = static_cast<LogLevel>(asInt(r[3]));
    rec.action = asText(r[4]);
    rec.user = asText(r[5]);
    rec.role = asText(r[6]);
    rec.target = asText(r[7]);
    rec.result = asText(r[8]);
    rec.detail = asText(r[9]);
    rec.stationId = asText(r[10]);
    rec.jobId = asText(r[11]);
    rec.toolId = asText(r[12]);
    rec.elapsedMs = static_cast<int>(asInt(r[13]));
    return rec;
}

} // namespace

struct LogDb::Impl {
    CRITICAL_SECTION cs; ///< 保护连接
    SqliteDatabase db;   ///< data\log.db
    bool ready;          ///< 表已就绪

    Impl()
        : ready(false)
    {
        InitializeCriticalSection(&cs);
    }
    ~Impl()
    {
        DeleteCriticalSection(&cs);
    }
};

// 进程内唯一日志库
LogDb& LogDb::instance()
{
    static LogDb g;
    return g;
}

// 分配实现
LogDb::LogDb()
    : m(new Impl())
{
}

// 释放实现
LogDb::~LogDb()
{
    if (m) {
        delete m;
        m = nullptr;
    }
}

// 打开日志库。月表在写入时创建
bool LogDb::ensureOpen()
{
    if (m->ready) {
        return true;
    }
    AppPaths::ensureDirs();
    std::string err;
    if (!m->db.open(AppPaths::logFile(), &err)) {
        return false;
    }
    SchemaMigrator mig(&m->db);
    if (!mig.ensureVersionTable(&err)) {
        return false;
    }
    if (mig.currentVersion(&err) < 1) {
        importLegacy();
        if (!mig.setVersion(1, &err)) {
            return false;
        }
    }
    m->ready = true;
    return true;
}

// 没有这张月表就创建
bool LogDb::ensureTable(const std::string& table)
{
    if (!TableRouter::isSafeIdent(table)) {
        return false;
    }
    const std::string sql =
        "CREATE TABLE IF NOT EXISTS " + table + " ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "time_iso TEXT NOT NULL,"
        "kind INTEGER NOT NULL,"
        "channel INTEGER NOT NULL,"
        "level INTEGER NOT NULL,"
        "action TEXT,"
        "login_name TEXT,"
        "role TEXT,"
        "target TEXT,"
        "result TEXT,"
        "detail TEXT,"
        "station_id TEXT,"
        "job_id TEXT,"
        "tool_id TEXT,"
        "elapsed_ms INTEGER);";
    if (!m->db.exec(sql, 0)) {
        return false;
    }
    const std::string index = "idx_" + table + "_time";
    if (!TableRouter::isSafeIdent(index)) {
        return false;
    }
    return m->db.exec("CREATE INDEX IF NOT EXISTS " + index + " ON " + table + "(time_iso);", 0);
}

// 调试先进调试表；运行通道进运行诊断；其余是操作
std::string LogDb::tableFor(const LogRecord& rec) const
{
    LogKind kind = LogKindOp;
    if (rec.kind == LogKindDebug) {
        kind = LogKindDebug;
    } else if (rec.kind == LogKindRun || rec.channel == LogChannelRun) {
        kind = LogKindRun;
    }
    return TableRouter::physical(logicalOf(kind), rec.timeIso);
}

// 列出这个种类在日期范围内的物理表
std::vector<std::string> LogDb::tablesFor(LogKind kind, const std::string& dayFrom, const std::string& dayTo)
{
    std::vector<std::string> out;
    const std::string logical = logicalOf(kind);
    std::vector<DbValue> args;
    args.push_back(DbValue::fromText(logical + "_%"));
    std::vector<std::vector<DbValue> > rows;
    if (!m->db.query(
            "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE ?1 ORDER BY name;",
            args, &rows, 0)) {
        return out;
    }
    const std::string fromMonth = dayFrom.empty() ? std::string() : TableRouter::monthSuffix(dayFrom);
    const std::string toMonth = dayTo.empty() ? std::string() : TableRouter::monthSuffix(dayTo);
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].empty()) {
            continue;
        }
        const std::string name = asText(rows[i][0]);
        if (!TableRouter::isSafeIdent(name) || name.compare(0, logical.size() + 1, logical + "_") != 0) {
            continue;
        }
        const std::string suffix = name.substr(logical.size() + 1);
        if (!fromMonth.empty() && suffix < fromMonth) {
            continue;
        }
        if (!toMonth.empty() && suffix > toMonth) {
            continue;
        }
        out.push_back(name);
    }
    return out;
}

// 把旧 vision.db 里的 app_log 按月搬进新库
bool LogDb::importLegacy()
{
    const std::wstring path = utf8ToWide(AppPaths::legacyDatabaseFile());
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    SqliteDatabase oldDb;
    std::string err;
    if (!oldDb.open(AppPaths::legacyDatabaseFile(), &err)) {
        return true;
    }
    std::vector<std::vector<DbValue> > rows;
    if (!oldDb.query(
            "SELECT time_iso,kind,channel,level,action,login_name,role,target,result,detail,"
            "station_id,job_id,tool_id,elapsed_ms FROM app_log;",
            std::vector<DbValue>(), &rows, 0)) {
        return true;
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        LogRecord rec = rowToRecord(rows[i]);
        const std::string table = tableFor(rec);
        if (!ensureTable(table)) {
            return false;
        }
        std::vector<DbValue> a;
        a.push_back(DbValue::fromText(rec.timeIso));
        a.push_back(DbValue::fromInt(rec.kind));
        a.push_back(DbValue::fromInt(rec.channel));
        a.push_back(DbValue::fromInt(rec.level));
        a.push_back(DbValue::fromText(rec.action));
        a.push_back(DbValue::fromText(rec.user));
        a.push_back(DbValue::fromText(rec.role));
        a.push_back(DbValue::fromText(rec.target));
        a.push_back(DbValue::fromText(rec.result));
        a.push_back(DbValue::fromText(rec.detail));
        a.push_back(DbValue::fromText(rec.stationId));
        a.push_back(DbValue::fromText(rec.jobId));
        a.push_back(DbValue::fromText(rec.toolId));
        a.push_back(DbValue::fromInt(rec.elapsedMs));
        const std::string sql =
            "INSERT INTO " + table + "(time_iso,kind,channel,level,action,login_name,role,target,result,"
            "detail,station_id,job_id,tool_id,elapsed_ms) "
            "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14);";
        if (!m->db.exec(sql, a, 0)) {
            return false;
        }
    }
    return true;
}

// 插入一条日志
static bool insertOne(SqliteDatabase& db, const std::string& table, const LogRecord& in)
{
    LogRecord r = in;
    if (r.timeIso.empty()) {
        r.timeIso = nowIso();
    }
    if (r.result.empty()) {
        r.result = "ok";
    }
    std::vector<DbValue> a;
    a.push_back(DbValue::fromText(r.timeIso));
    a.push_back(DbValue::fromInt(r.kind));
    a.push_back(DbValue::fromInt(r.channel));
    a.push_back(DbValue::fromInt(r.level));
    a.push_back(DbValue::fromText(r.action));
    a.push_back(DbValue::fromText(r.user));
    a.push_back(DbValue::fromText(r.role));
    a.push_back(DbValue::fromText(r.target));
    a.push_back(DbValue::fromText(r.result));
    a.push_back(DbValue::fromText(r.detail));
    a.push_back(DbValue::fromText(r.stationId));
    a.push_back(DbValue::fromText(r.jobId));
    a.push_back(DbValue::fromText(r.toolId));
    a.push_back(DbValue::fromInt(r.elapsedMs));
    return db.exec(
        "INSERT INTO " + table + "(time_iso,kind,channel,level,action,login_name,role,target,result,"
        "detail,station_id,job_id,tool_id,elapsed_ms) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14);",
        a, 0);
}

// 在一个事务里批量插入
bool LogDb::insertMany(const std::vector<LogRecord>& rows)
{
    if (rows.empty()) {
        return true;
    }
    EnterCriticalSection(&m->cs);
    if (!ensureOpen()) {
        LeaveCriticalSection(&m->cs);
        return false;
    }
    if (!m->db.begin(0)) {
        LeaveCriticalSection(&m->cs);
        return false;
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        const std::string table = tableFor(rows[i]);
        if (!ensureTable(table) || !insertOne(m->db, table, rows[i])) {
            m->db.rollback(0);
            LeaveCriticalSection(&m->cs);
            return false;
        }
    }
    const bool ok = m->db.commit(0);
    if (!ok) {
        m->db.rollback(0);
    }
    LeaveCriticalSection(&m->cs);
    return ok;
}

// 按种类和日期查询，新的在前
std::vector<LogRecord> LogDb::query(const LogQuery& q)
{
    std::vector<LogRecord> out;
    EnterCriticalSection(&m->cs);
    if (!ensureOpen()) {
        LeaveCriticalSection(&m->cs);
        return out;
    }
    const std::vector<std::string> tables = tablesFor(q.kind, q.dayFrom, q.dayTo);
    const int cap = q.maxRows > 0 ? q.maxRows : 500;
    for (size_t ti = tables.size(); ti > 0 && static_cast<int>(out.size()) < cap; --ti) {
        const std::string& table = tables[ti - 1];
        std::string sql =
            "SELECT time_iso,kind,channel,level,action,login_name,role,target,result,detail,"
            "station_id,job_id,tool_id,elapsed_ms FROM " + table + " WHERE 1=1";
        std::vector<DbValue> args;
        int n = 1;
        if (!q.dayFrom.empty()) {
            char mark[16];
            sprintf(mark, "?%d", n++);
            sql += " AND time_iso>=";
            sql += mark;
            args.push_back(DbValue::fromText(q.dayFrom));
        }
        if (!q.dayTo.empty()) {
            char mark[16];
            sprintf(mark, "?%d", n++);
            sql += " AND time_iso<=";
            sql += mark;
            args.push_back(DbValue::fromText(q.dayTo + "T99"));
        }
        sql += " ORDER BY time_iso DESC";
        char lim[32];
        sprintf(lim, " LIMIT %d", cap);
        sql += lim;
        sql += ";";
        std::vector<std::vector<DbValue> > rows;
        if (!m->db.query(sql, args, &rows, 0)) {
            continue;
        }
        for (size_t i = 0; i < rows.size() && static_cast<int>(out.size()) < cap; ++i) {
            out.push_back(rowToRecord(rows[i]));
        }
    }
    LeaveCriticalSection(&m->cs);
    return out;
}

// 列出有记录的日期
std::vector<LogFileInfo> LogDb::listDays()
{
    std::vector<LogFileInfo> out;
    EnterCriticalSection(&m->cs);
    if (!ensureOpen()) {
        LeaveCriticalSection(&m->cs);
        return out;
    }
    const LogKind kinds[3] = {LogKindOp, LogKindRun, LogKindDebug};
    for (int k = 0; k < 3; ++k) {
        const std::vector<std::string> tables = tablesFor(kinds[k], std::string(), std::string());
        for (size_t t = 0; t < tables.size(); ++t) {
            std::vector<std::vector<DbValue> > rows;
            const std::string sql =
                "SELECT substr(time_iso,1,10) AS day FROM " + tables[t]
                + " GROUP BY day ORDER BY day DESC;";
            if (!m->db.query(sql, std::vector<DbValue>(), &rows, 0)) {
                continue;
            }
            for (size_t i = 0; i < rows.size(); ++i) {
                if (rows[i].empty()) {
                    continue;
                }
                LogFileInfo fi;
                fi.kind = kinds[k];
                fi.day = asText(rows[i][0]);
                fi.fileName = fi.day;
                fi.path = std::string(kindKey(fi.kind)) + "|" + fi.day;
                out.push_back(fi);
            }
        }
    }
    LeaveCriticalSection(&m->cs);
    return out;
}

// 按日期键删除该日该种类
int LogDb::deleteDays(const std::vector<std::string>& keys)
{
    int n = 0;
    EnterCriticalSection(&m->cs);
    if (!ensureOpen()) {
        LeaveCriticalSection(&m->cs);
        return 0;
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        LogKind kind = LogKindOp;
        std::string day;
        if (!splitKey(keys[i], &kind, &day)) {
            continue;
        }
        const std::string table = TableRouter::physical(logicalOf(kind), day);
        if (!TableRouter::isSafeIdent(table)) {
            continue;
        }
        std::vector<DbValue> a;
        a.push_back(DbValue::fromText(day));
        if (m->db.exec(
                "DELETE FROM " + table + " WHERE substr(time_iso,1,10)=?1;",
                a, 0)) {
            ++n;
        }
    }
    LeaveCriticalSection(&m->cs);
    return n;
}

// 按种类丢掉过期记录，空月表直接删掉
void LogDb::prune()
{
    EnterCriticalSection(&m->cs);
    if (!ensureOpen()) {
        LeaveCriticalSection(&m->cs);
        return;
    }
    const LogKind kinds[3] = {LogKindOp, LogKindRun, LogKindDebug};
    for (int k = 0; k < 3; ++k) {
        const std::string cut = cutDay(keepDaysOf(kinds[k]));
        const std::vector<std::string> tables = tablesFor(kinds[k], std::string(), std::string());
        for (size_t t = 0; t < tables.size(); ++t) {
            std::vector<DbValue> a;
            a.push_back(DbValue::fromText(cut));
            m->db.exec("DELETE FROM " + tables[t] + " WHERE time_iso < ?1;", a, 0);
            std::vector<std::vector<DbValue> > rows;
            if (!m->db.query("SELECT COUNT(*) FROM " + tables[t] + ";", std::vector<DbValue>(), &rows, 0)) {
                continue;
            }
            if (!rows.empty() && !rows[0].empty() && asInt(rows[0][0]) == 0) {
                m->db.exec("DROP TABLE " + tables[t] + ";", 0);
            }
        }
    }
    LeaveCriticalSection(&m->cs);
}

}
