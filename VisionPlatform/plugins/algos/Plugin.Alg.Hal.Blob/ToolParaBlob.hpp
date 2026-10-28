/**
 * @file ToolParaBlob.hpp
 * @brief Blob 参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARABLOB_H
#define VISION_TOOLPARABLOB_H

#include "ToolParaBase.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 参数键。步骤字段写在 step_序号_ 后面 */
struct BlobKey {
    static constexpr const char* threshMode = "threshMode";
    static constexpr const char* absMin = "absMin";
    static constexpr const char* absMax = "absMax";
    static constexpr const char* relOffset = "relOffset";
    static constexpr const char* dynOffset = "dynOffset";
    static constexpr const char* dynKernel = "dynKernel";
    static constexpr const char* dark = "dark";
    static constexpr const char* openR = "openR";
    static constexpr const char* closeR = "closeR";
    static constexpr const char* minArea = "minArea";
    static constexpr const char* maxArea = "maxArea";
    static constexpr const char* minCirc = "minCirc";
    static constexpr const char* maxCirc = "maxCirc";
    static constexpr const char* okMinCount = "okMinCount";
    static constexpr const char* okMaxCount = "okMaxCount";
    static constexpr const char* okMinArea = "okMinArea";
    static constexpr const char* okMaxArea = "okMaxArea";
    static constexpr const char* absolute = "absolute";
    static constexpr const char* relative = "relative";
    static constexpr const char* dynamic = "dynamic";

    static constexpr const char* stepCount = "stepCount";
    static constexpr const char* type = "type";
    static constexpr const char* morphOp = "morphOp";
    static constexpr const char* radius = "radius";
    static constexpr const char* feature = "feature";
    static constexpr const char* selMin = "min";
    static constexpr const char* selMax = "max";
    static constexpr const char* roiIndex = "roiIndex";

    static constexpr const char* stepThresh = "threshold";
    static constexpr const char* stepMorph  = "morph";
    static constexpr const char* stepSelect = "select";

    static constexpr const char* morphOpen   = "open";
    static constexpr const char* morphClose  = "close";
    static constexpr const char* morphErode  = "erode";
    static constexpr const char* morphDilate = "dilate";

    static constexpr const char* featArea = "area";
    static constexpr const char* featCirc = "circularity";

    static constexpr const char* roi = "roi";
};

/**
 * @class ToolParaBlob
 * @brief Blob 参数
 */
class ToolParaBlob : public ToolParaBase {
public:
    struct Teach {
    };

    /** @brief 一步。类型是阈值、形态学或筛选 */
    struct Step {
        std::string type;
        std::string threshMode;
        int absMin;
        int absMax;
        double relOffset;
        double dynOffset;
        int dynKernel;
        bool dark;
        std::string morphOp;
        double radius;
        std::string feature;
        double minVal;
        double maxVal;
        int roiIndex;

        Step()
            : type(BlobKey::stepThresh)
            , threshMode(BlobKey::absolute)
            , absMin(80)
            , absMax(255)
            , relOffset(20.0)
            , dynOffset(15.0)
            , dynKernel(31)
            , dark(false)
            , morphOp(BlobKey::morphOpen)
            , radius(1.0)
            , feature(BlobKey::featArea)
            , minVal(50.0)
            , maxVal(0.0)
            , roiIndex(-1)
        {}
    };

    struct Run {
        int stepCount;
        std::vector<Step> steps;
        int okMinCount;
        int okMaxCount;
        double okMinArea;
        double okMaxArea;

        Run()
            : stepCount(1)
            , okMinCount(0)
            , okMaxCount(0)
            , okMinArea(0.0)
            , okMaxArea(0.0)
        {
            steps.push_back(Step());
        }
    };

    Teach teach;
    Run run;

