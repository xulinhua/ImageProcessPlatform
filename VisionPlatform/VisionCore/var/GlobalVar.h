/**
 * @file GlobalVar.h
 * @brief 方案级全局变量定义
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_GLOBALVAR_H
#define VISION_GLOBALVAR_H
#include "VisionCoreExport.h"

#include "ParamValue.h"
#include <string>

namespace Vision {

/** @brief 全局变量类型（存盘为 0..3） */
enum GlobalVarType {
    GlobalVarInt = 0,    ///< 整数
    GlobalVarFloat = 1,  ///< 浮点
    GlobalVarBool = 2,   ///< 布尔
    GlobalVarString = 3  ///< 文本
};

/**
 * @class GlobalVar
 * @brief 一条全局变量：名称、类型、当前值、注释
 */
struct VISIONCORE_API GlobalVar {
    std::string name;    ///< 变量名（方案内唯一）
    GlobalVarType type;  ///< 类型
    ParamValue value;    ///< 当前值
    std::string comment; ///< 注释
    /** @brief 整数类型、值为 0 */
    GlobalVar()
        : type(GlobalVarInt)
        , value(0)
    {}
};

}

#endif
