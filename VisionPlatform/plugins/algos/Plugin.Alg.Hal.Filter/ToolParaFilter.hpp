/**
 * @file ToolParaFilter.hpp
 * @brief FilterTool 运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAFILTER_H
#define VISION_TOOLPARAFILTER_H

#include "ToolParaBase.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 滤波工具落盘键。字符串只写在这里 */
struct FilterKey {
    static constexpr const char* stepCount = "stepCount";
    static constexpr const char* op = "op";
    static constexpr const char* w = "w";
    static constexpr const char* h = "h";
    static constexpr const char* c0 = "c0";
    static constexpr const char* c1 = "c1";
    static constexpr const char* scaleMax = "f";
    static constexpr const char* shape = "shape";
    static constexpr const char* roiIndex = "roiIndex";

    static constexpr const char* opEquHisto   = "EquHisto";
    static constexpr const char* opEmphasize  = "Emphasize";
    static constexpr const char* opIlluminate = "Illuminate";
    static constexpr const char* opScale      = "Scale";
    static constexpr const char* opInvert     = "Invert";
    static constexpr const char* opMean       = "Mean";
    static constexpr const char* opMedian     = "Median";
    static constexpr const char* opGauss      = "Gauss";
    static constexpr const char* opBilateral  = "Bilateral";
    static constexpr const char* opGrayOpen   = "GrayOpen";
    static constexpr const char* opGrayClose  = "GrayClose";
    static constexpr const char* opGrayErode  = "GrayErode";
    static constexpr const char* opGrayDilate = "GrayDilate";
    static constexpr const char* opTopHat     = "TopHat";
    static constexpr const char* opBottomHat  = "BottomHat";

    static constexpr const char* shapeRect    = "rectangle";
    static constexpr const char* shapeOctagon = "octagon";
    static constexpr const char* shapeRhombus = "rhombus";

    static constexpr const char* legacyStepCount = "pl_n";
    static constexpr const char* roi = "roi";
    static constexpr const char* outWidth = "filter.width";
    static constexpr const char* outHeight = "filter.height";
    static constexpr const char* outCostMs = "filter.costMs";
};

/**
 * @class ToolParaFilter
 * @brief FilterTool 运行参数
 */
class ToolParaFilter : public ToolParaBase {
public:
    /** @brief 示教参数。滤波不需要示教 */
    struct Teach {
    };

    /** @brief 一步预处理的运行参数 */
    struct Step {
        std::string op;
        int maskW;
        int maskH;
        double coeff;
        double coeff2;
        bool scaleMax;
        std::string shape;
        int roiIndex;

        Step()
            : op(FilterKey::opEquHisto)
            , maskW(7)
            , maskH(7)
            , coeff(1.0)
            , coeff2(3.0)
            , scaleMax(false)
            , shape(FilterKey::shapeRect)
            , roiIndex(-1)
        {
        }
    };

    /** @brief 运行参数：步数加有序步骤列表 */
    struct Run {
        int stepCount;
        std::vector<Step> steps;

        Run() : stepCount(1)
        {
            steps.push_back(Step());
        }
    };

    Teach teach;
    Run run;

    /** @brief 从 ToolParam 填写。新 step 键优先，旧 pl_* 只读 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.steps.clear();
        int n = m.value(FilterKey::stepCount).toInt(0);
        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                run.steps.push_back(readStep(m, i));
            }
        } else if (m.contains(FilterKey::legacyStepCount)) {
            n = m.value(FilterKey::legacyStepCount).toInt(0);
            for (int i = 0; i < n; ++i) {
                run.steps.push_back(readLegacyStep(m, i));
            }
        }
        if (run.steps.empty()) {
            run.steps.push_back(Step());
        }
        run.stepCount = static_cast<int>(run.steps.size());
        judge = src.judge;
    }

    /** @brief 兼容旧方案的数字算子名 */
    static std::string normalizeOp(const std::string& raw)
    {
        if (raw == "0" || raw == FilterKey::opEquHisto)   return FilterKey::opEquHisto;
        if (raw == "1" || raw == FilterKey::opEmphasize)  return FilterKey::opEmphasize;
        if (raw == "2" || raw == FilterKey::opIlluminate) return FilterKey::opIlluminate;
        if (raw == "3" || raw == FilterKey::opScale)      return FilterKey::opScale;
        if (raw == "4" || raw == FilterKey::opInvert)     return FilterKey::opInvert;
        if (raw == "5" || raw == FilterKey::opMean)       return FilterKey::opMean;
        if (raw == "6" || raw == FilterKey::opMedian)     return FilterKey::opMedian;
        if (raw == "7" || raw == FilterKey::opGauss)      return FilterKey::opGauss;
        if (raw == "8" || raw == FilterKey::opBilateral)  return FilterKey::opBilateral;
        if (raw == FilterKey::opGrayOpen)   return FilterKey::opGrayOpen;
        if (raw == FilterKey::opGrayClose)  return FilterKey::opGrayClose;
        if (raw == FilterKey::opGrayErode)  return FilterKey::opGrayErode;
        if (raw == FilterKey::opGrayDilate) return FilterKey::opGrayDilate;
        if (raw == FilterKey::opTopHat)     return FilterKey::opTopHat;
        if (raw == FilterKey::opBottomHat)  return FilterKey::opBottomHat;
        return FilterKey::opEquHisto;
    }

    /** @brief 形态学掩膜形状归一化 */
    static std::string normalizeShape(const std::string& raw)
    {
        if (raw == FilterKey::shapeOctagon) return FilterKey::shapeOctagon;
        if (raw == FilterKey::shapeRhombus) return FilterKey::shapeRhombus;
        return FilterKey::shapeRect;
    }

private:
    static std::string stepKey(int index, const char* suffix)
    {
        return std::string("step_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
    }

    static std::string legacyStepKey(int index, const char* suffix)
    {
        return std::string("pl_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
    }

    static Step readStep(const ParamMap& m, int index)
    {
        Step st;
        const std::string p = stepKey(index, "");
        st.op = normalizeOp(m.value(p + FilterKey::op).toString());
        st.maskW = m.value(p + FilterKey::w).toInt(7);
        st.maskH = m.value(p + FilterKey::h).toInt(7);
        st.coeff = m.value(p + FilterKey::c0).toDouble(1.0);
        st.coeff2 = m.value(p + FilterKey::c1).toDouble(3.0);
        st.scaleMax = m.value(p + FilterKey::scaleMax).toBool(false);
        st.shape = normalizeShape(m.value(p + FilterKey::shape).toString());
        st.roiIndex = m.value(p + FilterKey::roiIndex).toInt(-1);
        return st;
    }

    static Step readLegacyStep(const ParamMap& m, int index)
    {
        Step st;
        const std::string p = legacyStepKey(index, "");
        st.op = normalizeOp(m.value(p + FilterKey::op).toString());
        st.maskW = m.value(p + FilterKey::w).toInt(7);
        st.maskH = m.value(p + FilterKey::h).toInt(7);
        st.coeff = m.value(p + FilterKey::c0).toDouble(1.0);
        st.coeff2 = m.value(p + FilterKey::c1).toDouble(3.0);
        st.scaleMax = m.value(p + FilterKey::scaleMax).toBool(false);
        return st;
    }
};

}

#endif
