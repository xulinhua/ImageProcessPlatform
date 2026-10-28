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
        int threshold;              ///< 阈值
        int minR;                   ///< 最小半径
        int maxR;                   ///< 最大半径
        bool dark;                  ///< 暗目标
        double sigma;               ///< 平滑
        int low;                    ///< 低阈值
        int high;                   ///< 高阈值
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
