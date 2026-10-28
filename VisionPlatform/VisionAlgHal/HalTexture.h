/**
 * @file HalTexture.h
 * @brief HalTexture头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONTEXTURE_H
#define VISION_HALCONTEXTURE_H

#include "AlgToolBase.h"
#include "HalconCpp.h"

namespace Vision {

/**
 * @class HalTexture
 * @brief 用纹理模型比较缺陷（Halcon）
 */
class HalTexture : public AlgToolBase {
public:
    std::string toolName() const { return "detect.texture"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 用纹理模型比较缺陷 */
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
    AlgToolBase* clone() const { return new HalTexture(); }

private:
    /** @brief 连通、按面积筛选并打包 */
    AlgToolOut packRegions(const HalconCpp::HObject& region, const AlgToolIn& in,
                           double minArea, double maxArea, const std::string& okMsg) const;
};

}

#endif