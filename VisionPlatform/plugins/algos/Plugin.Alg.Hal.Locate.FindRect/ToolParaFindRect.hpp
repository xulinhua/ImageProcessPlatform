/**
 * @file ToolParaFindRect.hpp
 * @brief 找矩形的运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAFINDRECT_H
#define VISION_TOOLPARAFINDRECT_H

#include "ToolParaBase.h"

namespace Vision {

/**
 * @class FindRectKey
 * @brief 找矩形落盘键。字符串只写在这里
 */
struct FindRectKey {
    static constexpr const char* sigma = "sigma"; ///< Canny 亚像素边缘的平滑 sigma，缺省 1，小于 0.4 按 0.4
    static constexpr const char* low = "low"; ///< Canny 低阈值，梯度幅值，缺省 20
    static constexpr const char* high = "high"; ///< Canny 高阈值，梯度幅值，缺省 40；小于 low 时取 low
    static constexpr const char* minSide = "minSide"; ///< 最短边下限，像素，缺省 10；轮廓长度不到 4 倍的先剔除
    static constexpr const char* maxSide = "maxSide"; ///< 最长边上限，像素；0 不限
    static constexpr const char* maxDist = "maxDist"; ///< 边缘点到拟合矩形的平均距离上限，像素，缺省 2；<= 0 按 2
    static constexpr const char* roi = "roi"; ///< ROI 槽名：搜索区
    static constexpr const char* width = "width"; ///< 输出值键：矩形沿主方向的边长（2×len1），像素
    static constexpr const char* height = "height"; ///< 输出值键：矩形另一边长（2×len2），像素
    static constexpr const char* dist = "dist"; ///< 输出值键：边缘点到矩形的平均距离，像素
};

/**
 * @class ToolParaFindRect
 * @brief 找矩形的运行参数
 */
class ToolParaFindRect : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaFindRect"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数。找矩形不建模型 */
    struct Teach {
    };

    /** @brief 运行参数 */
    struct Run {
        double sigma;   ///< 边缘平滑
        int low;        ///< 边缘低阈值
        int high;       ///< 边缘高阈值
        double minSide; ///< 最短边下限，像素
        double maxSide; ///< 最长边上限，像素。0 表示不限
        double maxDist; ///< 边缘点到矩形的平均距离上限，像素
        /** @brief 缺省：sigma 1，阈值 20/40，最短边 10，平均距离 2 */
        Run()
            : sigma(1.0)
            , low(20)
            , high(40)
            , minSide(10.0)
            , maxSide(0.0)
            , maxDist(2.0)
        {}
    };

    Teach teach; ///< 示教参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写。非法值回到可用范围 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.sigma = m.value(FindRectKey::sigma).toDouble(1.0);
        if (run.sigma < 0.4) {
            run.sigma = 0.4;
        }
        run.low = m.value(FindRectKey::low).toInt(20);
        run.high = m.value(FindRectKey::high).toInt(40);
        if (run.high < run.low) {
            run.high = run.low;
        }
        run.minSide = m.value(FindRectKey::minSide).toDouble(10.0);
        run.maxSide = m.value(FindRectKey::maxSide).toDouble(0.0);
        run.maxDist = m.value(FindRectKey::maxDist).toDouble(2.0);
        if (run.maxDist <= 0.0) {
            run.maxDist = 2.0;
        }
        judge = src.judge;
    }
};

}

#endif
