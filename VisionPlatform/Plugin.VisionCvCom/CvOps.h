/**
 * @file CvOps.h
 * @brief CvOps头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CV_OPS_H
#define VISION_CV_OPS_H

#include "VisionCvComExport.h"

#include "opencvLib.h"

namespace Vision {

/**
 * @brief 掩膜开闭运算（椭圆核）。空掩膜直接返回。openR/closeR ≤0.4 跳过对应步
 */
VISION_CVCOM_API void morphOpenClose(const cv::Mat& bin, double openR, double closeR, cv::Mat*& out);

}

#endif
