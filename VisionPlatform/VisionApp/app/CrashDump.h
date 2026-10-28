/**
 * @file CrashDump.h
 * @brief 未处理异常时写 minidump
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CRASH_DUMP_H
#define VISION_CRASH_DUMP_H

namespace Vision {

/**
 * @class CrashDump
 * @brief 进程级未处理异常转储，做法对齐 HAD8601 的 SetCrashDump
 */
class CrashDump {
public:
    /** @brief 登记未处理异常过滤。Qt 初始化可能改掉过滤，入口和进入事件循环前各调一次 */
    static void install();
};

}

#endif
