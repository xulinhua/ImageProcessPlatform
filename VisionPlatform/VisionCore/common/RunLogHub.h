/**
 * @file RunLogHub.h
 * @brief RunLogHub头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RUNLOGHUB_H
#define VISION_RUNLOGHUB_H
#include "VisionCoreExport.h"

#include "RunListener.h"
#include <vector>

namespace Vision {

/**
 * @class RunLogHub
 * @brief 运行日志多 sink 分发器（扇出）；不拥有 sink；派发加锁，供多线程 Job 安全写入
 * @note 注册到 Station/Job 的仍是单一 IRunListener*；UI 与 VisionLog 等作为子 sink 挂接
 */
class VISIONCORE_API RunLogHub : public IRunListener {
public:
    /** @brief 构造并初始化锁 */
    RunLogHub();
    /** @brief 释放锁 */
    ~RunLogHub();

    /**
     * @brief 追加 sink（不拥有；重复添加忽略）
     * @param sink 监听器，生命周期须长于本 hub 或先 remove
     */
    void addSink(IRunListener* sink);
    /** @brief 移除 sink；未注册则无操作 */
    void removeSink(IRunListener* sink);
    /** @brief 清空全部 sink */
    void clearSinks();
    /** @brief 当前 sink 个数 */
    int sinkCount() const;

    /** @brief 扇出到全部 sink（持锁拷贝列表后释放，避免 sink 内反注册死锁） */
    void onRunLog(const RunLogEvent& ev);

private:
    mutable void* m_cs;                 ///< CRITICAL_SECTION*，避免头文件拉 windows.h
    std::vector<IRunListener*> m_sinks; ///< 子 sink，不拥有
};


}

#endif
