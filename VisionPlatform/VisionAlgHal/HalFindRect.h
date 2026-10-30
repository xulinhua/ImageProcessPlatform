/**
 * @file HalFindRect.h
 * @brief 找矩形（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONFINDRECT_H
#define VISION_HALCONFINDRECT_H

#include "AlgToolBase.h"
#include "HalconCpp.h"

namespace Vision {

/**
 * @class HalFindRect
 * @brief 在搜索区里取亚像素边缘，拟合旋转矩形，取面积最大的一个（Halcon）
 */
class HalFindRect : public AlgToolBase {
public:
    /** @brief 挂到 locate.find_rect 工具壳 */
    std::string toolName() const { return "locate.find_rect"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 找矩形。不留本拍状态，可重入 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalFindRect(); }

private:
    /** @brief 一个拟合出的矩形。角度为 Halcon 弧度 */
    struct Fit {
        double row;  ///< 中心行
        double col;  ///< 中心列
        double phi;  ///< 转角
        double len1; ///< 半长
        double len2; ///< 半宽
        double dist; ///< 边缘点到矩形的平均距离
        /** @brief 全部清零 */
        Fit() : row(0), col(0), phi(0), len1(0), len2(0), dist(0) {}
    };

    /** @brief 对一条轮廓拟合矩形。拟合失败返回 false */
    bool fitOne(const HalconCpp::HObject& contour, Fit* fit) const;
};

}

#endif
