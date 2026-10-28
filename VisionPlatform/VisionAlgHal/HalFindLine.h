/**
 * @file HalFindLine.h
 * @brief HalFindLine头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONFINDLINE_H
#define VISION_HALCONFINDLINE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalFindLine
 * @brief 沿卡尺边缘点拟合直线，支持多 ROI（Halcon）
 */
class HalFindLine : public AlgToolBase {
public:
    std::string toolName() const { return "locate.find_line"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 在各 ROI 上拟合直线 */
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
    AlgToolBase* clone() const { return new HalFindLine(); }
};

}

#endif