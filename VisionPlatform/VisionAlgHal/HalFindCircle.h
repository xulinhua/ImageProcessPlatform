/**
 * @file HalFindCircle.h
 * @brief HalFindCircle头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONFINDCIRCLE_H
#define VISION_HALCONFINDCIRCLE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalFindCircle
 * @brief 搜索区内亚像素边缘分段后拟合圆（Halcon）
 */
class HalFindCircle : public AlgToolBase {
public:
    /** @brief 挂到 locate.find_circle 工具壳 */
    std::string toolName() const { return "locate.find_circle"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 调 run 找圆；未通过且没给错误码时记为 HS_ERR_OPERATOR */
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
    /** @brief Canny 亚像素边缘按线/圆分段，逐段拟合圆，取半径在 minR~maxR 内最大的一个；找不到时 ok 为 false */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalFindCircle(); }
};

}

#endif