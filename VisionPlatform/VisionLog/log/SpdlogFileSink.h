/**
 * @file SpdlogFileSink.h
 * @brief SpdlogFileSink头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SPDLOGFILESINK_H
#define VISION_SPDLOGFILESINK_H
#include "VisionLogExport.h"

#include "LogMgr.h"

namespace Vision {
namespace detail {

/**
 * @class SpdlogFileSink
 * @brief 按 spdlog rotating_file_sink + async 语义实现的输出端：
 *        后台线程写入 SQLite。不引入 fmt，以便 VS2015 / C++11 可编译。
 */
class VISION_LOG_API SpdlogFileSink {
public:
    /** @brief 创建队列和同步事件 */
    SpdlogFileSink();
    /** @brief 停写线程并释放句柄 */
    ~SpdlogFileSink();

    /** @brief 启动后台写库线程 */
    void start();
    /** @brief 停线程并刷完队列 */
    void stop();
    /** @brief 入队；满时丢弃低于 minKeep 的记录，审计与错误尽量保留 */
    void enqueue(const LogRecord& rec);
    /** @brief 等到队列写完，供查询前调用 */
    void flush();
    /** @brief 该通道低于此级别的记录不入库 */
    void setMinLevel(LogChannel ch, LogLevel minLevel);
    /** @brief 关掉的通道不再入队 */
    void setEnabled(LogChannel ch, bool on);
    /** @brief 该通道当前最低级别 */
    LogLevel minLevel(LogChannel ch) const;
    /** @brief 该通道是否在写 */
    bool enabled(LogChannel ch) const;
    /** @brief 后台线程入口 */
    void runLoop();
    struct Impl;
    Impl* m; ///< 队列、线程与通道开关

private:
    /** @brief 禁止拷贝 */
    SpdlogFileSink(const SpdlogFileSink&);
    /** @brief 禁止赋值 */
    SpdlogFileSink& operator=(const SpdlogFileSink&);
};

/** @brief 进程内唯一的异步输出端 */
VISION_LOG_API SpdlogFileSink& logSink();

}

}

#endif
