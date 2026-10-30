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
 * @brief 大核背景差找纹理缺陷：原图与估计背景做差后阈值分割（Halcon）
 */
class HalTexture : public AlgToolBase {
public:
    /** @brief 挂到 detect.texture 工具壳 */
    std::string toolName() const { return "detect.texture"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 调 run 找缺陷；未通过且没给错误码时记为 HS_ERR_OPERATOR */
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
    /**
     * @brief 搜索区内按 bgMethod（mean/median/closing）估背景，差值超过 residThresh 的像素经开运算后按面积筛选
     * @return 找到连通域时 ok 为 true，values 写 count、area；一个都没有时 ok 为 false
     */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalTexture(); }

private:
    /** @brief 连通、按面积筛选并打包。面积上限为 0 时不封顶 */
    AlgToolOut packRegions(const HalconCpp::HObject& region, const AlgToolIn& in,
                           double minArea, double maxArea, const std::string& okMsg) const;
};

}

#endif