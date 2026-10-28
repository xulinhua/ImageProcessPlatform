/**
 * @file ToolParaSmooth.hpp
 * @brief ToolParaSmooth头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARASMOOTH_H
#define VISION_TOOLPARASMOOTH_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaSmooth
 * @brief SmoothTool 运行参数
 */
class ToolParaSmooth : public ToolParaBase {
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
        double pl_0_c0;             ///< 灰度域
        double pl_0_c1;             ///< 空间域
        Run()
            : pl_n(1)
            , pl_0_op(1)
            , pl_0_w(3)
            , pl_0_h(3)
            , pl_0_c0(20.0)
            , pl_0_c1(3.0)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.pl_n = m.value("pl_n").toInt(1);
        run.pl_0_op = m.value("pl_0_op").toInt(1);
        run.pl_0_w = m.value("pl_0_w").toInt(3);
        run.pl_0_h = m.value("pl_0_h").toInt(3);
        run.pl_0_c0 = m.value("pl_0_c0").toDouble(20.0);
        run.pl_0_c1 = m.value("pl_0_c1").toDouble(3.0);
        judge = src.judge;
    }
};

}

#endif
