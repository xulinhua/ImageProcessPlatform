/**
 * @file HalTupleConvert.h
 * @brief HalTupleConvert头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HAL_TUPLE_CONVERT_H
#define VISION_HAL_TUPLE_CONVERT_H

#include "VisionHalComExport.h"

#include <HsCommData/HsTuple.h>
#include "HalconCpp.h"
#include <string>

namespace Vision {

/**
 * @brief HsTuple → HTuple。混合元组按格类型追加；空元组得空 HTuple
 */
VISION_HALCOM_API bool hsTupleToHTuple(const HsCommData::HsTuple& src, HalconCpp::HTuple*& out, std::string* err);

/**
 * @brief HTuple → HsTuple。按 Halcon 元素类型写入 long/double/string
 */
VISION_HALCOM_API bool hTupleToHsTuple(const HalconCpp::HTuple& src, HsCommData::HsTuple*& out, std::string* err);

}

#endif
