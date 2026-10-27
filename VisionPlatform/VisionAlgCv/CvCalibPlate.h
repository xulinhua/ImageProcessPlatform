/**
 * @file CvCalibPlate.h
 * @brief CvCalibPlate头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCV_CALIB_PLATE_H
#define VISION_OPENCV_CALIB_PLATE_H

#include "AlgToolBase.h"

namespace Vision {

/** @class CvCalibPlate @brief checkerboard or circle grid, then calibrateCamera */
class CvCalibPlate : public AlgToolBase {
public:
    std::string toolName() const { return "calib.plate"; }
    AlgLibKind algLib() const { return AlgLibCv; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new CvCalibPlate(); }
};

/** @class CvCalibPlane @brief one board image to image/world points for npoint */
class CvCalibPlane : public AlgToolBase {
public:
    std::string toolName() const { return "calib.npoint"; }
    AlgLibKind algLib() const { return AlgLibCv; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new CvCalibPlane(); }
};

}

#endif
