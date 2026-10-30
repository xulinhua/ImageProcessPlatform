/**
 * @file ProdDb.cpp
 * @brief 生产库写线程。一批未跨月用 shot_<编号>，跨月后的新月份用 shot_<编号>_YYYY_MM
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ProdDb.h"
#include "SqliteDatabase.h"
#include "AppPaths.h"
#include "TextEncoding.h"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

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

std::string qident(const std::string& name)
{
    return "\"" + name + "\"";
}

} // namespace

struct ProdDb::Impl {
    std::mutex mu;
    std::condition_variable cv;
    std::deque<ProdShot> q;
    std::thread worker;
    bool started;
    bool stop;
    std::string lastError; ///< 最近一次放弃整批的原因，受 mu 保护
    int64_t dropped;       ///< 累计放弃的拍数，受 mu 保护
    std::function<void(const std::string&, int64_t)> onDrop; ///< 放弃整批时的通知，受 mu 保护

    static const int kMaxTries = 30; ///< 一批写入失败后的最多尝试次数，间隔 1 秒

    struct OpenBatch {
        int64_t id;
        std::string startMonth;
        bool crossed;
        OpenBatch() : id(0), crossed(false) {}
    };
    std::map<std::string, OpenBatch> open;

    Impl()
        : started(false)
        , stop(false)
        , dropped(0)
    {
    }

    // 重试用尽或停止时仍写不进去，记下原因、输出到调试器，再在锁外通知
    void recordDrop(size_t shots, const std::string& err)
    {
        std::function<void(const std::string&, int64_t)> notify;
        std::string reason;
        {
            std::lock_guard<std::mutex> lock(mu);
            dropped += static_cast<int64_t>(shots);
            lastError = err.empty() ? std::string("写入失败") : err;
            reason = lastError;
            notify = onDrop;
        }
        char head[64];
        sprintf(head, "ProdDb: dropped %u shots: ", static_cast<unsigned>(shots));
        OutputDebugStringW(utf8ToWide(head + reason + "\n").c_str());
        if (notify) {
            notify(reason, static_cast<int64_t>(shots));
        }
    }
};

ProdDb& ProdDb::instance()
{
    static ProdDb g;
    return g;
}

ProdDb::ProdDb()
    : m(new Impl())
{
}

ProdDb::~ProdDb()
{
    shutdown();
    delete m;
    m = 0;
}

void ProdDb::startup()
{
    note(ProdShot());
}

void ProdDb::shutdown()
{
    std::thread joining;
    {
        std::lock_guard<std::mutex> lock(m->mu);
        if (!m->started) {
            return;
        }
        m->stop = true;
        joining.swap(m->worker);
        m->started = false;
    }
    m->cv.notify_all();
    if (joining.joinable()) {
        joining.join();
    }
}

void ProdDb::note(const ProdShot& shot)
{
    {
        std::lock_guard<std::mutex> lock(m->mu);
        // 首次入队时启动写线程；打开或建表失败线程退出，下次入队再试
        if (!m->started) {
            if (m->worker.joinable()) {
                m->worker.join();
            }
            m->stop = false;
            m->started = true;
            m->worker = std::thread([this]() {
                SqliteDatabase db;
                std::string err;
                AppPaths::ensureDirs();
                if (!db.open(AppPaths::prodFile(), &err)) {
                    std::lock_guard<std::mutex> lock(m->mu);
                    m->started = false;
                    return;
                }
                // 版本 1 的固定表：批次、MES 待发、报警
                SchemaMigrator mig(&db);
                bool schemaOk = mig.ensureVersionTable(&err);
                if (schemaOk && mig.currentVersion(&err) < 1) {
                    schemaOk = db.exec(
                        "CREATE TABLE IF NOT EXISTS batch ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "code TEXT,"
                        "solution TEXT,"
                        "station_id TEXT,"
                        "started_at TEXT,"
                        "ended_at TEXT,"
                        "ok_count INTEGER,"
                        "ng_count INTEGER,"
                        "note TEXT);",
                        &err);
                    schemaOk = schemaOk && db.exec(
                        "CREATE TABLE IF NOT EXISTS mes_outbox ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "batch_id INTEGER,"
                        "shot_id INTEGER,"
                        "yyyymm TEXT,"
                        "status TEXT,"
                        "try_count INTEGER,"
                        "last_error TEXT);",
                        &err);
                    schemaOk = schemaOk && db.exec(
                        "CREATE TABLE IF NOT EXISTS alarm_event ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "time_iso TEXT,"
                        "station_id TEXT,"
                        "batch_id INTEGER,"
                        "kind TEXT,"
                        "detail TEXT);",
                        &err);
                    schemaOk = schemaOk && mig.setVersion(1, &err);
                }
                if (!schemaOk) {
                    std::lock_guard<std::mutex> lock(m->mu);
                    m->started = false;
                    return;
                }
                // 恢复上次未结束的批次，各工站接着写
                std::vector<std::vector<DbValue> > rows;
                db.query(
                    "SELECT id, station_id, started_at FROM batch "
                    "WHERE ended_at IS NULL OR ended_at='';",
                    std::vector<DbValue>(), &rows, 0);
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (rows[i].size() < 3 || rows[i][1].kind != DbValue::Text) {
                        continue;
                    }
                    Impl::OpenBatch ob;
                    ob.id = rows[i][0].kind == DbValue::Int ? rows[i][0].i : 0;
                    ob.startMonth = TableRouter::monthSuffix(rows[i][2].text);
                    if (ob.id > 0) {
                        m->open[rows[i][1].text] = ob;
                    }
                }
                // 每次最多取 32 拍，一个事务写入；无工站的拍丢弃。
                // 失败的批次留在手里隔 1 秒重试，停止时只再试一次
                std::vector<ProdShot> batch;
                int tries = 0;
                while (true) {
                    bool stopping = false;
                    {
                        std::unique_lock<std::mutex> lock(m->mu);
                        if (batch.empty()) {
                            m->cv.wait(lock, [this]() { return m->stop || !m->q.empty(); });
                            while (!m->q.empty() && batch.size() < 32) {
                                if (!m->q.front().stationId.empty()) {
                                    batch.push_back(m->q.front());
                                }
                                m->q.pop_front();
                            }
                            if (batch.empty()) {
                                if (m->stop) {
                                    break;
                                }
                                continue;
                            }
                            tries = 0;
                        } else {
                            m->cv.wait_for(lock, std::chrono::seconds(1), [this]() { return m->stop; });
                        }
                        stopping = m->stop;
                    }
                    m->cv.notify_all();
                    // 事务回滚后 open 表也要回到事务前，否则会指向已回滚的批次行
                    const std::map<std::string, Impl::OpenBatch> openBefore = m->open;
                    std::string err;
                    const bool began = db.begin(&err);
                    bool okWrite = began;
                    for (size_t i = 0; okWrite && i < batch.size(); ++i) {
                        ProdShot& shot = batch[i];
                        if (shot.timeIso.empty()) {
                            shot.timeIso = nowIso();
                        }
                        // 工站没有打开的批次就新建一条，批次号即行 id
                        Impl::OpenBatch ob;
                        std::map<std::string, Impl::OpenBatch>::iterator it = m->open.find(shot.stationId);
                        if (it == m->open.end()) {
                            std::vector<DbValue> a;
                            a.push_back(DbValue::fromText(std::string()));
                            a.push_back(DbValue::fromText(shot.solution));
                            a.push_back(DbValue::fromText(shot.stationId));
                            a.push_back(DbValue::fromText(shot.timeIso));
                            a.push_back(DbValue::fromText(std::string()));
                            a.push_back(DbValue::fromInt(0));
                            a.push_back(DbValue::fromInt(0));
                            a.push_back(DbValue::fromText(std::string()));
                            if (!db.exec(
                                    "INSERT INTO batch(code,solution,station_id,started_at,ended_at,ok_count,ng_count,note) "
                                    "VALUES(?1,?2,?3,?4,?5,?6,?7,?8);",
                                    a, &err)) {
                                okWrite = false;
                                break;
                            }
                            ob.id = db.lastInsertId();
                            ob.startMonth = TableRouter::monthSuffix(shot.timeIso);
                            char code[32];
                            sprintf(code, "%lld", static_cast<long long>(ob.id));
                            std::vector<DbValue> c;
                            c.push_back(DbValue::fromText(code));
                            c.push_back(DbValue::fromInt(ob.id));
                            if (!db.exec("UPDATE batch SET code=?1 WHERE id=?2;", c, &err)) {
                                okWrite = false;
                                break;
                            }
                            m->open[shot.stationId] = ob;
                        } else {
                            ob = it->second;
                        }
                        // 与批次起始月不同则写入带月份后缀的分表
                        const std::string shotMonth = TableRouter::monthSuffix(shot.timeIso);
                        std::string monthKey;
                        if (shotMonth != ob.startMonth) {
                            ob.crossed = true;
                            monthKey = shotMonth;
                            m->open[shot.stationId] = ob;
                        }
                        const std::string shotTable = TableRouter::batchPhysical("shot", ob.id, monthKey);
                        const std::string valueTable = TableRouter::batchPhysical("shot_value", ob.id, monthKey);
                        const std::string fileTable = TableRouter::batchPhysical("shot_file", ob.id, monthKey);
                        if (!TableRouter::isSafeIdent(shotTable) || !TableRouter::isSafeIdent(valueTable)
                            || !TableRouter::isSafeIdent(fileTable)) {
                            err = "分表名无效";
                            okWrite = false;
                            break;
                        }
                        const std::string createShot =
                            "CREATE TABLE IF NOT EXISTS " + qident(shotTable) + " ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                            "time_iso TEXT, solution TEXT, station_id TEXT, job_id TEXT,"
                            "frame_no INTEGER, barcode TEXT, verdict INTEGER, summary TEXT,"
                            "elapsed_ms INTEGER, cycle_ms INTEGER, image_path TEXT);";
                        const std::string createValue =
                            "CREATE TABLE IF NOT EXISTS " + qident(valueTable) + " ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT, shot_id INTEGER, tool_id TEXT,"
                            "name TEXT, value_num TEXT, value_text TEXT, unit TEXT,"
                            "low TEXT, high TEXT, verdict INTEGER, time_iso TEXT, station_id TEXT);";
                        const std::string createFile =
                            "CREATE TABLE IF NOT EXISTS " + qident(fileTable) + " ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT, shot_id INTEGER, job_id TEXT,"
                            "tool_id TEXT, kind TEXT, path TEXT);";
                        if (!db.exec(createShot, &err) || !db.exec(createValue, &err) || !db.exec(createFile, &err)) {
                            okWrite = false;
                            break;
                        }
                        db.exec("CREATE INDEX IF NOT EXISTS " + qident("idx_" + shotTable + "_time")
                                    + " ON " + qident(shotTable) + "(time_iso);",
                                0);
                        // 写这一拍并累加批次 OK/NG 计数
                        std::vector<DbValue> s;
                        s.push_back(DbValue::fromText(shot.timeIso));
                        s.push_back(DbValue::fromText(shot.solution));
                        s.push_back(DbValue::fromText(shot.stationId));
                        s.push_back(DbValue::fromText(shot.jobId));
                        s.push_back(DbValue::fromInt(shot.frameNo));
                        s.push_back(DbValue::fromText(shot.barcode));
                        s.push_back(DbValue::fromInt(shot.ok ? 1 : 0));
                        s.push_back(DbValue::fromText(shot.summary));
                        s.push_back(DbValue::fromInt(shot.elapsedMs));
                        s.push_back(DbValue::fromInt(shot.cycleMs));
                        s.push_back(DbValue::fromText(shot.imagePath));
                        const std::string insertShot =
                            "INSERT INTO " + qident(shotTable)
                            + "(time_iso,solution,station_id,job_id,frame_no,barcode,verdict,summary,"
                              "elapsed_ms,cycle_ms,image_path) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11);";
                        if (!db.exec(insertShot, s, &err)) {
                            okWrite = false;
                            break;
                        }
                        std::vector<DbValue> u;
                        u.push_back(DbValue::fromInt(shot.ok ? 1 : 0));
                        u.push_back(DbValue::fromInt(shot.ok ? 0 : 1));
                        u.push_back(DbValue::fromInt(ob.id));
                        if (!db.exec("UPDATE batch SET ok_count=ok_count+?1, ng_count=ng_count+?2 WHERE id=?3;", u, &err)) {
                            okWrite = false;
                            break;
                        }
                    }
                    if (okWrite && db.commit(&err)) {
                        batch.clear();
                        continue;
                    }
                    if (began) {
                        db.rollback(0);
                    }
                    m->open = openBefore;
                    if (stopping || ++tries >= Impl::kMaxTries) {
                        m->recordDrop(batch.size(), err);
                        batch.clear();
                    }
                }
            });
        }
        if (m->stop) {
            return;
        }
        m->q.push_back(shot);
    }
    m->cv.notify_all();
}

std::string ProdDb::lastError() const
{
    std::lock_guard<std::mutex> lock(m->mu);
    return m->lastError;
}

int64_t ProdDb::droppedShots() const
{
    std::lock_guard<std::mutex> lock(m->mu);
    return m->dropped;
}

void ProdDb::setDropHandler(const std::function<void(const std::string&, int64_t)>& handler)
{
    std::lock_guard<std::mutex> lock(m->mu);
    m->onDrop = handler;
}

} // namespace Vision
