/**
 * @file ToolParaTexture.hpp
 * @brief ToolParaTexture头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARATEXTURE_H
#define VISION_TOOLPARATEXTURE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaTexture
 * @brief TextureTool 运行参数
 */
class ToolParaTexture : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string bgMethod;       ///< 背景
        int kernelW;                ///< 核宽
        int kernelH;                ///< 核高
        double residThresh;         ///< 残差阈值
        double openR;               ///< 开运算
        int minArea;                ///< 最小面积
        int maxArea;                ///< 最大面积
        Run()
            : bgMethod("mean")
            , kernelW(31)
            , kernelH(31)
            , residThresh(18.0)
            , openR(1.5)
            , minArea(30)
            , maxArea(0)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.bgMethod = m.value("bgMethod").toString();
        if (run.bgMethod.empty()) {
            run.bgMethod = "mean";
        }
        run.kernelW = m.value("kernelW").toInt(31);
        run.kernelH = m.value("kernelH").toInt(31);
        run.residThresh = m.value("residThresh").toDouble(18.0);
        run.openR = m.value("openR").toDouble(1.5);
        run.minArea = m.value("minArea").toInt(30);
        run.maxArea = m.value("maxArea").toInt(0);
        judge = src.judge;
    }
};

}

#endif
