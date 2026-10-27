/**
 * @file HalSmooth.h
 * @brief HalSmooth头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONSMOOTH_H
#define VISION_HALCONSMOOTH_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalSmooth
 * @brief 图像平滑流水（Halcon）
 */
class HalSmooth : public AlgToolBase {
public:
    std::string toolName() const { return "preprocess.smooth"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 按步骤做图像平滑 */
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalSmooth(); }
};

}

#endif