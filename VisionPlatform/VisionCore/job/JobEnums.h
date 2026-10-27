/**
 * @file JobEnums.h
 * @brief JobEnums头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JOB_ENUMS_H
#define VISION_JOB_ENUMS_H
#include "VisionCoreExport.h"
#include <string>

namespace Vision {

/** @brief 流程实例（JobCfg）触发方式 */
enum TriggerMode {
    TriggerModeUndefined = -1, ///< 未定义
    TriggerSoft = 0,    ///< 软件触发
    TriggerHardwareIo,  ///< 硬件 IO
    TriggerUpstreamJob, ///< 上游 JobCfg 完成
    TriggerLoop,           ///< 循环
    TriggerModeMax ///< 个数，不是合法值
};

/** @brief JobCfg 或工站失败策略 */
enum FailPolicy {
    FailStopStation = 0, ///< 停站
    FailSkip,            ///< 跳过
    FailNgContinue       ///< NG 时继续
};

/** @brief 流程内工具组执行语义 */
enum GroupKind {
    GroupKindUndefined = -1, ///< 未定义
    GroupSequence = 0, ///< 顺序
    GroupParallel,     ///< 并行
    GroupIf,           ///< 条件
    GroupLoop,          ///< 循环
    GroupKindMax ///< 个数，不是合法值
};

/** @brief 条件组 THEN 分支期望 */
enum CondWant {
    CondThenOnOk = 0, ///< 来源 OK 时走 THEN
    CondThenOnNg      ///< 来源 NG 时走 THEN
};

/** @brief 输出结果规则项三态 */
enum RstTermState {
    RstTermIgnore = 0, ///< 不参与本行合成
    RstTermRequireOk,  ///< 要求来源 OK
    RstTermRequireNg   ///< 要求来源 NG（取反）
};

/** @brief 输出结果规则项来源 */
enum RstTermKind {
    RstTermTool = 0, ///< 工具实例
    RstTermRule      ///< 上面行的逻辑判断
};

VISIONCORE_API /** @brief 触发方式界面显示名 */
std::string triggerTitle(TriggerMode t);
VISIONCORE_API /** @brief 失败策略界面显示名 */
std::string failPolicyTitle(FailPolicy p);
VISIONCORE_API /** @brief 工具组语义界面显示名 */
std::string groupKindTitle(GroupKind k);
VISIONCORE_API /** @brief 从整型解析组语义 */
GroupKind groupKindFromInt(int v);
VISIONCORE_API /** @brief 结果规则三态界面显示名 */
std::string resultTermStateTitle(RstTermState s);
VISIONCORE_API /** @brief 结果规则来源界面显示名 */
std::string resultTermKindTitle(RstTermKind k);

}

#endif
