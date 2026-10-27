/**
 * @file RoiConv.h
 * @brief RoiConv头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ROICONV_H
#define VISION_ROICONV_H
#include "VisionCoreExport.h"
#include <HsCommData/Graphic.h>
#include <string>
#include <vector>
namespace Vision {
/** @brief 从 Drawing id（roi.N）解析行号；无效时 -1 */
VISIONCORE_API int roiIndexFromDrawingId(const std::string& id);
/** @brief 图形种类中文名 */
VISIONCORE_API std::string roiKindTitle(int kind);
/** @brief 一条 ROI 的可读明细（圆心半径、宽高等） */
VISIONCORE_API std::string formatRoiDetail(const HsCommData::HsObject& roi);
 /** @brief 明细文字在图上的锚点（图像坐标） */
VISIONCORE_API void roiLabelAnchor(const HsCommData::HsObject& roi, double* x, double* y);
}

#endif
