/**
 * @file CvUndistort.h
 * @brief CvUndistort头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCV_UNDISTORT_H
#define VISION_OPENCV_UNDISTORT_H

#include "AlgToolBase.h"

namespace Vision {

/** @class CvUndistort @brief initUndistortRectifyMap plus remap, cached by model stamp */
class CvUndistort : public AlgToolBase {
public:
    std::string toolName() const { return "calib.undistort"; }
    AlgLibKind algLib() const { return AlgLibCv; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override {
        if (!out) {
            return HS_ERR_NULL;
        }
        *out = run(in);
        if (out->ok) {
            out->errCode = HS_OK;
            return HS_OK;
        }
        if (out->errCode == HS_OK || out->errCode == 1) {
            out->errCode = HS_ERR_OPERATOR;
        }
        return out->errCode;
    }
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new CvUndistort(); }
};

}

#endif
