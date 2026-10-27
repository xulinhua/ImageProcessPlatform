/**
 * @file ThreadPool.h
 * @brief ThreadPool头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_THREADPOOL_H
#define VISION_THREADPOOL_H
#include "VisionCommonExport.h"

#include <cstddef>
#include <functional>

namespace Vision {

/**
 * @class ThreadPool
 * @brief 固定线程数的 C++ 任务池（无 Qt）；设计态/复检/工站并行 Job 共用
 */
class VISION_COMMON_API ThreadPool {
public:
    /**
     * @brief 创建线程池
     * @param threadCount 工作线程数；0 表示按硬件并发（至少 2）
     */
    explicit ThreadPool(std::size_t threadCount = 0);
    /** @brief 停止并 join 全部工作线程 */
    ~ThreadPool();

    /**
     * @brief 投递任务（可从任意线程调用）
     * @param task 无参可调用对象
     */
    void enqueue(const std::function<void()>& task);

    /**
     * @brief 是否仍接受新任务（关闭软件前设为 false）
     * @param on true 接受；false 拒绝 enqueue
     */
    void setAccepting(bool on);

    /**
     * @brief 丢掉尚未开始的排队任务（在飞任务不中断）
     */
    void clearPending();

    /**
     * @brief 等待队列清空且在飞任务结束（销毁 Station/Job 前调用）
     */
    void waitIdle();

    /**
     * @brief 关闭入口：拒收 + 清空排队 + 等待在飞结束（退出软件用）
     */
    void drainForShutdown();

    /** @brief 当前工作线程数 */
    std::size_t threadCount() const;

    /**
     * @brief 进程内共享池（设计调试 + 复检 + 工站并行 Job）
     * @return 单例引用
     */
    static ThreadPool& shared();

private:
    /** @brief 禁止拷贝 */
    ThreadPool(const ThreadPool&);
    /** @brief 禁止赋值 */
    ThreadPool& operator=(const ThreadPool&);

    /** @brief 工作线程循环取任务 */
    void workerLoop();

    /** @class Impl @brief 队列、锁和工作线程 */
    struct Impl;
    Impl* m_impl;                 ///< 隐藏 mutex/queue/thread，避免头文件膨胀
    std::size_t m_threadCount;    ///< 工作线程数
};

}

#endif
