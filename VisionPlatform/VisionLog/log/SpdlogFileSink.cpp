/**
 * @file SpdlogFileSink.cpp
 * @brief SpdlogFileSink实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SpdlogFileSink.h"
#include "LogDb.h"
#include <queue>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {
namespace detail {

namespace {

enum {
    kMaxQueue = 4096, ///< 待写队列上限
    kKeepDays = 30    ///< 入库后保留天数
};

} // namespace

struct SpdlogFileSink::Impl {
    CRITICAL_SECTION cs;     ///< 保护队列
    HANDLE thread;            ///< 后台线程
    HANDLE wake;              ///< 有新记录或要退出
    HANDLE flushed;           ///< 刷队列完成
    bool stop;                ///< 请求退出
    bool started;             ///< 线程已启动
    bool flushReq;            ///< 请求刷完队列
    std::queue<LogRecord> q;  ///< 待写队列
    LogLevel minLv[4];        ///< 各通道最低级别
    bool on[4];               ///< 各通道是否启用

    Impl()
        : thread(0)
        , wake(0)
        , flushed(0)
        , stop(false)
        , started(false)
        , flushReq(false)
    {
        InitializeCriticalSection(&cs);
        wake = CreateEventW(0, FALSE, FALSE, 0);
        flushed = CreateEventW(0, TRUE, TRUE, 0);
        for (int i = 0; i < 4; ++i) {
            minLv[i] = (i == LogChannelAlgo) ? LogLevelWarn : LogLevelInfo;
            on[i] = (i != LogChannelAlgo);
        }
        minLv[LogChannelAudit] = LogLevelInfo;
    }
};

// 后台线程入口，转调写库循环
static DWORD WINAPI sinkThread(LPVOID p)
{
    static_cast<SpdlogFileSink*>(p)->runLoop();
    return 0;
}

// 创建队列和同步事件
SpdlogFileSink::SpdlogFileSink()
    : m(new Impl())
{
}

// 停写线程并释放句柄
SpdlogFileSink::~SpdlogFileSink()
{
    stop();
    DeleteCriticalSection(&m->cs);
    if (m->wake) {
        CloseHandle(m->wake);
    }
    if (m->flushed) {
        CloseHandle(m->flushed);
    }
    if (m) {
        delete m;
        m = nullptr;
    }
}

// 启动后台写库线程
void SpdlogFileSink::start()
{
    EnterCriticalSection(&m->cs);
    if (m->started) {
        LeaveCriticalSection(&m->cs);
        return;
    }
    m->stop = false;
    m->started = true;
    m->thread = CreateThread(0, 0, sinkThread, this, 0, 0);
    LeaveCriticalSection(&m->cs);
}

// 停线程并刷完队列
void SpdlogFileSink::stop()
{
    EnterCriticalSection(&m->cs);
    if (!m->started) {
        LeaveCriticalSection(&m->cs);
        return;
    }
    m->stop = true;
    LeaveCriticalSection(&m->cs);
    if (m->wake) {
        SetEvent(m->wake);
    }
    if (m->thread) {
        WaitForSingleObject(m->thread, 5000);
        CloseHandle(m->thread);
        m->thread = 0;
    }
    m->started = false;
}

// 该通道低于此级别的记录不入库
void SpdlogFileSink::setMinLevel(LogChannel ch, LogLevel minLevel)
{
    const int i = static_cast<int>(ch);
    if (i < 0 || i > 3) {
        return;
    }
    EnterCriticalSection(&m->cs);
    m->minLv[i] = minLevel;
    LeaveCriticalSection(&m->cs);
}

// 关掉的通道不再入队
void SpdlogFileSink::setEnabled(LogChannel ch, bool on)
{
    const int i = static_cast<int>(ch);
    if (i < 0 || i > 3) {
        return;
    }
    EnterCriticalSection(&m->cs);
    m->on[i] = on;
    LeaveCriticalSection(&m->cs);
}

// 该通道当前最低级别
LogLevel SpdlogFileSink::minLevel(LogChannel ch) const
{
    const int i = static_cast<int>(ch);
    if (i < 0 || i > 3) {
        return LogLevelInfo;
    }
    return m->minLv[i];
}

// 该通道是否在写
bool SpdlogFileSink::enabled(LogChannel ch) const
{
    const int i = static_cast<int>(ch);
    if (i < 0 || i > 3) {
        return false;
    }
    return m->on[i];
}

// 入队；满时尽量留下审计和错误
void SpdlogFileSink::enqueue(const LogRecord& rec)
{
    const int i = static_cast<int>(rec.channel);
    if (i < 0 || i > 3) {
        return;
    }
    EnterCriticalSection(&m->cs);
    if (!m->on[i] || rec.level < m->minLv[i]) {
        LeaveCriticalSection(&m->cs);
        return;
    }
    if (static_cast<int>(m->q.size()) >= kMaxQueue) {
        const bool keep = (rec.channel == LogChannelAudit) || (rec.level >= LogLevelError);
        if (!keep) {
            LeaveCriticalSection(&m->cs);
            return;
        }
        std::queue<LogRecord> rest;
        bool dropped = false;
        while (!m->q.empty()) {
            LogRecord f = m->q.front();
            m->q.pop();
            if (!dropped && f.channel != LogChannelAudit && f.level < LogLevelError) {
                dropped = true;
                continue;
            }
            rest.push(f);
        }
        m->q.swap(rest);
        if (!dropped && !m->q.empty()) {
            m->q.pop();
        }
    }
    m->q.push(rec);
    LeaveCriticalSection(&m->cs);
    if (m->wake) {
        SetEvent(m->wake);
    }
}

// 等到队列写完
void SpdlogFileSink::flush()
{
    if (!m->started) {
        std::vector<LogRecord> batch;
        EnterCriticalSection(&m->cs);
        while (!m->q.empty()) {
            batch.push_back(m->q.front());
            m->q.pop();
        }
        LeaveCriticalSection(&m->cs);
        LogDb::instance().insertMany(batch);
        return;
    }
    EnterCriticalSection(&m->cs);
    m->flushReq = true;
    if (m->flushed) {
        ResetEvent(m->flushed);
    }
    LeaveCriticalSection(&m->cs);
    if (m->wake) {
        SetEvent(m->wake);
    }
    if (m->flushed) {
        WaitForSingleObject(m->flushed, 5000);
    }
}

// 后台循环：出队写库，并按天清理
void SpdlogFileSink::runLoop()
{
    LogDb::instance().pruneOlderThanDays(kKeepDays);
    for (;;) {
        WaitForSingleObject(m->wake, 500);
        for (;;) {
            std::vector<LogRecord> batch;
            EnterCriticalSection(&m->cs);
            while (!m->q.empty() && batch.size() < 64) {
                batch.push_back(m->q.front());
                m->q.pop();
            }
            const bool more = !m->q.empty();
            const bool doFlush = m->flushReq && !more;
            if (doFlush) {
                m->flushReq = false;
            }
            const bool stopping = m->stop && !more;
            LeaveCriticalSection(&m->cs);
            if (!batch.empty()) {
                LogDb::instance().insertMany(batch);
            }
            if (doFlush && m->flushed) {
                SetEvent(m->flushed);
            }
            if (more) {
                continue;
            }
            if (stopping) {
                return;
            }
            break;
        }
    }
}

// 进程内唯一的异步输出端
SpdlogFileSink& logSink()
{
    static SpdlogFileSink g;
    return g;
}

}

}
