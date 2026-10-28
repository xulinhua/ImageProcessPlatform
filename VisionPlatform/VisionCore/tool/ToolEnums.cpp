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

// 数据类型对应的键后缀
const char* dataSuffix(PortData d)
{
    switch (d) {
    case PortData::Image:   return "image";
    case PortData::Region:  return "region";
    case PortData::Contour: return "contour";
    case PortData::Roi:     return "roi";
    case PortData::Logic:   return "logic";
    case PortData::Pose:    return "pose";
    default:                return "image";
    }
}

// 由规范键后缀解析数据类型
PortData dataFromSuffix(const std::string& k)
{
    if (k.find("region") != std::string::npos) {
        return PortData::Region;
    }
    if (k.find("contour") != std::string::npos) {
        return PortData::Contour;
    }
    if (k.find("roi") != std::string::npos) {
        return PortData::Roi;
    }
    if (k.find("logic") != std::string::npos) {
        return PortData::Logic;
    }
    if (k.find("pose") != std::string::npos) {
        return PortData::Pose;
    }
    return PortData::Image;
}

}

// 端口槽位转 in.xxx / out.xxx
std::string portKey(const PortSlot& s)
{
    const char* dir = (s.dir == PortDir::In) ? "in" : "out";
    return std::string(dir) + "." + dataSuffix(s.data);
}

// 端口名规范；兼容旧 graphics/values
std::string canonicalizePortKey(const std::string& key)
{
    if (key == "in.graphics") {
        return "in.region";
    }
    if (key == "out.graphics") {
        return "out.region";
    }
    if (key == "in.values") {
        return "in.logic";
    }
    if (key == "out.values") {
        return "out.logic";
    }
    return key;
}

// 从键解析端口槽位
PortSlot portSlotFromKey(const std::string& key)
{
    const std::string k = canonicalizePortKey(key);
    PortDir dir = PortDir::In;
    if (k.size() >= 4 && k.compare(0, 4, "out.") == 0) {
        dir = PortDir::Out;
    }
    return PortSlot(dir, dataFromSuffix(k));
}

// 由端口名判断数据类型
PortData portDataFromKey(const std::string& key)
{
    return portSlotFromKey(key).data;
}

// 两端端口是否同一数据类型且方向正确
bool portsTypeMatch(const PortSlot& from, const PortSlot& to)
{
    return from.dir == PortDir::Out && to.dir == PortDir::In && from.data == to.data;
}

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
    default:                return Translate::tr("图像");
    }
}

}
