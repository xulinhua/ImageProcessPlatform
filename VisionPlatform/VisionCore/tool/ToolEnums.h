/**
 * @file ToolEnums.h
 * @brief ToolEnums头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOL_ENUMS_H
#define VISION_TOOL_ENUMS_H

#include "PortTypes.h"
#include "ToolExport.h"
#include <string>

namespace Vision {

/** @brief 工具箱分组（类型插件大类，不是具体工具实例） */
enum class ToolType {
    Undefined = -1, ///< 未定义
    Acquire = 0,     ///< 采集
    Preprocess,     ///< 预处理
    Locate,         ///< 定位
    Detect,         ///< 检测
    Logic,          ///< 逻辑
    Comm,           ///< 通信
    RstProcess,  ///< 结果处理
    Calib,           ///< 标定
    Acquire3D,      ///< 三维采集。点云、三维位姿、三维形状
    Prepare3D,      ///< 点云预处理
    Measure3D,      ///< 三维测量
    Align3D,        ///< 三维配准
    Infer,          ///< 深度学习推理
    Max ///< 个数，不是合法值
};

/** @brief 转存盘/下标用的整数 */
VISION_TOOL_API int toolTypeToInt(ToolType t);
/** @brief 工具类型种数 */
VISION_TOOL_API int toolTypeCount();
/** @brief 整数转类型；越界回采集 */
VISION_TOOL_API ToolType toolTypeFromInt(int v);
/** @brief 工具库展示顺序 */
VISION_TOOL_API ToolType toolTypeDisplayAt(int i);
/** @brief 工具库展示顺序的长度。空大类不在里面 */
VISION_TOOL_API int toolTypeDisplayCount();
/** @brief 工具类型界面显示名 */
VISION_TOOL_API std::string toolTypeTitle(ToolType t);
/** @brief 端口数据类型界面显示名 */
VISION_TOOL_API std::string portDataTitle(PortData d);

/** @brief 底图候选：采集、预处理和标定（有独立输出图像） */
inline bool toolTypeIsImageSource(ToolType t)
{
    return t == ToolType::Acquire || t == ToolType::Preprocess || t == ToolType::Calib;
}

/** @brief 三维采集。不走图像相机的设备路径和帧号 */
inline bool toolTypeIsAcquire3D(ToolType t)
{
    return t == ToolType::Acquire3D;
}

/** @brief 叠加候选：定位、检测、结果处理 */
inline bool toolTypeIsOverlaySource(ToolType t)
{
    return t == ToolType::Locate || t == ToolType::Detect || t == ToolType::RstProcess
        || t == ToolType::Infer;
}

/** @brief 工具类型是否参与 OK/NG 结果矩阵列 */
inline bool toolTypeProducesOkNg(ToolType t)
{
    return t == ToolType::Locate || t == ToolType::Detect
        || t == ToolType::RstProcess || t == ToolType::Logic
        || t == ToolType::Measure3D || t == ToolType::Align3D
        || t == ToolType::Infer;
}

}

#endif
