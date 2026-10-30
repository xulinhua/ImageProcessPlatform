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
    /** @brief 挂到 preprocess.enhance 工具壳 */
    std::string toolName() const { return "preprocess.enhance"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
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
    /** @brief 灰度图缩到搜索区定义域后按 pl_n、pl_i_* 步骤增强（无步骤时一步缺省），再输出整幅图 */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalEnhance(); }

private:
    /** @brief 结果图恢复全定义域后转成输出图，ok 置 true 并写 okMsg */
    AlgToolOut writeFullImage(const AlgToolIn& in, const HalconCpp::HImage& work, const std::string& okMsg) const;
};

}

#endif