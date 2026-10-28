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
#include "HalconCpp.h"

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
    AlgToolBase* clone() const { return new HalEnhance(); }

private:
    AlgToolOut writeFullImage(const AlgToolIn& in, const HalconCpp::HImage& work, const std::string& okMsg) const;
};

}

#endif