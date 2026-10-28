/**
 * @file ToolParaCalibRotate.hpp
 * @brief ToolParaCalibRotate头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARACALIBROTATE_H
#define VISION_TOOLPARACALIBROTATE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaCalibRotate
 * @brief CalibRotateTool 运行参数
 */
class ToolParaCalibRotate : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string tablePath;      ///< 点表
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.tablePath = m.value("tablePath").toString();
        judge = src.judge;
    }
};

}

#endif
