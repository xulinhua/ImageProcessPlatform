/**
 * @file CvBlob.h
 * @brief CvBlob头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVBLOB_H
#define VISION_OPENCVBLOB_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class CvBlob
 * @brief 绝对、相对、动态阈值 Blob（OpenCV）
 */
class CvBlob : public AlgToolBase {
public:
    /** @brief 挂到 detect.blob 工具壳 */
    std::string toolName() const { return "detect.blob"; }
    /** @brief 算法库：OpenCV */
    AlgLibKind algLib() const { return AlgLibCv; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 在搜索域里做阈值分割并输出连通域 */
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
    /** @brief 阈值分割、连通域筛选并按数量/总面积判定；失败时 ok 为 false 并带 message */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new CvBlob(); }
};

}

#endif