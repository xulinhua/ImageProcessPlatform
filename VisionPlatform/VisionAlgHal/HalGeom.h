/**
 * @file HalGeom.h
 * @brief Halcon 几何转换兼容头（转发 HalObjectConvert）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONGEOM_H
#define VISION_HALCONGEOM_H
/** @brief 兼容旧头：几何转换请用 HalObjectConvert.h */
#include "HalObjectConvert.h"
#include "HalImageConvert.h"

namespace Vision {
/** @deprecated 请用 toolRoiToHObject */
inline HalconCpp::HObject roiToRegion(const HsCommData::HsObject& roi, int imgW, int imgH)
{
    return toolRoiToHObject(roi, imgW, imgH);
}
/** @deprecated 请用 toolRoisToHObject */
inline HalconCpp::HObject roisToRegion(const std::vector<HsCommData::HsObject>& rois, int imgW, int imgH)
{
    return toolRoisToHObject(rois, imgW, imgH);
}
/** @deprecated 请用 hsRegionsToHObject */
inline HalconCpp::HObject rleRegionsToHObject(const std::vector<HsCommData::HsObject>& regs, int imgW, int imgH)
{
    return hsRegionsToHObject(regs, imgW, imgH);
}
/** @deprecated 请用 hObjectToRegionObject */
inline HsCommData::HsObject regionObject(const HalconCpp::HObject& r)
{
    return hObjectToRegionObject(r);
}
}
#endif
