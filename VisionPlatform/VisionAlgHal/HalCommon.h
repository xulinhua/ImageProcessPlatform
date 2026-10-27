/**
 * @file HalCommon.h
 * @brief HalCommon头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONCOMMON_H
#define VISION_HALCONCOMMON_H
/** @brief 兼容旧头：编排请用 HalAlgoHelpers.h；形态学请用 HalOps.h */
#include "HalAlgoHelpers.h"
#include "HalOps.h"

namespace Vision {
/** @deprecated 请用 morphOpenClose */
inline void morphRegion(const HalconCpp::HObject& bin, double openR, double closeR, HalconCpp::HObject*& out)
{
    morphOpenClose(bin, openR, closeR, out);
}
}
#endif
