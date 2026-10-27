/**
 * @file ParamField.cpp
 * @brief ParamField实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ParamField.h"

namespace Vision {

// 拼下拉选项；空指针跳过
std::vector<std::string> paramChoices(const char* a, const char* b, const char* c, const char* d)
{
    std::vector<std::string> v;
    if (a) {
        v.push_back(a);
    }
    if (b) {
        v.push_back(b);
    }
    if (c) {
        v.push_back(c);
    }
    if (d) {
        v.push_back(d);
    }
    return v;
}

// 布尔项
ParamField paramBool(const char* key, const char* title, bool def)
{
    ParamField f;
    f.key = key;
    f.title = title;
    f.type = ParamFieldType::Bool;
    f.def = def;
    return f;
}

// 整数项
ParamField paramInt(const char* key, const char* title, int def, bool hasMin, double minV,
                           bool hasMax, double maxV)
{
    ParamField f;
    f.key = key;
    f.title = title;
    f.type = ParamFieldType::Int;
    f.def = def;
    f.hasMin = hasMin;
    f.hasMax = hasMax;
    f.minValue = minV;
    f.maxValue = maxV;
    return f;
}

// 浮点项
ParamField paramDouble(const char* key, const char* title, double def, bool hasMin, double minV,
                              bool hasMax, double maxV)
{
    ParamField f;
    f.key = key;
    f.title = title;
    f.type = ParamFieldType::Double;
    f.def = def;
    f.hasMin = hasMin;
    f.hasMax = hasMax;
    f.minValue = minV;
    f.maxValue = maxV;
    return f;
}

// 字符串项；choices 非空则为下拉
ParamField paramString(const char* key, const char* title, const std::string& def,
                              const std::vector<std::string>& choices)
{
    ParamField f;
    f.key = key;
    f.title = title;
    f.type = ParamFieldType::String;
    f.def = def;
    f.choices = choices;
    return f;
}

// 路径项
ParamField paramPath(const char* key, const char* title, const std::string& def)
{
    ParamField f;
    f.key = key;
    f.title = title;
    f.type = ParamFieldType::Path;
    f.def = def;
    return f;
}

// 设备项；界面写 ToolCfg.deviceId，不进 params
ParamField paramDevice(const char* key, const char* title)
{
    ParamField f;
    f.key = key;
    f.title = title;
    f.type = ParamFieldType::Device;
    return f;
}

}
