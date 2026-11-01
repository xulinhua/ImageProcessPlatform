/**
 * @file CvGeom.h
 * @brief OpenCV 几何转换兼容头（转发 CvObjectConvert）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVGEOM_H
#define VISION_OPENCVGEOM_H
/** @brief 兼容旧头：几何转换请用 CvObjectConvert.h；编排辅助请用 CvAlgoHelpers.h */
#include "CvObjectConvert.h"
#include "CvAlgoHelpers.h"

namespace Vision {
/** @deprecated 请用 toolRoiToMask */
inline cv::Mat roiToMask(const HsCommData::HsObject& roi, int imgW, int imgH)
{
    return toolRoiToMask(roi, imgW, imgH);
}
/** @deprecated 请用 toolRoisToMask */
inline cv::Mat roisToMask(const std::vector<HsCommData::HsObject>& rois, int imgW, int imgH)
{
    return toolRoisToMask(rois, imgW, imgH);
}
/** @deprecated 请用 hsRegionsToMask */
inline cv::Mat rleRegionsToMask(const std::vector<HsCommData::HsObject>& regs, int imgW, int imgH)
{
    return hsRegionsToMask(regs, imgW, imgH);
}
/** @deprecated 请用 maskToRegionObject */
inline HsCommData::HsObject regionObjectFromMask(const cv::Mat& mask)
{
    return maskToRegionObject(mask);
}
}
#endif
