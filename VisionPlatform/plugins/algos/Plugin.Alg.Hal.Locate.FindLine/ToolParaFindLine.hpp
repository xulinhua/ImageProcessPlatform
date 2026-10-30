/**
 * @file ToolParaFindLine.hpp
 * @brief ToolParaFindLine头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAFINDLINE_H
#define VISION_TOOLPARAFINDLINE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaFindLine
 * @brief FindLineTool 运行参数
 */
class ToolParaFindLine : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string polarity;       ///< 极性；找直线算法当前不读
        std::string select;         ///< 选择：first 只留最长，last 只留最短，其他值按长度从长到短取
        double minLength;           ///< 最短长度，像素；更短的线段不拟合
        double sigma;               ///< 边缘平滑 sigma，小于 0.4 按 0.4
        int low;                    ///< Canny 低阈值
        int high;                   ///< Canny 高阈值
        int searchCount;            ///< 每块搜索区最多取的直线数；小于 1 按 1
        /** @brief 缺省：取最佳 1 条，最短 20，sigma 1，阈值 20/40 */
        Run()
            : polarity("all")
            , select("best")
            , minLength(20.0)
            , sigma(1.0)
            , low(20)
            , high(40)
            , searchCount(1)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.polarity = m.value("polarity").toString();
        if (run.polarity.empty()) {
            run.polarity = "all";
        }
        run.select = m.value("select").toString();
        if (run.select.empty()) {
            run.select = "best";
        }
        run.minLength = m.value("minLength").toDouble(20.0);
        run.sigma = m.value("sigma").toDouble(1.0);
        run.low = m.value("low").toInt(20);
        run.high = m.value("high").toInt(40);
        run.searchCount = m.value("searchCount").toInt(1);
        judge = src.judge;
    }
};

}

#endif
