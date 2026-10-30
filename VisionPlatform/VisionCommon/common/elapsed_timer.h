/**
 * @file elapsed_timer.h
 * @brief 分段耗时。一只表按标签切段，供工具和算法共用
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ELAPSED_TIMER_H
#define VISION_ELAPSED_TIMER_H

#include <chrono>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class Section
 * @brief 一段耗时。label 只用于打印，按 sections 的顺序读取。
 */
struct Section {
    std::string label; ///< 段名，仅打印用
    double ms = 0;     ///< 本段耗时（毫秒）
};

/**
 * @class SectionTimer
 * @brief 分段计时。start 之后每次 mark 记下刚结束的一段，stop 记下最后一段。
 * @note 未 start 或已 stop 时 mark/stop 不记录
 */
class SectionTimer {
public:
    /** @brief 开始并清空已有分段。 */
    void start()
    {
        sections_.clear();
        mark_ = Clock::now();
        running_ = true;
    }

    /** @brief 记下上一段，标签为 label，并接着计下一段。 */
    void mark(const std::string& label)
    {
        if (!running_) {
            return;
        }
        push(label);
    }

    /** @brief 记下最后一段并停表。 */
    void stop(const std::string& label)
    {
        if (!running_) {
            return;
        }
        push(label);
        running_ = false;
    }

    /** @brief 已记下的分段，顺序与 mark、stop 一致。 */
    const std::vector<Section>& sections() const
    {
        return sections_;
    }

    /** @brief 各段之和。 */
    double totalMs() const
    {
        double sum = 0;
        for (size_t i = 0; i < sections_.size(); ++i) {
            sum += sections_[i].ms;
        }
        return sum;
    }

private:
    using Clock = std::chrono::steady_clock;

    /** @brief 记下从上一标记到现在的一段，并把标记移到现在 */
    void push(const std::string& label)
    {
        const Clock::time_point now = Clock::now();
        Section one;
        one.label = label;
        one.ms = std::chrono::duration<double, std::milli>(now - mark_).count();
        sections_.push_back(one);
        mark_ = now;
    }

    Clock::time_point mark_ {};     ///< 当前段的起点
    std::vector<Section> sections_; ///< 已记下的分段
    bool running_ = false;          ///< 是否在计时（start 后、stop 前）
};

}

#endif
