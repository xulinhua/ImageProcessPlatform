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
        std::string resultPath;     ///< 结果文件：已算好的相机模型 json；非空时工具学习直接导入，不跑标定算法
        std::string imageDir;       ///< 标定板图片目录；空时工具层填相机的离线目录，算法读其中图片并在此生成标定板描述文件
        /** @brief 缺省两个路径都为空 */
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
