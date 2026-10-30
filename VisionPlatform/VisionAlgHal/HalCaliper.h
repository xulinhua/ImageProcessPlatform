/**
 * @file HalCaliper.h
 * @brief HalCaliper头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONCALIPER_H
#define VISION_HALCONCALIPER_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalCaliper
 * @brief 沿卡尺采样找边（Halcon）
 */
class HalCaliper : public AlgToolBase {
public:
    /** @brief 挂到 locate.caliper 工具壳 */
    std::string toolName() const { return "locate.caliper"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 在卡尺 ROI 上采样找边 */
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
    /** @brief mode 为 edge 时找单边，否则找边对测宽；一个都没测到时 ok 为 false */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalCaliper(); }
};

}

#endif