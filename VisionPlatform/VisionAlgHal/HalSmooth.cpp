/**
 * @file HalSmooth.cpp
 * @brief HalSmooth实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalSmooth.h"
#include "TextEncoding.h"
#include "HalAlgoHelpers.h"
#include "HalImageConvert.h"
namespace Vision {
// 第i步参数键
static std::string stepKey(int i, const char* suffix)
{
    return std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_" + suffix;
}

// 平滑算子
enum class SmoothOp {
    Mean = 0,
    Median,
    Gauss,
    Bilateral
};

// 平滑管线一步
struct SmoothStep {
    SmoothOp op;
    int maskW;
    int maskH;
    double sigmaRange;
    double sigmaSpace;
    SmoothStep()
        : op(SmoothOp::Median)
        , maskW(3)
        , maskH(3)
        , sigmaRange(20.0)
        , sigmaSpace(3.0)
    {}
};

// 整型转平滑算子
static SmoothOp smoothOpFromInt(int v)
{
    switch (v) {
    case 0: return SmoothOp::Mean;
    case 1: return SmoothOp::Median;
    case 2: return SmoothOp::Gauss;
    case 3: return SmoothOp::Bilateral;
    default: return SmoothOp::Median;
    }
}

// 从参数解开平滑步骤
static std::vector<SmoothStep> smoothStepsFromParams(const ParamMap& p)
{
    std::vector<SmoothStep> out;
    const int n = p.value("pl_n").toInt(0);
    if (n <= 0) {
        return out;
    }
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        SmoothStep st;
        st.op = smoothOpFromInt(p.value(stepKey(i, "op")).toInt());
        st.maskW = p.value(stepKey(i, "w")).toInt(st.maskW);
        st.maskH = p.value(stepKey(i, "h")).toInt(st.maskH);
        st.sigmaRange = p.value(stepKey(i, "c0")).toDouble(st.sigmaRange);
        st.sigmaSpace = p.value(stepKey(i, "c1")).toDouble(st.sigmaSpace);
        out.push_back(st);
    }
    return out;
}

// 平滑一步
static bool applySmoothOp(HalconCpp::HImage* img, const SmoothStep& st, std::string* err)
{
    switch (st.op) {
    case SmoothOp::Mean: {
        const int w = st.maskW < 1 ? 1 : st.maskW;
        const int h = st.maskH < 1 ? 1 : st.maskH;
        *img = img->MeanImage(w, h);
        return true;
    }
    case SmoothOp::Median: {
        const int radius = st.maskW < 1 ? 1 : st.maskW;
        *img = img->MedianImage("circle", radius, "mirrored");
        return true;
    }
    case SmoothOp::Gauss: {
        int size = st.maskW < 3 ? 3 : st.maskW;
        if ((size % 2) == 0) {
            ++size;
        }
        *img = img->GaussFilter(size);
        return true;
    }
    case SmoothOp::Bilateral: {
        HalconCpp::HTuple empty;
        *img = img->BilateralFilter(*img, st.sigmaSpace, st.sigmaRange, empty, empty);
        return true;
    }
    default:
        if (err) {
            *err = Vision::acpToUtf8("未知平滑算子");
        }
        return false;
    }
}

// 平滑管线
namespace {
const char kVer_HalSmooth[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalSmooth[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalSmooth::version() const
{
    return kVer_HalSmooth;
}

// 版本变更，只追加
std::vector<VerNote> HalSmooth::versionNotes() const
{
    return verNotesFrom(kNotes_HalSmooth, static_cast<int>(sizeof(kNotes_HalSmooth) / sizeof(kNotes_HalSmooth[0])));
}
AlgToolOut HalSmooth::run(const AlgToolIn& in)
{
    try {
        HalconCpp::HImage gray;
        HalconCpp::HImage work;
        std::string err;
        HalconCpp::HImage* pGray = &gray;
        HalconCpp::HImage* pWork = &work;
        if (!preparePreprocessWork(in, pGray, pWork, &err)) {
            return algorToolFail(err);
        }
        std::vector<SmoothStep> steps = smoothStepsFromParams(in.run);
        if (steps.empty()) {
            SmoothStep one;
            steps.push_back(one);
        }
        for (int i = 0; i < static_cast<int>(steps.size()); ++i) {
            if (!applySmoothOp(&work, steps[static_cast<size_t>(i)], &err)) {
                return algorToolFail(err);
            }
        }
        return finishPreprocessWork(in, work, Vision::acpToUtf8("图像平滑完成"));
    } catch (const HalconCpp::HException& e) {
        return algorToolFail(hExceptionText(e));
    }
}

}
