/**
 * @file HalFindCircle.h
 * @brief HalFindCircle头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONFINDCIRCLE_H
#define VISION_HALCONFINDCIRCLE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalFindCircle
 * @brief 沿卡尺边缘点拟合圆（Halcon）
 */
class HalFindCircle : public AlgToolBase {
public:
    std::string toolName() const { return "locate.find_circle"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 沿卡尺边缘点拟合圆 */
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
    AlgToolBase* clone() const { return new HalFindCircle(); }
};

}

#endif