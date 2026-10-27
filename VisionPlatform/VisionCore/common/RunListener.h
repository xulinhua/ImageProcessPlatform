/**
 * @file RunListener.h
 * @brief RunListener头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RUNLISTENER_H
#define VISION_RUNLISTENER_H
#include "VisionCoreExport.h"

#include "ObjectId.h"
#include <string>

namespace Vision {

/**
 * @brief 运行日志级别（与 UI 着色 / 落盘过滤共用）
 */
enum RunLogLevel {
    RunLogDebug = 0, ///< 细节（条件分支等）
    RunLogInfo,      ///< 正常进度（工具 OK、工站结束）
    RunLogWarn,      ///< 可恢复异常（工具 NG）
    RunLogError      ///< 严重失败（无配置、中止）
};

/**
 * @class RunLogEvent
 * @brief 一条结构化运行日志（非纯字符串）
 */
struct VISIONCORE_API RunLogEvent {
    RunLogLevel level;   ///< 级别
    ObjectId stationId;  ///< 工站配置 id（可空）
    ObjectId jobId;      ///< 流程配置 id（可空）
    ObjectId toolId;     ///< 工具配置 id（可空）
    std::string message; ///< 正文
    int elapsedMs;       ///< 耗时；未知为 -1
    RunLogEvent()
        : level(RunLogInfo)
        , elapsedMs(-1)
    {}
    /** @brief 级别短标签 */
    static const char* levelTag(RunLogLevel level);
    /** @brief 拼给人看的正文 */
    std::string formatMessage() const;
    /** @brief 拼完整诊断行 */
    std::string formatLine() const;
};

/**
 * @class IRunListener
 * @brief 运行日志 sink。HMI / 落盘实现；禁止 Qt。同步扇出，UI sink 须自行切到 UI 线程
 */
class VISIONCORE_API IRunListener {
public:
    virtual ~IRunListener() {} ///< 虚析构
    /** @brief 接收一条结构化运行日志 */
    virtual void onRunLog(const RunLogEvent& ev) = 0;
};

}

#endif
