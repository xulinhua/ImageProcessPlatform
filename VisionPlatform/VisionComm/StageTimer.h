/**
 * @file StageTimer.h
 * @brief StageTimer头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STAGE_TIMER_H
#define VISION_STAGE_TIMER_H
#include "VisionCommExport.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class StageSpan
 * @brief 一段已经结束的计时
 */
struct StageSpan {
    std::string name; ///< 阶段名
    double ms;        ///< 本段毫秒
    int64_t us;       ///< 本段微秒
    /** @brief 耗时清零 */
    StageSpan();
};

/**
 * @class StageTimer
 * @brief 高精度多段计时。用 QPC，可同时开多段，也可串行切段
 */
class VISION_COMM_API StageTimer {
public:
    /** @brief 空计时器 */
    StageTimer();

    /** @brief 清空已结束片段，并丢掉还没结束的段 */
    void clear();
    /**
     * @brief 开始一段
     * @param name 阶段名。同名仍开着则先结束再重开
     */
    void begin(const std::string& name);
    /**
     * @brief 结束一段
     * @param name 阶段名。空串则结束最近开始的一段
     */
    void end(const std::string& name = std::string());
    /**
     * @brief 结束最近一段并开始下一段
     * @param name 新阶段名
     */
    void lap(const std::string& name);

    /**
     * @brief 某阶段最近一次耗时
     * @return 毫秒。没有已结束的该阶段则 -1
     */
    double ms(const std::string& name) const;
    /**
     * @brief 某阶段最近一次耗时
     * @return 微秒。没有已结束的该阶段则 -1
     */
    int64_t us(const std::string& name) const;
    /** @brief 已结束片段耗时之和，毫秒 */
    double totalMs() const;
    /**
     * @brief 某一阶段的一行文本
     * @return 没有该阶段则空串
     */
    std::string report(const std::string& name) const;
    /** @brief 全部片段汇总，末行是合计 */
    std::string summary() const;
    /** @brief 已结束片段，按结束顺序 */
    const std::vector<StageSpan>& spans() const;

private:
    /**
     * @brief 尚未结束的一段
     */
    struct OpenSpan {
        std::string name; ///< 阶段名
        int64_t tick;     ///< 开始时的 QPC 计数
        /** @brief 计数清零 */
        OpenSpan();
    };

    std::vector<OpenSpan> m_open;   ///< 未结束的段
    std::vector<StageSpan> m_spans; ///< 已结束的段

    /** @brief 当前 QPC 计数。不可用则 0 */
    static int64_t tick();
    /** @brief 计数差换成毫秒和微秒 */
    static void ticksTo(int64_t dt, double* ms, int64_t* us);
    /** @brief 未结束段里同名的下标。没有则 -1 */
    int findOpen(const std::string& name) const;
    /** @brief 按下标结束一段 */
    void closeAt(int index);
};

}

#endif
