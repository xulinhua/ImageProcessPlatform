/**
 * @file CvObjectConvert.h
 * @brief CvObjectConvert头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CV_OBJECT_CONVERT_H
#define VISION_CV_OBJECT_CONVERT_H

#include "VisionCvComExport.h"

#include "ToolGeom.h"
#include <HsCommData/HsObject.h>
#include "opencvLib.h"
#include <vector>

namespace Vision {

/** @brief 掩膜是否无有效前景像素 */
VISION_CVCOM_API bool maskIsEmpty(const cv::Mat& mask);

/** @brief 整幅白掩膜 */
VISION_CVCOM_API cv::Mat fullImageMask(int imgW, int imgH);

/**
 * @brief 单格或整袋 HsObject 转掩膜（并集；圆/矩/线/轮廓/区域）
 * @note 图像格请用 CvImageConvert，本函数跳过 ObjImage
 */
VISION_CVCOM_API cv::Mat hsObjectToMask(const HsCommData::HsObject& obj, int imgW, int imgH);

/** @brief HsRegion 行程转掩膜 */
VISION_CVCOM_API cv::Mat hsRegionToMask(const HsCommData::HsRegion& reg, int imgW, int imgH);

/** @brief 多只上游区域对象并集；空则整幅图 */
VISION_CVCOM_API cv::Mat hsRegionsToMask(const std::vector<HsCommData::HsObject>& regs, int imgW, int imgH);

/** @brief 单只 HsObject 转掩膜；无效则整幅图 */
VISION_CVCOM_API cv::Mat toolRoiToMask(const HsCommData::HsObject& roi, int imgW, int imgH);

/** @brief 多 ROI 并集；空则整幅图 */
VISION_CVCOM_API cv::Mat toolRoisToMask(const std::vector<HsCommData::HsObject>& rois, int imgW, int imgH);

/** @brief 二值掩膜转 HsObject(ObjRegion) */
VISION_CVCOM_API HsCommData::HsObject maskToRegionObject(const cv::Mat& mask);

/** @brief 结果圆转 HsObject */
VISION_CVCOM_API HsCommData::HsObject circleObject(double cx, double cy, double r);

/** @brief 结果线段转 HsObject */
VISION_CVCOM_API HsCommData::HsObject lineObject(double x1, double y1, double x2, double y2);

}

#endif
