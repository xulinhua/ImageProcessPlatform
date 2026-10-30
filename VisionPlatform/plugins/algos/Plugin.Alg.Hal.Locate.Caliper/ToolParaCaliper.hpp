/**
 * @file ToolParaCaliper.hpp
 * @brief ToolParaCaliper头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARACALIPER_H
#define VISION_TOOLPARACALIPER_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaCaliper
 * @brief CaliperTool 运行参数
 */
class ToolParaCaliper : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string mode;           ///< 模式：edge 找单边，其他值（缺省 width）按边对测宽
        double sigma;               ///< 测量剖面的平滑 sigma
        double threshold;           ///< 最小边缘幅值，灰度
        std::string transition;     ///< 边缘极性：all、positive、negative；空按 all
        std::string select;         ///< 边缘选择：all、first、last；空按 all
        int searchCount;            ///< 每块搜索区最多取的结果数；小于 1 按 1
        /** @brief 缺省：测宽，sigma 1，幅值 20，各取 1 个 */
        Run()
            : mode("width")
            , sigma(1.0)
            , threshold(20.0)
            , transition("all")
            , select("all")
            , searchCount(1)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.mode = m.value("mode").toString();
        if (run.mode.empty()) {
            run.mode = "width";
        }
        run.sigma = m.value("sigma").toDouble(1.0);
        run.threshold = m.value("threshold").toDouble(20.0);
        run.transition = m.value("transition").toString();
        if (run.transition.empty()) {
            run.transition = "all";
        }
        run.select = m.value("select").toString();
        if (run.select.empty()) {
            run.select = "all";
        }
        run.searchCount = m.value("searchCount").toInt(1);
        judge = src.judge;
    }
};

}

#endif
