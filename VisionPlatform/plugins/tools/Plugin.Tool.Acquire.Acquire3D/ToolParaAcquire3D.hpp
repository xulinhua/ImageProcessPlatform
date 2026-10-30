/**
 * @file ToolParaAcquire3D.hpp
 * @brief 三维采集工具运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAACQUIRE3D_H
#define VISION_TOOLPARAACQUIRE3D_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaAcquire3D
 * @brief Acquire3DTool 运行参数：单文件或文件夹循环读点云
 */
class ToolParaAcquire3D : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string sourceMode; ///< "file" 单文件，其余按文件夹循环
        std::string path;       ///< 单文件路径或文件夹路径
        /** @brief 缺省文件夹循环、路径空 */
        Run()
            : sourceMode("folder")
            , path("")
        {}
    };

    Learn learn; ///< 学习参数
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.sourceMode = m.value("sourceMode").toString();
        if (run.sourceMode.empty()) {
            run.sourceMode = "folder";
        }
        run.path = m.value("path").toString();
        judge = src.judge;
    }
};

}

#endif
