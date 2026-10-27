/**
 * @file CvImageConvert.h
 * @brief CvImageConvert头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CV_IMAGE_CONVERT_H
#define VISION_CV_IMAGE_CONVERT_H

#include "VisionCvComExport.h"

#include <HsCommData/HsImage.h>
#include "opencvLib.h"
#include <string>

namespace Vision {

/**
 * @brief HsImage 转灰度 Mat（彩色按亮度折灰后拷贝）
 * @param img 输入图
 * @param out 输出，不可空
 * @param err 失败原因，可空
 */
VISION_CVCOM_API bool hsImageToMatGray(const HsCommData::HsImage& img, cv::Mat*& out, std::string* err);

/**
 * @brief HsImage 转 Mat。单通道→CV_8UC1；三/四通道交错 RGB/BGR→CV_8UC3（BGR）
 */
VISION_CVCOM_API bool hsImageToMat(const HsCommData::HsImage& img, cv::Mat*& out, std::string* err);

/**
 * @brief Mat 转 HsImage。1 通道→mono8；3 通道 BGR→rgb8 交错
 */
VISION_CVCOM_API bool matToHsImage(const cv::Mat& img, int64_t frameId, HsCommData::HsImage*& out, std::string* err);

}

#endif
