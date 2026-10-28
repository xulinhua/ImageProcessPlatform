/**
 * @file ClockLog.h
 * @brief 见过的最晚时间。三处算法不同。重写只发生在接受一枚更新的注册码时
 */

#ifndef VISION_CLOCK_LOG_H
#define VISION_CLOCK_LOG_H

namespace Vision {

class ClockLog {
public:
    /** @brief 能解开的记录里最晚的 Unix 秒。found 是解开的份数 */
    bool latest(long long* stamp, int* found) const;
    /** @brief 把现在写进三处 */
    bool writeNow() const;
    /** @brief 接受更新的注册码之后写成现在 */
    bool reset() const;
    long long now() const;
};

}

#endif
