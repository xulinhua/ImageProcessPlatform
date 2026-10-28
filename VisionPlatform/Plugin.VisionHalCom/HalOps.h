/**
 * @file HalOps.h
 * @brief 模板句柄和区域的共用操作
 */

#ifndef VISION_HAL_OPS_H
#define VISION_HAL_OPS_H

#include "VisionHalComExport.h"
#include "HalconCpp.h"

namespace Vision {

/** @brief 让 Halcon 按 UTF-8 解释路径。进程内只设一次 */
VISION_HALCOM_API void initFileNames();
/** @brief 元组里至少有一个有效模板号 */
VISION_HALCOM_API bool modelOk(const HalconCpp::HTuple& model);
/** @brief 释放形状模板并写成 -1 */
VISION_HALCOM_API void clearShapeModel(HalconCpp::HTuple& model);
/** @brief 释放 NCC 模板并写成 -1 */
VISION_HALCOM_API void clearNccModel(HalconCpp::HTuple& model);
/** @brief 区域有面积 */
VISION_HALCOM_API bool regionHasArea(const HalconCpp::HObject& region);
/** @brief 开运算再闭运算。半径不大于 0.4 的那一步跳过 */
VISION_HALCOM_API void morphOpenClose(const HalconCpp::HObject& bin, double openR, double closeR, HalconCpp::HObject*& out);

}

#endif
