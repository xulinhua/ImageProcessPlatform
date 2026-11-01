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
 * @class TextureKey
 * @brief 纹理工具落盘键。字符串只写在这里
 */
struct TextureKey {
    static constexpr const char* bgMethod = "bgMethod"; ///< 背景估计方法
    static constexpr const char* kernelW = "kernelW"; ///< 核宽
    static constexpr const char* kernelH = "kernelH"; ///< 核高
    static constexpr const char* residThresh = "residThresh"; ///< 残差阈值
    static constexpr const char* openR = "openR"; ///< 开运算半径
    static constexpr const char* minArea = "minArea"; ///< 最小面积
    static constexpr const char* maxArea = "maxArea"; ///< 最大面积
    static constexpr const char* mean = "mean"; ///< bgMethod 取值：均值
    static constexpr const char* roi = "roi"; ///< ROI 槽名
    static constexpr const char* count = "count"; ///< 输出个数
    static constexpr const char* resultRoi = "resultRoi"; ///< 结果区域端口
    static constexpr const char* verdict = "verdict"; ///< 判定文字
};

/**
 * @class ToolParaTexture
 * @brief TextureTool 运行参数
 */
class ToolParaTexture : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaTexture"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }
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
