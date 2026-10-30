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
enum ExecWay {
    ExecActive = 0, ///< 主动执行：工站每拍都跑
    ExecCalled,     ///< 被调用：仅在调用方要求时跑
    ExecStopped,    ///< 停止：工站运行时跳过
    ExecLoadOnce,   ///< 加载方案后只跑一次
    ExecUnloadOnce, ///< 卸载方案时跑一次，常规运行跳过
    ExecWayMax      ///< 取值上界，不是有效方式
};

/** @brief JobCfg 或工站失败策略 */
enum FailPolicy {
    FailContinue = 0,    ///< NG 后继续执行后续工具/流程
    FailStopStation = 1, ///< NG 后停止本工站后续流程（缺省）
    FailStopAll = 2,     ///< NG 后置 haltAll，停止全部工站
    FailSkip = 3         ///< 跳过；执行层目前与 FailStopStation 同样处理
};

/** @brief 流程内工具组执行语义 */
enum GroupKind {
    GroupKindUndefined = -1, ///< 未定义
    GroupSequence = 0,       ///< 顺序：子节点依次执行
    GroupIf = 1,             ///< 条件：按来源判定选 THEN/ELSE 支
    GroupLoop = 2,           ///< 循环：按 loopCount 重复执行
    GroupParallel = 3,       ///< 并行：各支路都执行
    GroupSubJob = 4,         ///< 子流程：调用 subJobRef 指定的流程
    GroupKindMax             ///< 取值上界，不是有效语义
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
std::string execWayTitle(ExecWay way);
/** @brief 触发方式的 JSON 名（active/called/stopped/loadOnce/unloadOnce） */
VISIONCORE_API const char* execWayName(ExecWay way);
/** @brief 由 JSON 名解析触发方式；未知名按 ExecActive */
VISIONCORE_API ExecWay execWayFromName(const std::string& name);
/** @brief 旧方案 trigger 迁移：3 及非法值为主动执行，0~2 为被调用 */
VISIONCORE_API ExecWay execWayFromLegacyTrigger(int trigger);
/** @brief 失败策略的 JSON 名（continue/stopStation/stopAll/skip） */
VISIONCORE_API const char* failPolicyName(FailPolicy policy);
/** @brief 由 JSON 名解析失败策略；未知名按 FailStopStation */
VISIONCORE_API FailPolicy failPolicyFromName(const std::string& name);
/** @brief 旧方案整型迁移：1 跳过，2 继续，其余停工站 */
VISIONCORE_API FailPolicy failPolicyFromLegacy(int value);
/** @brief 旧方案整型迁移：0 顺序，1 并行，2 条件，3 循环，其余未定义 */
VISIONCORE_API GroupKind groupKindFromLegacy(int value);
/** @brief 组语义的 JSON 名；未定义返回空串 */
VISIONCORE_API const char* groupKindName(GroupKind kind);
/** @brief 由 JSON 名解析组语义；未知名为 GroupKindUndefined */
VISIONCORE_API GroupKind groupKindFromName(const std::string& name);
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
