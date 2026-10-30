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
        int pl_n;                   ///< 叠加步骤数，0~8；本结构只存第 0 步
        int pl_0_op;                ///< 第 0 步算子：0 直方图均衡 / 1 强调 / 2 照度 / 3 缩放 / 4 反色
        int pl_0_w;                 ///< 掩膜宽（像素），1~255，强调、照度用
        int pl_0_h;                 ///< 掩膜高（像素），1~255，强调、照度用
        double pl_0_c0;             ///< 强调、照度为强度因子；缩放为乘数
        double pl_0_c1;             ///< 缩放的加数
        int pl_0_f;                 ///< 缩放时 1 表示拉伸到满灰度，此时忽略 c0、c1
        /** @brief 缺省一步直方图均衡 */
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
