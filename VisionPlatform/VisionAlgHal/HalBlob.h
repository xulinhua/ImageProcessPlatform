/**
 * @file HalBlob.h
 * @brief HalBlob头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONBLOB_H
#define VISION_HALCONBLOB_H

#include "AlgToolBase.h"
#include "HalconCpp.h"

namespace Vision {

/**
 * @class HalBlob
 * @brief 绝对、相对、动态阈值 Blob（Halcon）
 */
class HalBlob : public AlgToolBase {
public:
    std::string toolName() const { return "detect.blob"; }
    AlgLibKind algLib() const { return AlgLibHal; }
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
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalBlob(); }

private:
    /** @brief 连通、计数并打包。面积上限为 0 时不封顶 */
    AlgToolOut packRegions(const HalconCpp::HObject& region, const AlgToolIn& in,
                           double minArea, double maxArea, const std::string& okMsg) const;
};

}

#endif