/**
 * @file HalFilter.cpp
 * @brief 图像滤波流水。增强、平滑和灰度形态学按步骤做（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalFilter.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "preprocess.filter";
const char kPreviewFilter[] = "preview.filter";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版：多步骤滤波，支持形态学和步骤绑定区域" },
};
}

HalFilter::FilterStep::FilterStep()
    : op(FilterOp::EquHisto)
    , maskW(7)
    , maskH(7)
    , coeff(1.0)
    , coeff2(3.0)
    , scaleMax(false)
    , shape(FilterKey::shapeRect)
    , roiIndex(-1)
{
}

std::string HalFilter::tool_name() const { return kName; }
AlgLibKind HalFilter::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalFilter::clone() const { return new HalFilter(); }
std::string HalFilter::version() const { return kVer; }
std::vector<VerNote> HalFilter::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalFilter::HalFilter()
    : img_w_(0)
    , img_h_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&result_image_);
}

HalFilter::~HalFilter()
{
}

void HalFilter::init(const std::string&)
{
}

void HalFilter::save(const std::string&)
{
}

int HalFilter::tool_reset()
{
    steps_.clear();
    step_rois_.clear();
    tol_.reset();
    shot_err_ = HS_OK;
    shot_msg_.clear();
    HalconCpp::GenEmptyObj(&result_image_);
    return HS_OK;
}

bool HalFilter::filter_op_from_string(const std::string& v, FilterOp* out) const
{
    if (out == nullptr) {
        return false;
    }
    if (v == FilterKey::opEquHisto)   { *out = FilterOp::EquHisto;   return true; }
    if (v == FilterKey::opEmphasize)  { *out = FilterOp::Emphasize;  return true; }
    if (v == FilterKey::opIlluminate) { *out = FilterOp::Illuminate; return true; }
    if (v == FilterKey::opScale)      { *out = FilterOp::Scale;      return true; }
    if (v == FilterKey::opInvert)     { *out = FilterOp::Invert;     return true; }
    if (v == FilterKey::opMean)       { *out = FilterOp::Mean;       return true; }
    if (v == FilterKey::opMedian)     { *out = FilterOp::Median;     return true; }
    if (v == FilterKey::opGauss)      { *out = FilterOp::Gauss;      return true; }
    if (v == FilterKey::opBilateral)  { *out = FilterOp::Bilateral;  return true; }
    if (v == FilterKey::opGrayOpen)   { *out = FilterOp::GrayOpen;   return true; }
    if (v == FilterKey::opGrayClose)  { *out = FilterOp::GrayClose;  return true; }
    if (v == FilterKey::opGrayErode)  { *out = FilterOp::GrayErode;  return true; }
    if (v == FilterKey::opGrayDilate) { *out = FilterOp::GrayDilate; return true; }
    if (v == FilterKey::opTopHat)     { *out = FilterOp::TopHat;     return true; }
    if (v == FilterKey::opBottomHat)  { *out = FilterOp::BottomHat;  return true; }
    return false;
}

void HalFilter::morph_mask(const FilterStep& st, int* maskH, int* maskW, const char** shape) const
{
    int side = st.maskW > st.maskH ? st.maskW : st.maskH;
    if (side < 1) {
        side = 1;
    }
    if (side % 2 == 0) {
        ++side;
    }
    *maskH = side;
    *maskW = side;
    if (st.shape == FilterKey::shapeOctagon) {
        *shape = "octagon";
    } else if (st.shape == FilterKey::shapeRhombus) {
        *shape = "rhombus";
    } else {
        *shape = "rectangle";
    }
}

bool HalFilter::apply_filter_op(HalconCpp::HObject* img, const FilterStep& st, std::string* err) const
{
    switch (st.op) {
    case FilterOp::EquHisto: {
        HalconCpp::HObject out;
        HalconCpp::EquHistoImage(*img, &out);
        *img = out;
        return true;
    }
    case FilterOp::Emphasize: {
        const int mw = st.maskW < 1 ? 1 : st.maskW;
        const int mh = st.maskH < 1 ? 1 : st.maskH;
        HalconCpp::HObject out;
        HalconCpp::Emphasize(*img, &out, mw, mh, st.coeff);
        *img = out;
        return true;
    }
    case FilterOp::Illuminate: {
        const int mw = st.maskW < 1 ? 1 : st.maskW;
        const int mh = st.maskH < 1 ? 1 : st.maskH;
        HalconCpp::HObject out;
        HalconCpp::Illuminate(*img, &out, mw, mh, st.coeff);
        *img = out;
        return true;
    }
    case FilterOp::Scale:
        if (st.scaleMax) {
            HalconCpp::HObject out;
            HalconCpp::ScaleImageMax(*img, &out);
            *img = out;
        } else {
            HalconCpp::HObject out;
            HalconCpp::ScaleImage(*img, &out, st.coeff, st.coeff2);
            *img = out;
        }
        return true;
    case FilterOp::Invert: {
        HalconCpp::HObject out;
        HalconCpp::InvertImage(*img, &out);
        *img = out;
        return true;
    }
    case FilterOp::Mean: {
        const int w = st.maskW < 1 ? 1 : st.maskW;
        const int h = st.maskH < 1 ? 1 : st.maskH;
        HalconCpp::HObject out;
        HalconCpp::MeanImage(*img, &out, w, h);
        *img = out;
        return true;
    }
    case FilterOp::Median: {
        const int radius = st.maskW < 1 ? 1 : st.maskW;
        HalconCpp::HObject out;
        HalconCpp::MedianImage(*img, &out, "circle", radius, "mirrored");
        *img = out;
        return true;
    }
    case FilterOp::Gauss: {
        static const int kGaussSizes[] = { 3, 5, 7, 9, 11 };
        int best = st.maskW < 3 ? 3 : st.maskW;
        if (best > kGaussSizes[4]) {
            best = kGaussSizes[4];
        }
        int chosen = kGaussSizes[0];
        for (int i = 0; i < 5; ++i) {
            if (kGaussSizes[i] <= best) {
                chosen = kGaussSizes[i];
            }
        }
        HalconCpp::HObject out;
        HalconCpp::GaussFilter(*img, &out, chosen);
        *img = out;
        return true;
    }
    case FilterOp::Bilateral: {
        HalconCpp::HTuple empty;
        HalconCpp::HObject out;
        HalconCpp::BilateralFilter(*img, *img, &out, st.coeff2, st.coeff, empty, empty);
        *img = out;
        return true;
    }
    case FilterOp::GrayOpen: {
        int mh = 0;
        int mw = 0;
        const char* shape = nullptr;
        morph_mask(st, &mh, &mw, &shape);
        HalconCpp::HObject out;
        HalconCpp::GrayOpeningShape(*img, &out, mh, mw, shape);
        *img = out;
        return true;
    }
    case FilterOp::GrayClose: {
        int mh = 0;
        int mw = 0;
        const char* shape = nullptr;
        morph_mask(st, &mh, &mw, &shape);
        HalconCpp::HObject out;
        HalconCpp::GrayClosingShape(*img, &out, mh, mw, shape);
        *img = out;
        return true;
    }
    case FilterOp::GrayErode: {
        int mh = 0;
        int mw = 0;
        const char* shape = nullptr;
        morph_mask(st, &mh, &mw, &shape);
        HalconCpp::HObject out;
        HalconCpp::GrayErosionShape(*img, &out, mh, mw, shape);
        *img = out;
        return true;
    }
    case FilterOp::GrayDilate: {
        int mh = 0;
        int mw = 0;
        const char* shape = nullptr;
        morph_mask(st, &mh, &mw, &shape);
        HalconCpp::HObject out;
        HalconCpp::GrayDilationShape(*img, &out, mh, mw, shape);
        *img = out;
        return true;
    }
    case FilterOp::TopHat: {
        int mh = 0;
        int mw = 0;
        const char* shape = nullptr;
        morph_mask(st, &mh, &mw, &shape);
        HalconCpp::HObject opened;
        HalconCpp::GrayOpeningShape(*img, &opened, mh, mw, shape);
        HalconCpp::HObject out;
        HalconCpp::SubImage(*img, opened, &out, 1.0, 0.0);
        *img = out;
        return true;
    }
    case FilterOp::BottomHat: {
        int mh = 0;
        int mw = 0;
        const char* shape = nullptr;
        morph_mask(st, &mh, &mw, &shape);
        HalconCpp::HObject closed;
        HalconCpp::GrayClosingShape(*img, &closed, mh, mw, shape);
        HalconCpp::HObject out;
        HalconCpp::SubImage(closed, *img, &out, 1.0, 0.0);
        *img = out;
        return true;
    }
    default:
        if (err) {
            *err = "未知滤波算子";
        }
        return false;
    }
}

int HalFilter::build_steps(const ToolParaFilter::Run& rp, std::vector<FilterStep>* steps, std::string* err)
{
    if (steps == nullptr) {
        return HS_ERR_TYPE;
    }
    steps->clear();
    for (size_t i = 0; i < rp.steps.size(); ++i) {
        const ToolParaFilter::Step& src = rp.steps[i];
        FilterStep st;
        if (!filter_op_from_string(src.op, &st.op)) {
            if (err) {
                *err = "未知滤波算子";
            }
            return HS_ERR_PARAM;
        }
        if (st.op == FilterOp::Bilateral && (src.coeff <= 0.0 || src.coeff2 <= 0.0)) {
            if (err) {
                *err = "双边滤波的标准差必须大于 0";
            }
            return HS_ERR_PARAM;
        }
        st.maskW = src.maskW;
        st.maskH = src.maskH;
        st.coeff = src.coeff;
        st.coeff2 = src.coeff2;
        st.scaleMax = src.scaleMax;
        st.shape = src.shape;
        st.roiIndex = src.roiIndex;
        steps->push_back(st);
    }
    return HS_OK;
}

void HalFilter::collect_step_rois(const AlgToolIn& in)
{
    step_rois_.clear();
    struct IndexedRegion {
        int index;
        HalconCpp::HObject region;
    };
    std::vector<IndexedRegion> rows;
    auto pushOne = [&](int index, const HsCommData::HsObject& shape) {
        if (!shape.isInitialized() || shape.isEmpty()) {
            return;
        }
        try {
            const HalconCpp::HObject piece = hsObjectToHObject(shape, img_w_, img_h_);
            if (hoIsEmpty(piece)) {
                return;
            }
            IndexedRegion row;
            row.index = index;
            row.region = piece;
            rows.push_back(row);
        } catch (const HalconCpp::HException&) {
        }
    };
    for (size_t i = 0; i < in.regions.size(); ++i) {
        const PortRegion& one = in.regions[i];
        if (one.use != RegionUse::Normal || one.port != FilterKey::roi) {
            continue;
        }
        pushOne(one.index, one.object);
    }
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use != RegionUse::Normal) {
            continue;
        }
        if (!one.port.empty() && one.port != FilterKey::roi) {
            continue;
        }
        pushOne(one.index, one.roi);
    }
    for (size_t i = 1; i < rows.size(); ++i) {
        const IndexedRegion cur = rows[i];
        size_t j = i;
        while (j > 0 && rows[j - 1].index > cur.index) {
            rows[j] = rows[j - 1];
            --j;
        }
        rows[j] = cur;
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        step_rois_.push_back(rows[i].region);
    }
}

int HalFilter::proc_input(const AlgToolIn& in)
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
    if (in.param != nullptr && toolParaAs<ToolParaFilter>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    ToolParaFilter local;
    const ToolParaFilter* para = paraOf(in, &local);
    const int built = build_steps(para->run, &steps_, &err);
    if (built != HS_OK) {
        shot_err_ = built;
        shot_msg_ = err;
        return shot_err_;
    }
    collect_step_rois(in);
    return HS_OK;
}

int HalFilter::alg_run(const AlgToolIn& in)
{
    (void)in;
    HalconCpp::HObject work = image_;
    std::string err;
    for (size_t i = 0; i < steps_.size(); ++i) {
        const int ri = steps_[i].roiIndex;
        if (ri >= 0) {
            if (ri >= static_cast<int>(step_rois_.size())) {
                continue;
            }
            HalconCpp::HObject frame = fullImageRegion(img_w_, img_h_);
            HalconCpp::HObject inside;
            HalconCpp::Intersection(step_rois_[static_cast<size_t>(ri)], frame, &inside);
            if (!regionHasArea(inside)) {
                continue;
            }
            HalconCpp::HObject reduced;
            HalconCpp::ReduceDomain(work, inside, &reduced);
            if (!apply_filter_op(&reduced, steps_[i], &err)) {
                shot_err_ = HS_ERR_OPERATOR;
                shot_msg_ = "step " + std::to_string(static_cast<long long>(i)) + ": " + err;
                return shot_err_;
            }
            HalconCpp::OverpaintGray(work, reduced);
            continue;
        }
        if (!apply_filter_op(&work, steps_[i], &err)) {
            shot_err_ = HS_ERR_OPERATOR;
            shot_msg_ = "step " + std::to_string(static_cast<long long>(i)) + ": " + err;
            return shot_err_;
        }
    }
    result_image_ = work;
    shot_err_ = HS_OK;
    shot_msg_ = "图像滤波完成";
    return HS_OK;
}

int HalFilter::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    tol_.put_int(FilterKey::outWidth, img_w_);
    tol_.put_int(FilterKey::outHeight, img_h_);
    // 预处理只输出图像，无数值工艺公差；写入宽高供显示，judge 仍统一走 tol_
    tol_.judge(in.judge, nullptr);
    return HS_OK;
}

int HalFilter::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
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
        result.values.insert(FilterKey::outWidth, result.image.image.width);
        result.values.insert(FilterKey::outHeight, result.image.image.height);
        result.values.insert(FilterKey::outCostMs, run_timer_.totalMs());
        for (size_t i = 0; i < steps_.size(); ++i) {
            const int ri = steps_[i].roiIndex;
            if (ri < 0 || ri >= static_cast<int>(step_rois_.size())) {
                continue;
            }
            NamedObject roi;
            roi.port = FilterKey::roi;
            roi.index = static_cast<int>(i);
            roi.object = hObjectToRegionObject(step_rois_[static_cast<size_t>(ri)]);
            if (!roi.object.isEmpty()) {
                result.portObjects.push_back(roi);
                result.objects.push_back(roi.object);
            }
        }
    }
    *out = result;
    return shot_err_;
}

int HalFilter::action(const std::string& id, const AlgToolIn& in, AlgToolOut* out)
{
    if (id != kPreviewFilter) {
        return AlgToolBase::action(id, in, out);
    }
    return execute(in, out);
}

int HalFilter::execute(const AlgToolIn& in, AlgToolOut* out)
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
