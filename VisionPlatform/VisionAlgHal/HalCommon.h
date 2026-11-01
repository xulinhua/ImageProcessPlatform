/**
 * @file HalCommon.h
 * @brief Halcon 形态学兼容头（转发 HalOps）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONCOMMON_H
#define VISION_HALCONCOMMON_H
/** @brief 兼容旧头：形态学请用 HalOps.h */
#include "HalOps.h"

namespace Vision {
/** @deprecated 请用 morphOpenClose */
inline void morphRegion(const HalconCpp::HObject& bin, double openR, double closeR, HalconCpp::HObject*& out)
{
    morphOpenClose(bin, openR, closeR, out);
}
}
#endif
