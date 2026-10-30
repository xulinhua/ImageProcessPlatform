/**
 * @file CvFindCircle.h
 * @brief CvFindCircle头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVFINDCIRCLE_H
#define VISION_OPENCVFINDCIRCLE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class CvFindCircle
 * @brief 用 Canny 拟合圆（OpenCV）
 */
class CvFindCircle : public AlgToolBase {
public:
    /** @brief 挂到 locate.find_circle 工具壳 */
    std::string toolName() const { return "locate.find_circle"; }
    /** @brief 算法库：OpenCV */
    AlgLibKind algLib() const { return AlgLibCv; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 用 Canny 拟合圆 */
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
    /** @brief 搜索区内 Canny 取轮廓，按 minR/maxR 和圆度筛选，取半径最大的最小外接圆；找不到时 ok 为 false */
    AlgToolOut run(const AlgToolIn& in);
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new CvFindCircle(); }
};

}

#endif