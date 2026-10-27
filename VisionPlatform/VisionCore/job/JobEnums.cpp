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

// 触发方式显示名
std::string triggerTitle(TriggerMode t)
{
    switch (t) {
    case TriggerSoft:        return Translate::tr("软件触发");
    case TriggerHardwareIo:  return Translate::tr("硬件触发");
    case TriggerUpstreamJob: return Translate::tr("上游完成");
    case TriggerLoop:        return Translate::tr("循环");
    default:                 return Translate::tr("软件触发");
    }
}

// 失败策略显示名
std::string failPolicyTitle(FailPolicy p)
{
    switch (p) {
    case FailStopStation: return Translate::tr("停工站");
    case FailSkip:        return Translate::tr("跳过");
    case FailNgContinue:  return Translate::tr("NG 时继续");
    default:              return Translate::tr("停工站");
    }
}

// 工具组语义显示名
std::string groupKindTitle(GroupKind k)
{
    switch (k) {
    case GroupSequence: return Translate::tr("顺序");
    case GroupParallel: return Translate::tr("并行");
    case GroupIf:       return Translate::tr("条件");
    case GroupLoop:     return Translate::tr("循环");
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
