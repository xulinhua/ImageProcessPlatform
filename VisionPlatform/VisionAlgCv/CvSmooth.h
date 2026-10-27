/**
 * @file CvSmooth.h
 * @brief CvSmooth头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVSMOOTH_H
#define VISION_OPENCVSMOOTH_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class CvSmooth
 * @brief 图像平滑流水（OpenCV）
 */
class CvSmooth : public AlgToolBase {
public:
    std::string toolName() const { return "preprocess.smooth"; }
    AlgLibKind algLib() const { return AlgLibCv; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 按步骤做图像平滑 */
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new CvSmooth(); }
};

}

#endif