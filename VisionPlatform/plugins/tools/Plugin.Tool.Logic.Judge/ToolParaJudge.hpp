/**
 * @file ToolParaJudge.hpp
 * @brief ToolParaJudge头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAJUDGE_H
#define VISION_TOOLPARAJUDGE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaJudge
 * @brief JudgeTool 运行参数
 */
class ToolParaJudge : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        bool useOkAnd;              ///< 是否把上游各有效逻辑口的 OK 做与运算并入结果
        int termCount;              ///< 数值条件条数，0~16；每条读 term{i}_id / _key / _op / _val
        /** @brief 缺省汇总上游 OK、无数值条件 */
        Run()
            : useOkAnd(true)
            , termCount(0)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.useOkAnd = m.value("useOkAnd").toBool(true);
        run.termCount = m.value("termCount").toInt(0);
        judge = src.judge;
    }
};

}

#endif
