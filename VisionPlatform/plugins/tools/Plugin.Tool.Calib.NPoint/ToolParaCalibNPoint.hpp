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
        std::string tablePath;      ///< 点表文件（每行 imageX,imageY,worldX,worldY）；空则用标定板图自动提点
        std::string imageDir;       ///< 标定板图目录，空则取 CalibMgr 的离线目录；自动提点时 board.csv 写在这里
        /** @brief 全部置空 */
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