    /** @brief 从 ToolParam 读步骤。没有步骤时按旧字段拼 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.steps.clear();
        int n = m.value(BlobKey::stepCount).toInt(0);
        if (n < 0) {
            n = 0;
        }
        for (int i = 0; i < n; ++i) {
            run.steps.push_back(readStep(m, i));
        }
        if (run.steps.empty()) {
            readLegacy(m, &run.steps);
        }
        run.stepCount = static_cast<int>(run.steps.size());
        run.okMinCount = m.value(BlobKey::okMinCount).toInt(0);
        run.okMaxCount = m.value(BlobKey::okMaxCount).toInt(0);
        run.okMinArea = m.value(BlobKey::okMinArea).toDouble(0.0);
        run.okMaxArea = m.value(BlobKey::okMaxArea).toDouble(0.0);
        judge = src.judge;
    }

    static std::string normalizeType(const std::string& raw)
    {
        if (raw == BlobKey::stepMorph)  return BlobKey::stepMorph;
        if (raw == BlobKey::stepSelect) return BlobKey::stepSelect;
        return BlobKey::stepThresh;
    }

    static std::string normalizeThreshMode(const std::string& raw)
    {
        if (raw == BlobKey::relative) return BlobKey::relative;
        if (raw == BlobKey::dynamic)  return BlobKey::dynamic;
        return BlobKey::absolute;
    }

    static std::string normalizeMorphOp(const std::string& raw)
    {
        if (raw == BlobKey::morphClose)  return BlobKey::morphClose;
        if (raw == BlobKey::morphErode)  return BlobKey::morphErode;
        if (raw == BlobKey::morphDilate) return BlobKey::morphDilate;
        return BlobKey::morphOpen;
    }

    static std::string normalizeFeature(const std::string& raw)
    {
        if (raw == BlobKey::featCirc) return BlobKey::featCirc;
        return BlobKey::featArea;
    }

private:
    static std::string stepKey(int index, const char* suffix)
    {
        return std::string("step_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
    }

    static Step readStep(const ParamMap& m, int index)
    {
        Step st;
        const std::string p = stepKey(index, "");
        st.type = normalizeType(m.value(p + BlobKey::type).toString());
        st.threshMode = normalizeThreshMode(m.value(p + BlobKey::threshMode).toString());
        st.absMin = m.value(p + BlobKey::absMin).toInt(80);
        st.absMax = m.value(p + BlobKey::absMax).toInt(255);
        st.relOffset = m.value(p + BlobKey::relOffset).toDouble(20.0);
        st.dynOffset = m.value(p + BlobKey::dynOffset).toDouble(15.0);
        st.dynKernel = m.value(p + BlobKey::dynKernel).toInt(31);
        st.dark = m.value(p + BlobKey::dark).toBool(false);
        st.morphOp = normalizeMorphOp(m.value(p + BlobKey::morphOp).toString());
        st.radius = m.value(p + BlobKey::radius).toDouble(1.0);
        st.feature = normalizeFeature(m.value(p + BlobKey::feature).toString());
        st.minVal = m.value(p + BlobKey::selMin).toDouble(50.0);
        st.maxVal = m.value(p + BlobKey::selMax).toDouble(0.0);
        st.roiIndex = m.value(p + BlobKey::roiIndex).toInt(-1);
        return st;
    }

    static void readLegacy(const ParamMap& m, std::vector<Step>* out)
    {
        Step th;
        th.type = BlobKey::stepThresh;
        th.threshMode = normalizeThreshMode(m.value(BlobKey::threshMode).toString());
        th.absMin = m.value(BlobKey::absMin).toInt(80);
        th.absMax = m.value(BlobKey::absMax).toInt(255);
        th.relOffset = m.value(BlobKey::relOffset).toDouble(20.0);
        th.dynOffset = m.value(BlobKey::dynOffset).toDouble(15.0);
        th.dynKernel = m.value(BlobKey::dynKernel).toInt(31);
        th.dark = m.value(BlobKey::dark).toBool(false);
        out->push_back(th);

        const double openR = m.value(BlobKey::openR).toDouble(1.0);
        if (openR > 0.4) {
            Step st;
            st.type = BlobKey::stepMorph;
            st.morphOp = BlobKey::morphOpen;
            st.radius = openR;
            out->push_back(st);
        }
        const double closeR = m.value(BlobKey::closeR).toDouble(0.0);
        if (closeR > 0.4) {
            Step st;
            st.type = BlobKey::stepMorph;
            st.morphOp = BlobKey::morphClose;
            st.radius = closeR;
            out->push_back(st);
        }

        Step sel;
        sel.type = BlobKey::stepSelect;
        sel.feature = BlobKey::featArea;
        sel.minVal = m.value(BlobKey::minArea).toDouble(50.0);
        sel.maxVal = m.value(BlobKey::maxArea).toDouble(0.0);
        out->push_back(sel);

        const double minCirc = m.value(BlobKey::minCirc).toDouble(0.0);
        const double maxCirc = m.value(BlobKey::maxCirc).toDouble(1.0);
        if (minCirc > 0.0 || maxCirc < 0.999) {
            Step circ;
            circ.type = BlobKey::stepSelect;
            circ.feature = BlobKey::featCirc;
            circ.minVal = minCirc;
            circ.maxVal = maxCirc;
            out->push_back(circ);
        }
    }
};

}

#endif
