/**
 * @file HalImageConvert.h
 * @brief HalImageConvert头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HAL_IMAGE_CONVERT_H
#define VISION_HAL_IMAGE_CONVERT_H

#include "VisionHalComExport.h"

#include <HsCommData/HsImage.h>
#include "HalconCpp.h"
#include <string>

namespace Vision {

/** @brief Halcon 异常转说明字符串 */
VISION_HALCOM_API std::string hExceptionText(const HalconCpp::HException& e);

/**
 * @brief HsImage 转 HObject
 * byte 单通道用 GenImage1，byte 三/四通道用 GenImageInterleaved（Halcon 内部拷贝）
 * int16/uint16/int32/real32/complex 按对应 Halcon 类型；real64 收成 real
 */
VISION_HALCOM_API bool hsImageToHObject(const HsCommData::HsImage& img, HalconCpp::HObject*& out, std::string* err);

/**
 * @brief HObject 转 HsImage
 * 单通道按类型 memcpy；三通道用 InterleaveChannels 收成 rgb 再 memcpy
 */
VISION_HALCOM_API bool hObjectToHsImage(const HalconCpp::HObject& himg, int64_t frameId, HsCommData::HsImage*& out, std::string* err);

}

#endif
