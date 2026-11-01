/**
 * @file CvCommon.h
 * @brief OpenCV 形态学/编排兼容头（转发 CvOps、CvAlgoHelpers）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVCOMMON_H
#define VISION_OPENCVCOMMON_H
/** @brief 兼容旧头：编排请用 CvAlgoHelpers.h；形态学请用 CvOps.h */
#include "CvAlgoHelpers.h"
#include "CvOps.h"

namespace Vision {
/** @deprecated 请用 morphOpenClose */
inline void morphMask(const cv::Mat& bin, double openR, double closeR, cv::Mat*& out)
{
    morphOpenClose(bin, openR, closeR, out);
}
}
#endif
