/**
 * @file HalUndistort.h
 * @brief HalUndistort头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCON_UNDISTORT_H
#define VISION_HALCON_UNDISTORT_H

#include "AlgToolBase.h"

namespace Vision {

/** @class HalUndistort @brief change_radial_distortion_image from camera.halcon.dat text */
class HalUndistort : public AlgToolBase {
public:
    std::string toolName() const { return "calib.undistort"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalUndistort(); }
};

}

#endif
