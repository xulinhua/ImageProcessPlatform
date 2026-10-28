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
    std::string toolName() const { return "preprocess.enhance"; }
    AlgLibKind algLib() const { return AlgLibCv; }
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
    AlgToolBase* clone() const { return new CvEnhance(); }
};

}

#endif