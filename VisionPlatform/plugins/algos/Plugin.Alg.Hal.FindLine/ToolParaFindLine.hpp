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
        std::string polarity;       ///< 极性
        std::string select;         ///< 选择
        double minLength;           ///< 最短长度
        double sigma;               ///< 平滑
        int low;                    ///< 低阈值
        int high;                   ///< 高阈值
        int searchCount;            ///< 查找条数
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
