/**
 * @file ToolBase.cpp
 * @brief ToolBase实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolBase.h"
#include "VisionVersion.h"

namespace Vision {

// 默认工具版本
std::string ToolBase::version() const
{
    return std::string("1.0.0");
}

// 默认能力：采集/逻辑/通讯不画 ROI
ToolCaps ToolBase::capabilities() const
{
    ToolCaps c;
    const ToolType t = type();
    if (t == ToolType::Acquire || t == ToolType::Logic || t == ToolType::Comm) {
        c.usesRoi = false;
    }
    return c;
}

}
