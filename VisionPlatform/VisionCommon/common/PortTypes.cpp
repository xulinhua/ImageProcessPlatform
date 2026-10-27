/**
 * @file PortTypes.cpp
 * @brief PortTypes实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PortTypes.h"

namespace Vision {

// 整数还原区域用途；越界当正常区域
RegionUse regionUseFromInt(int v)
{
    if (v == static_cast<int>(RegionUse::Mask)) {
        return RegionUse::Mask;
    }
    if (v == static_cast<int>(RegionUse::Invalid)) {
        return RegionUse::Invalid;
    }
    return RegionUse::Normal;
}

// 数据类型对应的键后缀
const char* portDataSuffix(PortData d)
{
    switch (d) {
    case PortData::Image:   return "image";
    case PortData::Region:  return "region";
    case PortData::Contour: return "contour";
    case PortData::Roi:     return "roi";
    case PortData::Logic:   return "logic";
    case PortData::Pose:    return "pose";
    case PortData::Tuple:   return "tuple";
    default:                return "image";
    }
}

// 端口槽位转 in.xxx / out.xxx；有口名时再加一段
std::string portKey(const PortSlot& s)
{
    const char* dir = (s.dir == PortDir::In) ? "in" : "out";
    std::string k = std::string(dir) + "." + portDataSuffix(s.data);
    if (!s.name.empty()) {
        k += ".";
        k += s.name;
    }
    return k;
}

// 端口名规范成 in.xxx、out.xxx；兼容旧 graphics、values
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

// 由类型后缀解析数据类型
PortData portDataFromSuffix(const std::string& k)
{
    if (k == "region") {
        return PortData::Region;
    }
    if (k == "contour") {
        return PortData::Contour;
    }
    if (k == "roi") {
        return PortData::Roi;
    }
    if (k == "logic") {
        return PortData::Logic;
    }
    if (k == "pose") {
        return PortData::Pose;
    }
    if (k == "tuple") {
        return PortData::Tuple;
    }
    return PortData::Image;
}

// 从键解析端口槽位。in.image.locate 的口名是 locate
PortSlot portSlotFromKey(const std::string& key)
{
    const std::string k = canonicalizePortKey(key);
    PortDir dir = PortDir::In;
    std::string rest = k;
    if (k.size() >= 4 && k.compare(0, 4, "out.") == 0) {
        dir = PortDir::Out;
        rest = k.substr(4);
    } else if (k.size() >= 3 && k.compare(0, 3, "in.") == 0) {
        rest = k.substr(3);
    }
    std::string suffix = rest;
    std::string name;
    const std::string::size_type dot = rest.find('.');
    if (dot != std::string::npos) {
        suffix = rest.substr(0, dot);
        name = rest.substr(dot + 1);
    }
    PortSlot s(dir, portDataFromSuffix(suffix));
    s.name = name;
    return s;
}

// 由存盘的方向与数据类型整数还原端口槽位
PortSlot portSlotFromInts(int dir, int data)
{
    PortDir d = (dir == static_cast<int>(PortDir::In)) ? PortDir::In : PortDir::Out;
    if (data < 0 || data >= static_cast<int>(PortData::Max)) {
        data = 0;
    }
    return PortSlot(d, static_cast<PortData>(data));
}

}
