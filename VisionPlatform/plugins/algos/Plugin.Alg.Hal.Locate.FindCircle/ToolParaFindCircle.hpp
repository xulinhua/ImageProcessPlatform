/**
 * @file ToolParaFindCircle.hpp
 * @brief ToolParaFindCircle头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAFINDCIRCLE_H
#define VISION_TOOLPARAFINDCIRCLE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaFindCircle
 * @brief FindCircleTool 运行参数
 */
class ToolParaFindCircle : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        int threshold;              ///< 阈值；找圆算法当前不读
        int minR;                   ///< 最小半径，像素；拟合半径小于它的丢弃
        int maxR;                   ///< 最大半径，像素；拟合半径大于它的丢弃
        bool dark;                  ///< 暗目标；找圆算法当前不读
        double sigma;               ///< 边缘平滑 sigma，小于 0.4 按 0.4
        int low;                    ///< Canny 低阈值
        int high;                   ///< Canny 高阈值
        /** @brief 缺省：半径 8~400，sigma 1，阈值 20/40 */
        Run()
            : threshold(80)
            , minR(8)
            , maxR(400)
            , dark(true)
            , sigma(1.0)
            , low(20)
            , high(40)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.threshold = m.value("threshold").toInt(80);
        run.minR = m.value("minR").toInt(8);
        run.maxR = m.value("maxR").toInt(400);
        run.dark = m.value("dark").toBool(true);
        run.sigma = m.value("sigma").toDouble(1.0);
        run.low = m.value("low").toInt(20);
        run.high = m.value("high").toInt(40);
        judge = src.judge;
    }
};

}

#endif
