/**
 * @file ToolEnums.cpp
 * @brief ToolEnums实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEnums.h"
#include "Translate.h"

namespace Vision {

// 转存盘整数
int toolTypeToInt(ToolType t)
{
    return static_cast<int>(t);
}

// 工具类型种数
int toolTypeCount()
{
    return static_cast<int>(ToolType::Max);
}

// 整数转工具类型
ToolType toolTypeFromInt(int v)
{
    if (v < 0 || v >= toolTypeCount()) {
        return ToolType::Undefined;
    }
    return static_cast<ToolType>(v);
}

// 工具库展示顺序
ToolType toolTypeDisplayAt(int i)
{
    static const ToolType kOrder[] = {
        ToolType::Acquire,
        ToolType::Preprocess,
        ToolType::Locate,
        ToolType::Detect,
        ToolType::RstProcess,
        ToolType::Calib,
        ToolType::Logic,
        ToolType::Comm
    };
    const int n = static_cast<int>(sizeof(kOrder) / sizeof(kOrder[0]));
    if (i < 0 || i >= n) {
        return ToolType::Acquire;
    }
    return kOrder[i];
}

// 工具类型界面显示名
std::string toolTypeTitle(ToolType t)
{
    switch (t) {
    case ToolType::Acquire:        return Translate::tr("采集");
    case ToolType::Preprocess:    return Translate::tr("预处理");
    case ToolType::Locate:        return Translate::tr("定位");
    case ToolType::Detect:        return Translate::tr("检测");
    case ToolType::Logic:         return Translate::tr("逻辑");
    case ToolType::Comm:          return Translate::tr("通信");
    case ToolType::RstProcess: return Translate::tr("结果处理");
    case ToolType::Calib:         return Translate::tr("标定");
    default:                      return Translate::tr("采集");
    }
}

// 端口数据类型界面显示名
std::string portDataTitle(PortData d)
{
    switch (d) {
    case PortData::Image:   return Translate::tr("图像");
    case PortData::Region:  return Translate::tr("区域");
    case PortData::Contour: return Translate::tr("轮廓");
    case PortData::Roi:     return Translate::tr("ROI");
    case PortData::Logic:   return Translate::tr("逻辑");
    case PortData::Pose:    return Translate::tr("位姿");
    case PortData::Tuple:   return Translate::tr("元组");
    default:                return Translate::tr("图像");
    }
}

}
