/**
 * @file ToolParaRstMerge.hpp
 * @brief ToolParaRstMerge头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARARSTMERGE_H
#define VISION_TOOLPARARSTMERGE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

struct RstMergeKey {
    static constexpr const char* minArea = "minArea";
    static constexpr const char* maxCount = "maxCount";
};

/**
 * @class ToolParaRstMerge
 * @brief RstMergeTool 运行参数
 */
class ToolParaRstMerge : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        double minArea;             ///< 合并后各区域外接框面积之和（像素²）的下限，小于它判 NG
        int maxCount;               ///< 合并后区域数上限，超过判 NG；0 不限
        /** @brief 缺省不设限 */
        Run()
            : minArea(0.0)
            , maxCount(0)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.minArea = m.value("minArea").toDouble(0.0);
        run.maxCount = m.value("maxCount").toInt(0);
        judge = src.judge;
    }
};

}

#endif
