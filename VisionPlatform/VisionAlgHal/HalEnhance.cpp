/**
 * @file HalEnhance.cpp
 * @brief HalEnhance实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalEnhance.h"
#include "TextEncoding.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
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
static bool applyEnhanceOp(HalconCpp::HImage* img, const EnhanceStep& st, std::string* err)
{
    switch (st.op) {
    case EnhanceOp::EquHisto:
        *img = img->EquHistoImage();
        return true;
    case EnhanceOp::Emphasize: {
        const int mw = st.maskW < 1 ? 1 : st.maskW;
        const int mh = st.maskH < 1 ? 1 : st.maskH;
        *img = img->Emphasize(mw, mh, st.coeff);
        return true;
    }
    case EnhanceOp::Illuminate: {
        const int mw = st.maskW < 1 ? 1 : st.maskW;
        const int mh = st.maskH < 1 ? 1 : st.maskH;
        *img = img->Illuminate(mw, mh, st.coeff);
        return true;
    }
    case EnhanceOp::Scale:
        if (st.scaleMax) {
            *img = img->ScaleImageMax();
        } else {
            *img = img->ScaleImage(st.coeff, st.coeff2);
        }
        return true;
    case EnhanceOp::Invert:
        *img = img->InvertImage();
        return true;
    default:
        if (err) {
            *err = "未知增强算子";
        }
        return false;
    }
}

// 增强管线
namespace {
const char kVer_HalEnhance[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalEnhance[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalEnhance::version() const
{
    return kVer_HalEnhance;
}

// 版本变更，只追加
std::vector<VerNote> HalEnhance::versionNotes() const
{
    return verNotesFrom(kNotes_HalEnhance, static_cast<int>(sizeof(kNotes_HalEnhance) / sizeof(kNotes_HalEnhance[0])));
}
AlgToolOut HalEnhance::writeFullImage(const AlgToolIn& in, const HalconCpp::HImage& work, const std::string& okMsg) const
{
    AlgToolOut out;
    const HalconCpp::HImage full = work.FullDomain();
    std::string err;
    HsCommData::HsImage* pImage = &out.image.image;
    if (!hObjectToHsImage(full, inputImage(in).frameId, pImage, &err)) {
        return AlgToolOut::fail(err);
    }
    out.ok = true;
    out.message = okMsg;
    return out;
}

AlgToolOut HalEnhance::run(const AlgToolIn& in)
{
    try {
        HalconCpp::HImage gray;
        HalconCpp::HImage work;
        std::string err;
        HalconCpp::HImage* pGray = &gray;
        int w = 0;
        int h = 0;
        if (!hsImageToGray(inputImage(in), pGray, &w, &h, &err)) {
            return AlgToolOut::fail(err);
        }
        const HalconCpp::HObject roi = toolRoisToHObject(plainRois(in.rois), w, h);
        HalconCpp::HObject reduced;
        HalconCpp::ReduceDomain(gray, roi, &reduced);
        work = HalconCpp::HImage(reduced);
        std::vector<EnhanceStep> steps = enhanceStepsFromParams(in.run);
        if (steps.empty()) {
            EnhanceStep one;
            steps.push_back(one);
        }
        for (int i = 0; i < static_cast<int>(steps.size()); ++i) {
            if (!applyEnhanceOp(&work, steps[static_cast<size_t>(i)], &err)) {
                return AlgToolOut::fail(err);
            }
        }
        return writeFullImage(in, work, "图像增强完成");
    } catch (const HalconCpp::HException& e) {
        return AlgToolOut::fail(hExceptionText(e));
    }
}

}
