/**
 * @file ToolParaEnhance.hpp
 * @brief ToolParaEnhance头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAENHANCE_H
#define VISION_TOOLPARAENHANCE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaEnhance
 * @brief EnhanceTool 运行参数
 */
class ToolParaEnhance : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        int pl_n;                   ///< 步骤数
        int pl_0_op;                ///< 算子
        int pl_0_w;                 ///< 掩膜宽
        int pl_0_h;                 ///< 掩膜高
        double pl_0_c0;             ///< 系数
        double pl_0_c1;             ///< 系数2
        int pl_0_f;                 ///< 缩放到最大
        Run()
            : pl_n(1)
            , pl_0_op(0)
            , pl_0_w(7)
            , pl_0_h(7)
            , pl_0_c0(1.0)
            , pl_0_c1(0.0)
            , pl_0_f(0)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.pl_n = m.value("pl_n").toInt(1);
        run.pl_0_op = m.value("pl_0_op").toInt(0);
        run.pl_0_w = m.value("pl_0_w").toInt(7);
        run.pl_0_h = m.value("pl_0_h").toInt(7);
        run.pl_0_c0 = m.value("pl_0_c0").toDouble(1.0);
        run.pl_0_c1 = m.value("pl_0_c1").toDouble(0.0);
        run.pl_0_f = m.value("pl_0_f").toInt(0);
        judge = src.judge;
    }
};

}

#endif
