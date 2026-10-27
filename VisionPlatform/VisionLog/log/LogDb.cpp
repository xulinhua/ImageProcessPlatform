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

// 种类对应的删除键前缀
const char* kindKey(LogKind k)
{
    return k == LogKindDebug ? "debug" : "op";
}

// 拆开 op|日期 或 debug|日期
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
    *kind = (ks == "debug") ? LogKindDebug : LogKindOp;
    return ks == "debug" || ks == "op";
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
    SqliteDatabase db;   ///< 共用 vision.db
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

// 打开共用库并确保 app_log 表存在
bool LogDb::ensureOpen()
{
    if (m->ready) {
        return true;
    }
    AppPaths::ensureDirs();
    std::string err;
    if (!m->db.open(AppPaths::databaseFile(), &err)) {
        return false;
    }
    if (!m->db.exec(
            "CREATE TABLE IF NOT EXISTS app_log ("
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
            "elapsed_ms INTEGER"
            ");",
            0)) {
        return false;
    }
    if (!m->db.exec(
            "CREATE INDEX IF NOT EXISTS idx_app_log_kind_time ON app_log(kind, time_iso);",
            0)) {
        return false;
    }
    m->ready = true;
    return true;
}

// 插入一条日志
static bool insertOne(SqliteDatabase& db, const LogRecord& in)
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
        "INSERT INTO app_log(time_iso,kind,channel,level,action,login_name,role,target,result,"
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
        if (!insertOne(m->db, rows[i])) {
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
    std::string sql =
        "SELECT time_iso,kind,channel,level,action,login_name,role,target,result,detail,"
        "station_id,job_id,tool_id,elapsed_ms FROM app_log WHERE kind=?1";
    std::vector<DbValue> args;
    args.push_back(DbValue::fromInt(q.kind));
    int n = 2;
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
    if (q.maxRows > 0) {
        char lim[32];
        sprintf(lim, " LIMIT %d", q.maxRows);
        sql += lim;
    }
    sql += ";";
    std::vector<std::vector<DbValue> > rows;
    if (!m->db.query(sql, args, &rows, 0)) {
        LeaveCriticalSection(&m->cs);
        return out;
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        out.push_back(rowToRecord(rows[i]));
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
    std::vector<std::vector<DbValue> > rows;
    if (!m->db.query(
            "SELECT kind, substr(time_iso,1,10) AS day FROM app_log "
            "GROUP BY kind, day ORDER BY day DESC;",
            std::vector<DbValue>(), &rows, 0)) {
        LeaveCriticalSection(&m->cs);
        return out;
    }
    LeaveCriticalSection(&m->cs);
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].size() < 2) {
            continue;
        }
        LogFileInfo fi;
        fi.kind = static_cast<LogKind>(asInt(rows[i][0]));
        fi.day = asText(rows[i][1]);
        fi.fileName = fi.day;
        fi.path = std::string(kindKey(fi.kind)) + "|" + fi.day;
        out.push_back(fi);
    }
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
        std::vector<DbValue> a;
        a.push_back(DbValue::fromInt(kind));
        a.push_back(DbValue::fromText(day));
        if (m->db.exec(
                "DELETE FROM app_log WHERE kind=?1 AND substr(time_iso,1,10)=?2;",
                a, 0)) {
            ++n;
        }
    }
    LeaveCriticalSection(&m->cs);
    return n;
}

// 删掉早于保留天数的记录
void LogDb::pruneOlderThanDays(int days)
{
    EnterCriticalSection(&m->cs);
    if (!ensureOpen()) {
        LeaveCriticalSection(&m->cs);
        return;
    }
    std::vector<DbValue> a;
    a.push_back(DbValue::fromText(cutDay(days)));
    m->db.exec("DELETE FROM app_log WHERE time_iso < ?1;", a, 0);
    LeaveCriticalSection(&m->cs);
}

}
