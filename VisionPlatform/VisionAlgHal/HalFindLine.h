/**
 * @file HalFindLine.h
 * @brief HalFindLine头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONFINDLINE_H
#define VISION_HALCONFINDLINE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalFindLine
 * @brief 各 ROI 内亚像素边缘切成直线段后拟合直线，支持多 ROI（Halcon）
 */
class HalFindLine : public AlgToolBase {
public:
    /** @brief 挂到 locate.find_line 工具壳 */
    std::string toolName() const { return "locate.find_line"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 在各 ROI 上拟合直线 */
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
     * @brief 无 ROI 时用整幅图；每块 ROI 按长度降序最多取 searchCount 条（短于 minLength 的丢弃），
     *        select 为 first/last 时只留最长/最短一条
     * @return 位姿取第一条线的中点和方向；objects 为各条线段，末尾附一块线条带并集区域
     */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalFindLine(); }
};

}

#endif