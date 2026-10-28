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
    AlgToolBase* clone() const { return new HalMerge(); }
};

}

#endif