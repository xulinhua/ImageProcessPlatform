/**
 * @file ToolParaAcquire.hpp
 * @brief ToolParaAcquire头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAACQUIRE_H
#define VISION_TOOLPARAACQUIRE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaAcquire
 * @brief AcquireTool 运行参数
 */
class ToolParaAcquire : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string sourceMode;     ///< sourceMode
        std::string path;           ///< path
        std::string deviceStatus;   ///< deviceStatus
        std::string images;         ///< images
        Run()
            : sourceMode("")
            , path("")
            , deviceStatus("")
            , images("")
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.sourceMode = m.value("sourceMode").toString();
        run.path = m.value("path").toString();
        run.deviceStatus = m.value("deviceStatus").toString();
        run.images = m.value("images").toString();
        judge = src.judge;
    }
};

}

#endif
