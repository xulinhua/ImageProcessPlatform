/**
 * @file ToolParaCalibPlate.hpp
 * @brief 标定板求内参运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARACALIBPLATE_H
#define VISION_TOOLPARACALIBPLATE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/** @brief 标定板工具落盘键。字符串只写在这里 */
struct CalibPlateKey {
    static constexpr const char* resultPath = "resultPath";
    static constexpr const char* imageDir = "imageDir";
    static constexpr const char* rows = "rows";
    static constexpr const char* cols = "cols";
    static constexpr const char* spacingMm = "spacingMm";
    static constexpr const char* minImages = "minImages";
    static constexpr const char* halconCam = "halconCam";
    static constexpr const char* count = "count";
};

/**
 * @class ToolParaCalibPlate
 * @brief CalibPlateTool 运行参数
 */
class ToolParaCalibPlate : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaCalibPlate"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数，没有则为空 */
    struct Teach {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string resultPath; ///< 已算好的相机模型 json；非空时工具层直接导入
        std::string imageDir;   ///< 标定板图片目录
        int rows;               ///< 标定板行数
        int cols;               ///< 标定板列数
        double spacingMm;       ///< 圆心间距，毫米
        int minImages;          ///< 至少几张图找到标定板
        Run()
            : resultPath("")
            , imageDir("")
            , rows(0)
            , cols(0)
            , spacingMm(0.0)
            , minImages(3)
        {}
    };

    Teach teach; ///< 示教参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.resultPath = m.value(CalibPlateKey::resultPath).toString();
        run.imageDir = m.value(CalibPlateKey::imageDir).toString();
        run.rows = m.value(CalibPlateKey::rows).toInt(0);
        run.cols = m.value(CalibPlateKey::cols).toInt(0);
        run.spacingMm = m.value(CalibPlateKey::spacingMm).toDouble(0);
        run.minImages = m.value(CalibPlateKey::minImages).toInt(3);
        judge = src.judge;
    }
};

}

#endif
