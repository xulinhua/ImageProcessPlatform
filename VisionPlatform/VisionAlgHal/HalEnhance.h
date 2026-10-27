/**
 * @file HalEnhance.h
 * @brief HalEnhance头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONENHANCE_H
#define VISION_HALCONENHANCE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalEnhance
 * @brief 图像增强流水（Halcon）
 */
class HalEnhance : public AlgToolBase {
public:
    std::string toolName() const { return "preprocess.enhance"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 按步骤做图像增强 */
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalEnhance(); }
};

}

#endif