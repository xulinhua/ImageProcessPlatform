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
        std::string bgMethod;       ///< 背景估计：mean 均值、median 中值、closing 灰度闭运算；其他按 mean
        int kernelW;                ///< 背景滤波核宽，像素；小于 3 按 3
        int kernelH;                ///< 背景滤波核高，像素；小于 3 按 3
        double residThresh;         ///< 残差阈值：与背景的灰度差绝对值不小于它的像素为缺陷
        double openR;               ///< 缺陷区开运算半径，像素
        int minArea;                ///< 缺陷最小面积，像素
        int maxArea;                ///< 缺陷最大面积，像素；0 不限
        /** @brief 缺省：均值背景 31×31，残差 18，开运算 1.5 */
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
