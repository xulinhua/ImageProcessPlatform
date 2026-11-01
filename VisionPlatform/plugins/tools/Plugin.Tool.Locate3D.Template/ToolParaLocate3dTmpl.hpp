/**
 * @file ToolParaLocate3dTmpl.hpp
 * @brief 点云模板定位参数（工具壳副本）
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

struct Locate3dTmplKey {
    static constexpr const char* leafSize = "leafSize";
    static constexpr const char* maxCorr = "maxCorr";
    static constexpr const char* maxIter = "maxIter";
    static constexpr const char* minFitness = "minFitness";
    static constexpr const char* modelFile = "modelFile";
    static constexpr const char* scene = "scene";
    static constexpr const char* tmpl = "template";
    static constexpr const char* fitness = "fitness";
    static constexpr const char* count = "count";
    static constexpr const char* tx = "tx";
    static constexpr const char* ty = "ty";
    static constexpr const char* tz = "tz";
    static constexpr const char* qx = "qx";
    static constexpr const char* qy = "qy";
    static constexpr const char* qz = "qz";
    static constexpr const char* qw = "qw";
    static constexpr const char* teachModel = "teach.model";
};

class ToolParaLocate3dTmpl : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaLocate3dTmpl"; }
    const char* typeName() const override { return typeId(); }

    struct Teach {
        double leafSize;
        std::string modelFile;
        Teach() : leafSize(1.0), modelFile("model.pcd") {}
    };
    struct Run {
        double maxCorr;
        int maxIter;
        double minFitness;
        Run() : maxCorr(5.0), maxIter(50), minFitness(1.0e9) {}
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
