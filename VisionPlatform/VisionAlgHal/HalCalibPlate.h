/**
 * @file HalCalibPlate.h
 * @brief HalCalibPlate头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCON_CALIB_PLATE_H
#define VISION_HALCON_CALIB_PLATE_H

#include "AlgToolBase.h"

namespace Vision {

/** @class HalCalibPlate @brief Halcon caltab find and calibrate_cameras */
class HalCalibPlate : public AlgToolBase {
public:
    std::string toolName() const { return "calib.plate"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalCalibPlate(); }
};

}

#endif
