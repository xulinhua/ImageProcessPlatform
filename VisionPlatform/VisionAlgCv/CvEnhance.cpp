/**
 * @file CvEnhance.cpp
 * @brief CvEnhance实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvEnhance.h"
#include "TextEncoding.h"
#include "CvAlgoHelpers.h"
namespace Vision {
// 第i步参数键
static std::string stepKey(int i, const char* suffix)
{
    return std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_" + suffix;
}

// 增强算子
enum class EnhanceOp {
    EquHisto = 0,
    Emphasize,
    Illuminate,
    Scale,
    Invert
};

// 增强管线一步
struct EnhanceStep {
    EnhanceOp op;
    int maskW;
    int maskH;
    double coeff;
    double coeff2;
    bool scaleMax;
    EnhanceStep()
        : op(EnhanceOp::EquHisto)
        , maskW(7)
        , maskH(7)
        , coeff(1.0)
        , coeff2(0.0)
        , scaleMax(false)
    {}
};

// 整型转增强算子
static EnhanceOp enhanceOpFromInt(int v)
{
    switch (v) {
    case 0: return EnhanceOp::EquHisto;
    case 1: return EnhanceOp::Emphasize;
    case 2: return EnhanceOp::Illuminate;
    case 3: return EnhanceOp::Scale;
    case 4: return EnhanceOp::Invert;
    default: return EnhanceOp::EquHisto;
    }
}

// 从参数解开增强步骤
static std::vector<EnhanceStep> enhanceStepsFromParams(const ParamMap& p)
{
    std::vector<EnhanceStep> out;
    const int n = p.value("pl_n").toInt(0);
    if (n <= 0) {
        return out;
    }
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        EnhanceStep st;
        st.op = enhanceOpFromInt(p.value(stepKey(i, "op")).toInt());
        st.maskW = p.value(stepKey(i, "w")).toInt(st.maskW);
        st.maskH = p.value(stepKey(i, "h")).toInt(st.maskH);
        st.coeff = p.value(stepKey(i, "c0")).toDouble(st.coeff);
        st.coeff2 = p.value(stepKey(i, "c1")).toDouble(st.coeff2);
        st.scaleMax = p.value(stepKey(i, "f")).toInt(0) != 0;
        out.push_back(st);
    }
    return out;
}

// 增强一步
static bool applyEnhanceOp(cv::Mat* img, const EnhanceStep& st, std::string* err)
{
    switch (st.op) {
    case EnhanceOp::EquHisto:
        cv::equalizeHist(*img, *img);
        return true;
    case EnhanceOp::Emphasize: {
        int mw = st.maskW < 1 ? 1 : st.maskW;
        int mh = st.maskH < 1 ? 1 : st.maskH;
        if ((mw % 2) == 0) {
            ++mw;
        }
        if ((mh % 2) == 0) {
            ++mh;
        }
        cv::Mat blur;
        cv::blur(*img, blur, cv::Size(mw, mh));
        cv::Mat diff;
        cv::subtract(*img, blur, diff, cv::noArray(), CV_16S);
        cv::Mat scaled;
        diff.convertTo(scaled, CV_16S, st.coeff);
        cv::Mat src16;
        img->convertTo(src16, CV_16S);
        cv::Mat sum;
        cv::add(src16, scaled, sum);
        sum.convertTo(*img, CV_8U);
        return true;
    }
    case EnhanceOp::Illuminate: {
        int mw = st.maskW < 1 ? 1 : st.maskW;
        int mh = st.maskH < 1 ? 1 : st.maskH;
        if ((mw % 2) == 0) {
            ++mw;
        }
        if ((mh % 2) == 0) {
            ++mh;
        }
        cv::Mat blur;
        cv::blur(*img, blur, cv::Size(mw, mh));
        cv::Mat fImg;
        cv::Mat fBlur;
        img->convertTo(fImg, CV_32F);
        blur.convertTo(fBlur, CV_32F);
        cv::Mat out;
        cv::divide(fImg, fBlur + 1e-3f, out);
        out *= static_cast<float>(128.0 * (st.coeff > 0.0 ? st.coeff : 1.0));
        out.convertTo(*img, CV_8U);
        return true;
    }
    case EnhanceOp::Scale:
        if (st.scaleMax) {
            double mn = 0;
            double mx = 0;
            cv::minMaxLoc(*img, &mn, &mx);
            if (mx > mn + 1e-9) {
                img->convertTo(*img, CV_8U, 255.0 / (mx - mn), -mn * 255.0 / (mx - mn));
            }
        } else {
            cv::Mat tmp;
            img->convertTo(tmp, CV_32F, st.coeff, st.coeff2);
            tmp.convertTo(*img, CV_8U);
        }
        return true;
    case EnhanceOp::Invert:
        cv::bitwise_not(*img, *img);
        return true;
    default:
        if (err) {
            *err = Vision::acpToUtf8("未知增强算子");
        }
        return false;
    }
}

// 增强管线
namespace {
const char kVer_CvEnhance[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_CvEnhance[] = {
    { "1.0.0.0", "首版" },
};
}

std::string CvEnhance::version() const
{
    return kVer_CvEnhance;
}

std::vector<Vision::VerNote> CvEnhance::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_CvEnhance, static_cast<int>(sizeof(kNotes_CvEnhance) / sizeof(kNotes_CvEnhance[0])));
}
AlgToolOut CvEnhance::run(const AlgToolIn& in)
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
        std::vector<EnhanceStep> steps = enhanceStepsFromParams(in.run);
        if (steps.empty()) {
            EnhanceStep one;
            steps.push_back(one);
        }
        for (size_t i = 0; i < steps.size(); ++i) {
            if (!applyEnhanceOp(&work, steps[i], &err)) {
                return algorToolFail(err);
            }
        }
        return finishPreprocessWork(in, gray, work, roiMask, Vision::acpToUtf8("图像增强完成"));
    } catch (const cv::Exception& e) {
        return algorToolFail(e.what() ? std::string(e.what()) : std::string("OpenCV error"));
    }
}

}
