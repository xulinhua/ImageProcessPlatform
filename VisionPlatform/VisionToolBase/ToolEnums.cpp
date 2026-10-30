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

namespace {
const ToolType kDisplayOrder[] = {
    ToolType::Acquire,
    ToolType::Preprocess,
    ToolType::Locate,
    ToolType::Detect,
    ToolType::Identify,
    ToolType::RstProcess,
    ToolType::Calib,
    ToolType::Logic,
    ToolType::Comm,
    ToolType::Acquire3D,
    ToolType::Prepare3D,
    ToolType::Locate3D,
    ToolType::Measure3D,
    ToolType::Detect3D,
    ToolType::Calib3D,
    ToolType::Infer
};
}

int toolTypeToInt(ToolType t)
{
    return static_cast<int>(t);
}

int toolTypeCount()
{
    return static_cast<int>(ToolType::Max);
}

ToolType toolTypeFromInt(int v)
{
    if (v < 0 || v >= toolTypeCount()) {
        return ToolType::Undefined;
    }
    return static_cast<ToolType>(v);
}

ToolType toolTypeDisplayAt(int i)
{
    const int n = static_cast<int>(sizeof(kDisplayOrder) / sizeof(kDisplayOrder[0]));
    if (i < 0 || i >= n) {
        return ToolType::Acquire;
    }
    return kDisplayOrder[i];
}

int toolTypeDisplayCount()
{
    return static_cast<int>(sizeof(kDisplayOrder) / sizeof(kDisplayOrder[0]));
}

std::string toolTypeTitle(ToolType t)
{
    switch (t) {
    case ToolType::Acquire:     return Translate::tr("采集");
    case ToolType::Preprocess:  return Translate::tr("预处理");
    case ToolType::Locate:      return Translate::tr("定位");
    case ToolType::Detect:      return Translate::tr("检测");
    case ToolType::Logic:       return Translate::tr("逻辑");
    case ToolType::Comm:        return Translate::tr("通信");
    case ToolType::RstProcess:  return Translate::tr("结果处理");
    case ToolType::Calib:       return Translate::tr("标定");
    case ToolType::Acquire3D:   return Translate::tr("三维采集");
    case ToolType::Prepare3D:   return Translate::tr("点云预处理");
    case ToolType::Locate3D:    return Translate::tr("三维定位");
    case ToolType::Measure3D:   return Translate::tr("三维测量");
    case ToolType::Detect3D:    return Translate::tr("三维检测");
    case ToolType::Calib3D:     return Translate::tr("三维标定");
    case ToolType::Infer:       return Translate::tr("深度学习");
    case ToolType::Identify:    return Translate::tr("识别");
    default:                    return Translate::tr("采集");
    }
}

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
    case PortData::Cloud:   return Translate::tr("点云");
    case PortData::Pose3:   return Translate::tr("三维位姿");
    case PortData::Shape3:  return Translate::tr("三维形状");
    default:                return Translate::tr("图像");
    }
}

}
