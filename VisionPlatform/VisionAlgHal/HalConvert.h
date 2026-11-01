/**
 * @file HalConvert.h
 * @brief Halcon 图像转换兼容头（转发 HalImageConvert）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONCONVERT_H
#define VISION_HALCONCONVERT_H
/** @brief 兼容旧头：请改用 HalImageConvert.h */
#include "HalImageConvert.h"

namespace Vision {
/** @deprecated 请用 hsImageToHObject，彩色再 Rgb1ToGray */
    bool frameToHImageGray(const HsCommData::HsImage& fb, HalconCpp::HImage*& out, std::string* err);

/** @deprecated 请用 hObjectToHsImage */
inline bool hImageToFrame(const HalconCpp::HImage& img, int64_t frameId, HsCommData::HsImage*& out, std::string* err)
{
    return hObjectToHsImage(img, frameId, out, err);
}
}
#endif
