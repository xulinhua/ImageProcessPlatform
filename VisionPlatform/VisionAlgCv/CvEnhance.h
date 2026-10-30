/**
 * @file CvEnhance.h
 * @brief CvEnhance头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVENHANCE_H
#define VISION_OPENCVENHANCE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class CvEnhance
 * @brief 图像增强流水（OpenCV）
 */
class CvEnhance : public AlgToolBase {
public:
    /** @brief 挂到 preprocess.enhance 工具壳 */
    std::string toolName() const { return "preprocess.enhance"; }
    /** @brief 算法库：OpenCV */
    AlgLibKind algLib() const { return AlgLibCv; }
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
    /** @brief 按 pl_n、pl_i_* 解出增强步骤依次处理灰度图（无步骤时做一次直方图均衡），只把搜索区写回输出图 */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new CvEnhance(); }
};

}

#endif