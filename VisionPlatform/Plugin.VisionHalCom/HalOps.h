/**
 * @file HalOps.h
 * @brief HalOps头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HAL_OPS_H
#define VISION_HAL_OPS_H

#include "VisionHalComExport.h"

#include "HalconCpp.h"

namespace Vision {

/**
 * @brief 区域开闭运算。空域直接返回。openR/closeR 为圆半径，≤0.4 跳过对应步
 */
VISION_HALCOM_API void morphOpenClose(const HalconCpp::HObject& bin, double openR, double closeR,
                                      HalconCpp::HObject*& out);

/**
 * @brief 在矩形测量区域内拟合直线（edges_sub_pix + fit_line_contour_xld 薄封装）
 * @param image 输入图（已 ReduceDomain 亦可）
 * @param row1,col1,row2,col2 测量矩形对角（Halcon 行列）
 * @param sigma,low,high 边缘参数
 * @param outRow1..outCol2 输出线段端点；失败返回 false
 */
VISION_HALCOM_API bool measureLineRect(const HalconCpp::HImage& image,
                                       double row1, double col1, double row2, double col2,
                                       double sigma, double low, double high,
                                       double*& outRow1, double*& outCol1, double*& outRow2, double*& outCol2);

}

#endif
