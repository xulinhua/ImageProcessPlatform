/**
 * @file ToolParaLocate3dTmpl.hpp
 * @brief 点云模板定位的示教参数和运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARALOCATE3DTMPL_H
#define VISION_TOOLPARALOCATE3DTMPL_H

#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>

namespace Vision {

/**
 * @class Locate3dTmplKey
 * @brief 点云模板工具落盘键
 */
struct Locate3dTmplKey {
    static constexpr const char* leafSize = "leafSize";       ///< 体素边长，建模/配准前降采样；0 不降
    static constexpr const char* maxCorr = "maxCorr";         ///< ICP 最大对应距离
    static constexpr const char* maxIter = "maxIter";         ///< 最大迭代次数
    static constexpr const char* minFitness = "minFitness";   ///< 最低 fitness（越小越好时作上限 RMSE）
    static constexpr const char* modelFile = "modelFile";     ///< 模型文件名，空按 model.pcd
    static constexpr const char* scene = "scene";             ///< 场景点云口名
    static constexpr const char* tmpl = "template";           ///< 模板点云口名 / 示教输入口
    static constexpr const char* fitness = "fitness";         ///< 输出：配准误差
    static constexpr const char* count = "count";             ///< 输出：匹配个数
    static constexpr const char* tx = "tx";
    static constexpr const char* ty = "ty";
    static constexpr const char* tz = "tz";
    static constexpr const char* qx = "qx";
    static constexpr const char* qy = "qy";
    static constexpr const char* qz = "qz";
    static constexpr const char* qw = "qw";
    static constexpr const char* teachModel = "teach.model";  ///< 动作：用模板口点云建临时模型并试配准
};

/**
 * @class ToolParaLocate3dTmpl
 * @brief 点云模板定位参数。示教键优先读 learn，没有再读 run
 */
class ToolParaLocate3dTmpl : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaLocate3dTmpl"; }
    const char* typeName() const override { return typeId(); }

    struct Teach {
        double leafSize;       ///< 体素边长
        std::string modelFile; ///< 模型文件名
        Teach()
            : leafSize(1.0)
            , modelFile("model.pcd")
        {}
    };

    struct Run {
        double maxCorr;     ///< 最大对应距离
        int maxIter;        ///< 最大迭代
        double minFitness;  ///< fitness 上限（ICP score / RMSE）
        Run()
            : maxCorr(5.0)
            , maxIter(50)
            , minFitness(1.0e9)
        {}
    };

    Teach teach;
    Run run;

    int loadCode() const { return loadCode_; }

    void load(const ToolParam& src) override
    {
        loadCode_ = HS_OK;
        const ParamMap& learnMap = src.learn;
        const ParamMap& runMap = src.run;
        const ParamMap& leafMap = learnMap.contains(Locate3dTmplKey::leafSize) ? learnMap : runMap;
        teach.leafSize = leafMap.value(Locate3dTmplKey::leafSize).toDouble(1.0);
        if (teach.leafSize < 0.0) {
            teach.leafSize = 0.0;
        }
        teach.modelFile = learnMap.contains(Locate3dTmplKey::modelFile)
            ? learnMap.value(Locate3dTmplKey::modelFile).toString()
            : runMap.value(Locate3dTmplKey::modelFile).toString();
        if (teach.modelFile.empty()) {
            teach.modelFile = "model.pcd";
        }
        run.maxCorr = runMap.value(Locate3dTmplKey::maxCorr).toDouble(5.0);
        if (run.maxCorr < 0.0) {
            run.maxCorr = 0.0;
        }
        run.maxIter = runMap.value(Locate3dTmplKey::maxIter).toInt(50);
        if (run.maxIter < 1) {
            run.maxIter = 1;
        }
        run.minFitness = runMap.value(Locate3dTmplKey::minFitness).toDouble(1.0e9);
        if (run.minFitness < 0.0) {
            run.minFitness = 0.0;
        }
        judge = src.judge;
    }

private:
    int loadCode_ = HS_OK;
};

}

#endif
