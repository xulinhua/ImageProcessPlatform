/**
 * @file ToolParaCalibPlate.hpp
 * @brief ToolParaCalibPlate头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARACALIBPLATE_H
#define VISION_TOOLPARACALIBPLATE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaCalibPlate
 * @brief CalibPlateTool 运行参数
 */
class ToolParaCalibPlate : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string resultPath;     ///< 已算好的相机模型 json；非空则直接导入，空则用标定板图现算
        std::string imageDir;       ///< 标定板图目录，空则取 CalibMgr 的离线目录
        /** @brief 全部置空 */
        Run()
            : resultPath("")
            , imageDir("")
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.resultPath = m.value("resultPath").toString();
        run.imageDir = m.value("imageDir").toString();
        judge = src.judge;
    }
};

}

#endif
