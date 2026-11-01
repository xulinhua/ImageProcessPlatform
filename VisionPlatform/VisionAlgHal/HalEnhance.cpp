/**
 * @file HalEnhance.cpp
 * @brief 图像增强流水（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalEnhance.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "preprocess.enhance";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版" },
};
}

HalEnhance::EnhanceStep::EnhanceStep()
    : op(EnhanceOp::EquHisto)
    , maskW(7)
    , maskH(7)
    , coeff(1.0)
    , coeff2(0.0)
    , scaleMax(false)
{
}

std::string HalEnhance::tool_name() const { return kName; }
AlgLibKind HalEnhance::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalEnhance::clone() const { return new HalEnhance(); }
std::string HalEnhance::version() const { return kVer; }
std::vector<VerNote> HalEnhance::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalEnhance::HalEnhance()
    : img_w_(0)
    , img_h_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&roi_region_);
    HalconCpp::GenEmptyObj(&result_image_);
}

HalEnhance::~HalEnhance()
{
}

void HalEnhance::init(const std::string&)
{
}

void HalEnhance::save(const std::string&)
{
}

int HalEnhance::tool_reset()
{
    steps_.clear();
    tol_.reset();
    shot_err_ = HS_OK;
    shot_msg_.clear();
    HalconCpp::GenEmptyObj(&result_image_);
    return HS_OK;
}

std::string HalEnhance::step_key(int i, const char* suffix) const
{
    return std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_" + suffix;
}

HalEnhance::EnhanceOp HalEnhance::enhance_op_from_int(int v) const
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

std::vector<HalEnhance::EnhanceStep> HalEnhance::steps_from_params(const ParamMap& p) const
{
    std::vector<EnhanceStep> out;
    const int n = p.value("pl_n").toInt(0);
    if (n <= 0) {
        return out;
    }
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        EnhanceStep st;
        st.op = enhance_op_from_int(p.value(step_key(i, "op")).toInt());
        st.maskW = p.value(step_key(i, "w")).toInt(st.maskW);
        st.maskH = p.value(step_key(i, "h")).toInt(st.maskH);
        st.coeff = p.value(step_key(i, "c0")).toDouble(st.coeff);
        st.coeff2 = p.value(step_key(i, "c1")).toDouble(st.coeff2);
        st.scaleMax = p.value(step_key(i, "f")).toInt(0) != 0;
        out.push_back(st);
    }
    return out;
}

bool HalEnhance::apply_enhance_op(HalconCpp::HImage* img, const EnhanceStep& st, std::string* err) const
{
    if (img == nullptr) {
        return false;
    }
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

int HalEnhance::proc_input(const AlgToolIn& in)
{
    HalconCpp::HObject gray;
    int w = 0;
    int h = 0;
    std::string err;
    HalconCpp::HObject* pGray = &gray;
    if (!inputToGray(in.imageNative.ptr, static_cast<int>(in.imageNative.algLib), inputImage(in), pGray, &w, &h, &err)) {
        shot_err_ = HS_ERR_IMAGE_EMPTY;
        shot_msg_ = err;
        return shot_err_;
    }
    if (w < 1 || h < 1) {
        shot_err_ = HS_ERR_IMAGE_EMPTY;
        shot_msg_ = HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage();
        return shot_err_;
    }
    image_ = gray;
    img_w_ = w;
    img_h_ = h;
    std::vector<HsCommData::HsObject> roiObjs;
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use == RegionUse::Normal) {
            roiObjs.push_back(one.roi);
        }
    }
    roi_region_ = toolRoisToHObject(roiObjs, img_w_, img_h_);
    ToolParaEnhance local;
    paraOf(in, &local);
    steps_ = steps_from_params(in.run);
    if (steps_.empty()) {
        EnhanceStep one;
        steps_.push_back(one);
    }
    return HS_OK;
}

int HalEnhance::alg_run(const AlgToolIn& in)
{
    (void)in;
    HalconCpp::HObject reduced;
    HalconCpp::ReduceDomain(image_, roi_region_, &reduced);
    HalconCpp::HImage work(reduced);
    std::string err;
    for (size_t i = 0; i < steps_.size(); ++i) {
        if (!apply_enhance_op(&work, steps_[i], &err)) {
            shot_err_ = HS_ERR_OPERATOR;
            shot_msg_ = err;
            return shot_err_;
        }
    }
    const HalconCpp::HImage full = work.FullDomain();
    result_image_ = full;
    shot_err_ = HS_OK;
    shot_msg_ = "图像增强完成";
    return HS_OK;
}

int HalEnhance::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    tol_.put_int("outWidth", img_w_);
    tol_.put_int("outHeight", img_h_);
    // 预处理仅输出图像，无独立测值公差域；结构保留以便挂接 judge 表
    tol_.judge(in.judge, nullptr);
    return HS_OK;
}

int HalEnhance::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_TYPE;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK;
    result.ok = shot_err_ == HS_OK && tol_.ok;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK) {
        tol_.apply(&result);
        std::string err;
        HsCommData::HsImage* view = &result.image.image;
        if (!hObjectToHsImage(result_image_, inputImage(in).frameId, view, &err)) {
            result.ok = false;
            result.errCode = HS_ERR_OPERATOR;
            result.message = err;
            *out = result;
            return HS_ERR_OPERATOR;
        }
        result.image.native.algLib = AlgLibHal;
        result.image.native.ptr = &result_image_;
    }
    *out = result;
    return shot_err_;
}

int HalEnhance::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    try {
        run_timer_.start();
        tool_reset();
        run_timer_.mark("tool_reset");
        int stepRet = proc_input(in);
        run_timer_.mark("proc_input");
        if (stepRet == HS_OK) {
            stepRet = alg_run(in);
            run_timer_.mark("alg_run");
        }
        if (stepRet == HS_OK) {
            stepRet = judge_tol(in);
            run_timer_.mark("judge_tol");
        }
        alg_rst_proc(in, out);
        run_timer_.stop("alg_rst_proc");
        write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
        return stepRet != HS_OK ? stepRet : shot_err_;
    } catch (const HalconCpp::HException& e) {
        if (out) {
            out->ok = false;
            out->errCode = HS_ERR_OPERATOR;
            out->message = hExceptionText(e);
        }
        return HS_ERR_OPERATOR;
    }
}

}
