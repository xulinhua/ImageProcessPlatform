/**
 * @file ToolInstanceData.h
 * @brief ToolInstanceData头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOL_INSTANCE_DATA_H
#define VISION_TOOL_INSTANCE_DATA_H

#include "ObjectId.h"
#include "ToolEnums.h"
#include "ToolGeom.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 连线来源：自动补边或用户手连 */
enum ToolLinkOrigin {
    LinkOriginAuto = 0,   ///< 按执行树等自动生成
    LinkOriginManual = 1  ///< 用户拖线或改过
};

/**
 * @class ToolInputLink
 * @brief 工具持有的输入连线（只存两端；权威存盘）
 */
struct ToolInputLink {
    ObjectId fromToolId; ///< 上游工具
    PortSlot fromPort;   ///< 上游输出口
    PortSlot toPort;     ///< 本工具输入口
    int index;          ///< 上游该口的第几个；-1 表示整口
    RegionUse use;       ///< 区域用途，只对 Region / Roi 有意义
    ToolLinkOrigin origin; ///< ToolLinkOrigin
    /** @brief 缺省为图像口到图像口、整口、正常用途、用户手连 */
    ToolInputLink()
        : fromPort(portSlotOut(PortData::Image))
        , toPort(portSlotIn(PortData::Image))
        , index(-1)
        , use(RegionUse::Normal)
        , origin(LinkOriginManual)
    {}
};

/**
 * @class ToolInfo
 * @brief 工具身份（写入方案）
 */
struct ToolInfo {
    ObjectId id;          ///< 实例 id
    std::string toolName; ///< 工具种类名
    ToolType type;        ///< 工具大类
    std::string name;     ///< 显示名
    bool enabled;         ///< 是否启用
    std::string toolVersion; ///< 方案里记录的工具版本，参数适配据此判断升降；为空时存盘改取原型 version()
    ObjectId compositeId;     ///< 组合定义 id。普通工具为空
    int compositeScope;       ///< CompositeScope。普通工具为 0
    /** @brief 大类未定义、启用、非组合工具 */
    ToolInfo()
        : type(ToolType::Undefined)
        , enabled(true)
        , compositeScope(0)
    {}
};



}

#endif
