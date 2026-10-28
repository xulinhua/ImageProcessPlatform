/**
 * @file JobEnums.cpp
 * @brief JobEnums实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JobEnums.h"
#include "Translate.h"

namespace Vision {

// 执行方式
std::string execWayTitle(ExecWay way)
{
    switch (way) {
    case ExecCalled:     return Translate::tr("被调用");
    case ExecStopped:    return Translate::tr("停止");
    case ExecLoadOnce:   return Translate::tr("加载时一次");
    case ExecUnloadOnce: return Translate::tr("卸载时一次");
    case ExecActive:
    default:             return Translate::tr("主动执行");
    }
}

const char* execWayName(ExecWay way)
{
    switch (way) {
    case ExecCalled:     return "called";
    case ExecStopped:    return "stopped";
    case ExecLoadOnce:   return "loadOnce";
    case ExecUnloadOnce: return "unloadOnce";
    case ExecActive:
    default:             return "active";
    }
}

ExecWay execWayFromName(const std::string& name)
{
    if (name == "called") return ExecCalled;
    if (name == "stopped") return ExecStopped;
    if (name == "loadOnce") return ExecLoadOnce;
    if (name == "unloadOnce") return ExecUnloadOnce;
    return ExecActive;
}

ExecWay execWayFromLegacyTrigger(int trigger)
{
    if (trigger == 3) {
        return ExecActive;
    }
    if (trigger >= 0 && trigger <= 2) {
        return ExecCalled;
    }
    return ExecActive;
}

const char* failPolicyName(FailPolicy policy)
{
    switch (policy) {
    case FailContinue: return "continue";
    case FailStopAll:  return "stopAll";
    case FailSkip:     return "skip";
    case FailStopStation:
    default:           return "stopStation";
    }
}

FailPolicy failPolicyFromName(const std::string& name)
{
    if (name == "continue") return FailContinue;
    if (name == "stopAll") return FailStopAll;
    if (name == "skip") return FailSkip;
    return FailStopStation;
}

FailPolicy failPolicyFromLegacy(int value)
{
    if (value == 1) return FailSkip;
    if (value == 2) return FailContinue;
    return FailStopStation;
}

GroupKind groupKindFromLegacy(int value)
{
    if (value == 1) return GroupParallel;
    if (value == 2) return GroupIf;
    if (value == 3) return GroupLoop;
    if (value == 0) return GroupSequence;
    return GroupKindUndefined;
}

const char* groupKindName(GroupKind kind)
{
    switch (kind) {
    case GroupIf:       return "if";
    case GroupLoop:     return "loop";
    case GroupParallel: return "parallel";
    case GroupSubJob:   return "subJob";
    case GroupSequence: return "sequence";
    default:            return "";
    }
}

GroupKind groupKindFromName(const std::string& name)
{
    if (name == "sequence") return GroupSequence;
    if (name == "if") return GroupIf;
    if (name == "loop") return GroupLoop;
    if (name == "parallel") return GroupParallel;
    if (name == "subJob") return GroupSubJob;
    return GroupKindUndefined;
}

// 失败策略显示名
std::string failPolicyTitle(FailPolicy p)
{
    switch (p) {
    case FailContinue:    return Translate::tr("继续");
    case FailStopStation: return Translate::tr("停工站");
    case FailStopAll:     return Translate::tr("停全部");
    case FailSkip:        return Translate::tr("跳过");
    default:              return Translate::tr("停工站");
    }
}
// 工具组语义显示名
std::string groupKindTitle(GroupKind k)
{
    switch (k) {
    case GroupSequence: return Translate::tr("顺序");
    case GroupIf:       return Translate::tr("条件");
    case GroupLoop:     return Translate::tr("循环");
    case GroupParallel: return Translate::tr("并行");
    case GroupSubJob:   return Translate::tr("子流程");
    default:            return Translate::tr("顺序");
    }
}

// 从整型解析组语义
GroupKind groupKindFromInt(int v)
{
    if (v < 0 || v >= static_cast<int>(GroupKindMax)) {
        return GroupKindUndefined;
    }
    return static_cast<GroupKind>(v);
}

// 结果规则三态显示名
std::string resultTermStateTitle(RstTermState s)
{
    switch (s) {
    case RstTermIgnore:    return Translate::tr("忽略");
    case RstTermRequireOk: return Translate::tr("要 OK");
    case RstTermRequireNg: return Translate::tr("要 NG");
    default:                  return Translate::tr("忽略");
    }
}

// 结果规则来源显示名
std::string resultTermKindTitle(RstTermKind k)
{
    switch (k) {
    case RstTermTool: return Translate::tr("工具");
    case RstTermRule: return Translate::tr("规则");
    default:             return Translate::tr("工具");
    }
}

}
