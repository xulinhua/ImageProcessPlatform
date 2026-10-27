/**
 * @file HalFilter.h
 * @brief 图像滤波流水。增强、平滑和灰度形态学按步骤做（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALFILTER_H
#define VISION_HALFILTER_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalFilter
 * @brief 图像滤波流水（Halcon）
 */
class HalFilter : public AlgToolBase {
public:
    std::string toolName() const { return "preprocess.filter"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    std::vector<VerNote> versionNotes() const;
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalFilter(); }
};

}

#endif
