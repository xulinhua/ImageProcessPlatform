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
        int threshold;              ///< 灰度阈值 0~255；当前 Hal/Cv 算法都不读
        int minR;                   ///< 最小半径（像素），小于它的圆丢弃
        int maxR;                   ///< 最大半径（像素），大于它的圆丢弃
        bool dark;                  ///< 目标是否比背景暗；当前 Hal/Cv 算法都不读
        double sigma;               ///< Canny 边缘平滑 sigma，0.4~20
        int low;                    ///< Canny 低阈值，0~255
        int high;                   ///< Canny 高阈值，0~255
        /** @brief 缺省值与 FindCircleTool 参数说明一致 */
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
