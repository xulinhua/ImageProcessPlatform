/**
 * @file ThreadPool.cpp
 * @brief ThreadPool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ThreadPool.h"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Vision {

struct ThreadPool::Impl {
    std::mutex mutex;                       ///< 队列锁
    std::condition_variable cv;             ///< 有任务或停止
    std::condition_variable idleCv;         ///< 空闲通知
    std::queue<std::function<void()> > tasks; ///< 待执行
    std::vector<std::thread> workers;       ///< 工作线程
    int inflight;                           ///< 正在执行的任务数
    bool stop;                              ///< 析构停止
    bool accepting;                         ///< 是否接受 enqueue
    Impl()
        : inflight(0)
        , stop(false)
        , accepting(true)
    {}
};

namespace {

// 默认线程数：硬件并发，夹在 [2, 8]
std::size_t defaultThreadCount()
{
    std::size_t n = std::thread::hardware_concurrency();
    if (n < 2) {
        n = 2;
    }
    if (n > 8) {
        n = 8;
    }
    return n;
}

} // namespace

// 创建并拉起工作线程
ThreadPool::ThreadPool(std::size_t threadCount)
    : m_impl(new Impl)
    , m_threadCount(threadCount == 0 ? defaultThreadCount() : threadCount)
{
    for (std::size_t i = 0; i < m_threadCount; ++i) {
        m_impl->workers.push_back(std::thread(&ThreadPool::workerLoop, this));
    }
}

// 停止并回收
ThreadPool::~ThreadPool()
{
    if (!m_impl) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->stop = true;
        m_impl->accepting = false;
        while (!m_impl->tasks.empty()) {
            m_impl->tasks.pop();
        }
    }
    m_impl->cv.notify_all();
    for (std::size_t i = 0; i < m_impl->workers.size(); ++i) {
        if (m_impl->workers[i].joinable()) {
            m_impl->workers[i].join();
        }
    }
    if (m_impl) {
        delete m_impl;
        m_impl = nullptr;
    }
}

// 入队
void ThreadPool::enqueue(const std::function<void()>& task)
{
    if (!m_impl || !task) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->stop || !m_impl->accepting) {
            return;
        }
        m_impl->tasks.push(task);
    }
    m_impl->cv.notify_one();
}

// 开关接收
void ThreadPool::setAccepting(bool on)
{
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->accepting = on;
}

// 清空排队
void ThreadPool::clearPending()
{
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    while (!m_impl->tasks.empty()) {
        m_impl->tasks.pop();
    }
    if (m_impl->inflight == 0) {
        m_impl->idleCv.notify_all();
    }
}

// 等待空闲
void ThreadPool::waitIdle()
{
    if (!m_impl) {
        return;
    }
    std::unique_lock<std::mutex> lock(m_impl->mutex);
    m_impl->idleCv.wait(lock, [this]() {
        return m_impl->tasks.empty() && m_impl->inflight == 0;
    });
}

// 退出软件：拒收 + 清排队 + 等在飞
void ThreadPool::drainForShutdown()
{
    setAccepting(false);
    clearPending();
    waitIdle();
}

// 工作线程循环
void ThreadPool::workerLoop()
{
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_impl->mutex);
            m_impl->cv.wait(lock, [this]() {
                return m_impl->stop || !m_impl->tasks.empty();
            });
            if (m_impl->stop && m_impl->tasks.empty()) {
                return;
            }
            if (m_impl->tasks.empty()) {
                continue;
            }
            task = m_impl->tasks.front();
            m_impl->tasks.pop();
            ++m_impl->inflight;
        }
        task();
        {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            --m_impl->inflight;
            if (m_impl->tasks.empty() && m_impl->inflight == 0) {
                m_impl->idleCv.notify_all();
            }
        }
    }
}

// 进程共享池
ThreadPool& ThreadPool::shared()
{
    static ThreadPool pool(0);
    return pool;
}

// 线程数
std::size_t ThreadPool::threadCount() const
{
    return m_threadCount;
}



}
