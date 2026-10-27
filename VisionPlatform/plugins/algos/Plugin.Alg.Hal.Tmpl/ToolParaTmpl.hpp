/**
 * @file ToolParaTmpl.hpp
 * @brief ToolParaTmpl头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARATMPL_H
#define VISION_TOOLPARATMPL_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaTmpl
 * @brief TmplTool 运行参数
 */
class ToolParaTmpl : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string method;         ///< 匹配方式
        double minScore;            ///< 最低分
        double angleRange;          ///< 角度范围
        int numMatches;             ///< 匹配个数
        std::string modelFile;      ///< 模型文件
        bool rebuildModel;          ///< 强制重建
        Run()
            : method("ncc")
            , minScore(0.5)
            , angleRange(15.0)
            , numMatches(1)
            , modelFile("model.ncm")
            , rebuildModel(false)
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.method = m.value("method").toString();
        if (run.method.empty()) {
            run.method = "ncc";
        }
        run.minScore = m.value("minScore").toDouble(0.5);
        run.angleRange = m.value("angleRange").toDouble(15.0);
        run.numMatches = m.value("numMatches").toInt(1);
        run.modelFile = m.value("modelFile").toString();
        if (run.modelFile.empty()) {
            run.modelFile = "model.ncm";
        }
        run.rebuildModel = m.value("rebuildModel").toBool(false);
        judge = src.judge;
    }
};

}

#endif
