/**
 * @file CvSmooth.cpp
 * @brief CvSmooth实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvSmooth.h"
#include "TextEncoding.h"
#include "CvAlgoHelpers.h"
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
static bool applySmoothOp(cv::Mat* img, const SmoothStep& st, std::string* err)
{
    switch (st.op) {
    case SmoothOp::Mean: {
        int w = st.maskW < 1 ? 1 : st.maskW;
        int h = st.maskH < 1 ? 1 : st.maskH;
        if ((w % 2) == 0) {
            ++w;
        }
        if ((h % 2) == 0) {
            ++h;
        }
        cv::blur(*img, *img, cv::Size(w, h));
        return true;
    }
    case SmoothOp::Median: {
        int radius = st.maskW < 1 ? 1 : st.maskW;
        int k = radius * 2 + 1;
        if (k < 3) {
            k = 3;
        }
        if ((k % 2) == 0) {
            ++k;
        }
        cv::medianBlur(*img, *img, k);
        return true;
    }
    case SmoothOp::Gauss: {
        int size = st.maskW < 3 ? 3 : st.maskW;
        if ((size % 2) == 0) {
            ++size;
        }
        cv::GaussianBlur(*img, *img, cv::Size(size, size), 0);
        return true;
    }
    case SmoothOp::Bilateral: {
        cv::Mat tmp;
        const int d = 0;
        cv::bilateralFilter(*img, tmp, d, st.sigmaRange, st.sigmaSpace);
        *img = tmp;
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
const char kVer_CvSmooth[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_CvSmooth[] = {
    { "1.0.0.0", "首版" },
};
}

std::string CvSmooth::version() const
{
    return kVer_CvSmooth;
}

std::vector<Vision::VerNote> CvSmooth::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_CvSmooth, static_cast<int>(sizeof(kNotes_CvSmooth) / sizeof(kNotes_CvSmooth[0])));
}
AlgToolOut CvSmooth::run(const AlgToolIn& in)
{
    try {
        cv::Mat gray;
        cv::Mat work;
        cv::Mat roiMask;
        std::string err;
        cv::Mat* pGray = &gray;
        cv::Mat* pWork = &work;
        cv::Mat* pRoi = &roiMask;
        if (!preparePreprocessWork(in, pGray, pWork, pRoi, &err)) {
            return algorToolFail(err);
        }
        std::vector<SmoothStep> steps = smoothStepsFromParams(in.run);
        if (steps.empty()) {
            SmoothStep one;
            steps.push_back(one);
        }
        for (size_t i = 0; i < steps.size(); ++i) {
            if (!applySmoothOp(&work, steps[i], &err)) {
                return algorToolFail(err);
            }
        }
        return finishPreprocessWork(in, gray, work, roiMask, Vision::acpToUtf8("图像平滑完成"));
    } catch (const cv::Exception& e) {
        return algorToolFail(e.what() ? std::string(e.what()) : std::string("OpenCV error"));
    }
}

}
