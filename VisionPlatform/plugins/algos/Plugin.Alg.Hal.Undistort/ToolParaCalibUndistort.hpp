/**
 * @file ToolParaCalibUndistort.hpp
 * @brief ToolParaCalibUndistort头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARACALIBUNDISTORT_H
#define VISION_TOOLPARACALIBUNDISTORT_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaCalibUndistort
 * @brief CalibUndistortTool 运行参数
 */
class ToolParaCalibUndistort : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        judge = src.judge;
    }
};

}

#endif
