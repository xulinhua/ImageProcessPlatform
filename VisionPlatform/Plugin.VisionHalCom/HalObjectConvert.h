/**
 * @file HalObjectConvert.h
 * @brief HalObjectConvert头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HAL_OBJECT_CONVERT_H
#define VISION_HAL_OBJECT_CONVERT_H

#include "VisionHalComExport.h"

#include "ToolGeom.h"
#include <HsCommData/HsObject.h>
#include "HalconCpp.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 区域是否无有效面积 */
VISION_HALCOM_API bool hoIsEmpty(const HalconCpp::HObject& r);

/** @brief 整幅矩形区域（Halcon 行/列闭区间） */
VISION_HALCOM_API HalconCpp::HObject fullImageRegion(int imgW, int imgH);

/**
 * @brief 单格或整袋 HsObject 转 HObject（区域并集；圆/矩/线/轮廓等生成对应 region）
 * @note 图像格请用 HalImageConvert，本函数跳过 ObjImage
 */
VISION_HALCOM_API HalconCpp::HObject hsObjectToHObject(const HsCommData::HsObject& obj, int imgW, int imgH);

/** @brief HsRegion 行程 → HObject */
VISION_HALCOM_API HalconCpp::HObject hsRegionToHObject(const HsCommData::HsRegion& reg);

/** @brief 多只上游区域对象并集；空则整幅图 */
VISION_HALCOM_API HalconCpp::HObject hsRegionsToHObject(const std::vector<HsCommData::HsObject>& regs, int imgW, int imgH);

/** @brief 单只 HsObject → 区域；无效则整幅图 */
VISION_HALCOM_API HalconCpp::HObject toolRoiToHObject(const HsCommData::HsObject& roi, int imgW, int imgH);

/** @brief 多 ROI 并集；空则整幅图 */
VISION_HALCOM_API HalconCpp::HObject toolRoisToHObject(const std::vector<HsCommData::HsObject>& rois, int imgW, int imgH);

/** @brief HObject(Region) → HsObject(ObjRegion) */
VISION_HALCOM_API HsCommData::HsObject hObjectToRegionObject(const HalconCpp::HObject& r);

/** @brief 结果圆 → HsObject */
VISION_HALCOM_API HsCommData::HsObject circleObject(double cx, double cy, double r);

/** @brief 结果线段 → HsObject */
VISION_HALCOM_API HsCommData::HsObject lineObject(double x1, double y1, double x2, double y2);

/** @brief 匹配点十字（两段线段）→ HsObject */
VISION_HALCOM_API HsCommData::HsObject crossObject(double cx, double cy);

}

#endif
