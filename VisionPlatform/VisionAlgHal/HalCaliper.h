/**
 * @file HalCaliper.h
 * @brief HalCaliper头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONCALIPER_H
#define VISION_HALCONCALIPER_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalCaliper
 * @brief 沿卡尺采样找边（Halcon）
 */
class HalCaliper : public AlgToolBase {
public:
    std::string toolName() const { return "locate.caliper"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 在卡尺 ROI 上采样找边 */
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
    AlgToolBase* clone() const { return new HalCaliper(); }
};

}

#endif