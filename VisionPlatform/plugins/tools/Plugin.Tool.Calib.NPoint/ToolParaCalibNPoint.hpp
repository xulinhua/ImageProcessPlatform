/**
 * @file ToolParaCalibNPoint.hpp
 * @brief ToolParaCalibNPoint头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARACALIBNPOINT_H
#define VISION_TOOLPARACALIBNPOINT_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaCalibNPoint
 * @brief CalibNPointTool 运行参数
 */
class ToolParaCalibNPoint : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string tablePath;      ///< 点表
        std::string imageDir;       ///< imageDir
        Run()
            : tablePath("")
            , imageDir("")
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.tablePath = m.value("tablePath").toString();
        run.imageDir = m.value("imageDir").toString();
        judge = src.judge;
    }
};

}

#endif
