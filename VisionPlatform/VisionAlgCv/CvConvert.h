/**
 * @file CvConvert.h
 * @brief CvConvert头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVCONVERT_H
#define VISION_OPENCVCONVERT_H
/** @brief 兼容旧头：请改用 CvImageConvert.h */
#include "CvImageConvert.h"

namespace Vision {
/** @deprecated 请用 hsImageToMatGray */
inline bool frameToMatGray(const HsCommData::HsImage& fb, cv::Mat*& out, std::string* err)
{
    return hsImageToMatGray(fb, out, err);
}
/** @deprecated 请用 matToHsImage */
inline bool matToFrame(const cv::Mat& img, int64_t frameId, HsCommData::HsImage*& out, std::string* err)
{
    return matToHsImage(img, frameId, out, err);
}
}
#endif
