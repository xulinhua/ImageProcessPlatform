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
    /** @brief 挂到 preprocess.filter 工具壳 */
    std::string toolName() const { return "preprocess.filter"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 调 run 逐步滤波；未通过且没给错误码时记为 HS_ERR_OPERATOR */
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
     * @brief 按 ToolParaFilter 的步骤逐步处理灰度图；绑定区域的步骤只改该区域
     * @return 整幅结果图，values 写输出宽高，objects 为各步用到的区域
     */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalFilter(); }

private:
    /** @brief 结果图恢复全定义域后转成输出图，ok 置 true 并写 okMsg */
    AlgToolOut writeFullImage(const AlgToolIn& in, const HalconCpp::HImage& work, const std::string& okMsg) const;
};

}

#endif
