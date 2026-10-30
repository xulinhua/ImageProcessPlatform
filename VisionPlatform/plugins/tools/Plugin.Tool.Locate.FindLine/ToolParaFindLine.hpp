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
        std::string polarity;       ///< 边缘极性：all / positive / negative；当前 Hal 算法不读
        std::string select;         ///< 每框取舍（按长度降序）：best 取前 searchCount 条 / first 只留最长 / last 只留最短
        double minLength;           ///< 线段最短长度（像素），更短的丢弃
        double sigma;               ///< Canny 边缘平滑 sigma，0.4~20
        int low;                    ///< Canny 低阈值，0~255
        int high;                   ///< Canny 高阈值，0~255
        int searchCount;            ///< 每个 ROI 最多输出条数，1~50
        /** @brief 缺省值与 FindLineTool 参数说明一致 */
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
