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
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaSmooth"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        int pl_n;                   ///< 步骤数；步骤字段写在 pl_序号_ 后面，这里只映射第 0 步
        int pl_0_op;                ///< 算子：0 均值，1 中值，2 高斯，3 双边；其他按中值
        int pl_0_w;                 ///< 掩膜宽，像素：均值用宽高，中值作半径，高斯取奇数尺寸且不小于 3
        int pl_0_h;                 ///< 掩膜高，像素；均值用
        double pl_0_c0;             ///< 灰度域：双边滤波的灰度 sigma
        double pl_0_c1;             ///< 空间域：双边滤波的空间 sigma
        /** @brief 缺省一步中值滤波，半径 3 */
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
