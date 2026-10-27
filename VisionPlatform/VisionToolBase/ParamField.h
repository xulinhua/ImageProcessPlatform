/**
 * @file ParamField.h
 * @brief ParamField头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PARAMFIELD_H
#define VISION_PARAMFIELD_H

#include "ToolExport.h"
#include "ParamValue.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 参数页控件种类 */
enum class ParamFieldType {
    Undefined = -1, ///< 未定义
    Bool = 0, ///< 勾选
    Int,      ///< 整数
    Double,   ///< 浮点
    String,   ///< 文本；有选项则为下拉
    Path,     ///< 路径，文本加浏览
    Device,    ///< 当前方案里的设备
    Max ///< 个数，不是合法值
};

/** @class ParamField @brief 工具壳上的一项参数说明，界面据此生成表单 */
struct ParamField {
    std::string key;                    ///< 写入 ParamMap 的键
    std::string title;                  ///< 界面文字，作翻译键
    ParamFieldType type;                ///< 控件种类
    ParamValue def;                     ///< 新拖进流程时的初值
    bool hasMin;                        ///< 是否限制最小
    bool hasMax;                        ///< 是否限制最大
    double minValue;                    ///< 最小
    double maxValue;                    ///< 最大
    std::vector<std::string> choices;   ///< 字符串枚举；空表示自由文本
    ParamField()
        : type(ParamFieldType::Undefined)
        , hasMin(false)
        , hasMax(false)
        , minValue(0)
        , maxValue(0)
    {}
};

/** @brief 拼下拉选项；空指针跳过 */
    VISION_TOOL_API std::vector<std::string> paramChoices(const char* a, const char* b = 0, const char* c = 0, const char* d = 0);


/** @brief 布尔项 */
    VISION_TOOL_API ParamField paramBool(const char* key, const char* title, bool def);


/** @brief 整数项 */
    VISION_TOOL_API ParamField paramInt(const char* key, const char* title, int def, bool hasMin, double minV,
                           bool hasMax, double maxV);


/** @brief 浮点项 */
    VISION_TOOL_API ParamField paramDouble(const char* key, const char* title, double def, bool hasMin, double minV,
                              bool hasMax, double maxV);


/** @brief 字符串项；choices 非空则为下拉 */
    VISION_TOOL_API ParamField paramString(const char* key, const char* title, const std::string& def,
                              const std::vector<std::string>& choices);


/** @brief 路径项 */
    VISION_TOOL_API ParamField paramPath(const char* key, const char* title, const std::string& def);


/** @brief 设备项；界面写 ToolCfg.deviceId，不进 params */
    VISION_TOOL_API ParamField paramDevice(const char* key, const char* title);


}

#endif
