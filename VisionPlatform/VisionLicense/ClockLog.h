/**
 * @file ClockLog.h
 * @brief 见过的最晚时间。三处算法不同。重写只发生在接受一枚更新的注册码时
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CLOCK_LOG_H
#define VISION_CLOCK_LOG_H

namespace Vision {

/**
 * @class ClockLog
 * @brief 防回拨时间记录：HKCU 注册表、ProgramData、本地应用数据各存一份加密的 Unix 秒
 * @note 三处用同一主密钥按位置派生不同子密钥（AES-GCM），任一处能解开即算有记录
 */
class ClockLog {
public:
    /**
     * @brief 读三处记录，取能解开的里面最晚的时间
     * @param stamp 最晚的 Unix 秒，没有记录时为 0，可空
     * @param found 解开的份数（0~3），可空
     * @return 至少解开一份返回 true
     */
    bool latest(long long* stamp, int* found) const;
    /** @brief 把现在写进三处（与 reset 相同） */
    bool writeNow() const;
    /** @brief 接受更新的注册码之后写成现在。任一处写失败返回 false */
    bool reset() const;
    /** @brief 当前系统时间，UTC Unix 秒 */
    long long now() const;
};

}

#endif
