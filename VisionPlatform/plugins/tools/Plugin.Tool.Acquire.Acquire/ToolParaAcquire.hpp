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
        std::string sourceMode;     ///< 取图来源："file" 单文件，"folder" 文件夹；其余按绑定设备状态走
        std::string path;           ///< 单文件路径或文件夹路径（file / folder 模式用）
        std::string deviceStatus;   ///< 绑定设备状态，Job 执行前写入：missing / not_acquire / not_camera / no_sdk / image 等
        std::string images;         ///< 图像模块记录的路径，换行分隔，Job 执行前写入；按帧轮流取
        /** @brief 全部置空 */
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
