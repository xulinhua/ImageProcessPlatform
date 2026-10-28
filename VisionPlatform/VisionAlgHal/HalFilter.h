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
#include "HalconCpp.h"

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
    AlgToolBase* clone() const { return new HalFilter(); }

private:
    AlgToolOut writeFullImage(const AlgToolIn& in, const HalconCpp::HImage& work, const std::string& okMsg) const;
};

}

#endif
