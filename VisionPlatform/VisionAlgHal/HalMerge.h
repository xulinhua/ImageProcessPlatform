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
    /** @brief 挂到 result.merge 工具壳 */
    std::string toolName() const { return "result.merge"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
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
    /**
     * @brief 汇总上游区域、轮廓和非空 ROI，统计 count、area，按 minArea、maxCount 判定
     * @note 区域按并集算面积，其余几何按外接框面积累加
     */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalMerge(); }
};

}

#endif