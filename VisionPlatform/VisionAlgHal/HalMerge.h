/**
 * @file HalMerge.h
 * @brief HalMerge头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONMERGE_H
#define VISION_HALCONMERGE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalMerge
 * @brief 合并区域并统计数量和面积（Halcon）
 */
class HalMerge : public AlgToolBase {
public:
    std::string toolName() const { return "result.merge"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 合并区域并输出结果 ROI */
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalMerge(); }
};

}

#endif