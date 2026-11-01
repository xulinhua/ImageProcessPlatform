/**
 * @file HalCaliper.cpp
 * @brief 沿卡尺采样找边（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalCaliper.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "ParamKeys.h"
#include "ToolIo.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <cmath>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "locate.caliper";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版" },
};
constexpr const char kPortRoi[] = "roi";
}

std::string HalCaliper::tool_name() const { return kName; }
AlgLibKind HalCaliper::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalCaliper::clone() const { return new HalCaliper(); }
std::string HalCaliper::version() const { return kVer; }
std::vector<VerNote> HalCaliper::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalCaliper::HalCaliper()
    : img_w_(0)
    , img_h_(0)
    , found_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
}

HalCaliper::~HalCaliper()
{
}

int HalCaliper::tool_reset()
{
    objects_.clear();
    tol_.reset();
    HalconCpp::GenEmptyObj(&image_);
    found_ = 0;
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}

bool HalCaliper::caliper_rect(const HsCommData::HsObject& roi, double* row, double* col, double* phi,
    double* l1, double* l2) const
{
    if (row == nullptr || col == nullptr || phi == nullptr || l1 == nullptr || l2 == nullptr) {
        return false;
    }
    if (!roi.isInitialized() || roi.isEmpty()) {
        return false;
    }
    const HsCommData::ObjectKind k = roi.kind();
    if (k == HsCommData::ObjRect2) {
        const HsCommData::Rect2P* rc = roi.rect2At(0);
        *row = rc->c.y;
        *col = rc->c.x;
        *phi = rc->phi;
        *l1 = rc->l1;
        *l2 = rc->l2;
        return *l1 > 1.0 && *l2 > 0.5;
    }
    if (k == HsCommData::ObjEllipse) {
        const HsCommData::EllipseP* e = roi.ellipseAt(0);
        *row = e->c.y;
        *col = e->c.x;
        *phi = e->phi;
        *l1 = e->ra;
        *l2 = e->rb;
        return *l1 > 1.0 && *l2 > 0.5;
    }
    if (k == HsCommData::ObjRect1) {
        const HsCommData::Rect1P* rc = roi.rect1At(0);
        *col = rc->r.x + rc->r.w * 0.5;
        *row = rc->r.y + rc->r.h * 0.5;
        *phi = 0.0;
        *l1 = rc->r.w * 0.5;
        *l2 = rc->r.h * 0.5;
        return *l1 > 1.0 && *l2 > 0.5;
    }
    if (k == HsCommData::ObjLine) {
        const HsCommData::LineP* ln = roi.lineAt(0);
        *col = (ln->a.x + ln->b.x) * 0.5;
        *row = (ln->a.y + ln->b.y) * 0.5;
        *phi = std::atan2(ln->b.y - ln->a.y, ln->b.x - ln->a.x);
        const double dx = ln->b.x - ln->a.x;
        const double dy = ln->b.y - ln->a.y;
        *l1 = 0.5 * std::sqrt(dx * dx + dy * dy);
        *l2 = 8.0;
        return *l1 > 1.0;
    }
    return false;
}

int HalCaliper::proc_input(const AlgToolIn& in)
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
    if (in.param != nullptr && toolParaAs<ToolParaCaliper>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    return HS_OK;
}

int HalCaliper::alg_run(const AlgToolIn& in)
{
    ToolParaCaliper local;
    const ToolParaCaliper* para = paraOf(in, &local);
    const ToolParaCaliper::Run& rp = para->run;
    std::string trans = rp.transition;
    if (trans.empty()) {
        trans = "all";
    }
    std::string sel = rp.select;
    if (sel.empty()) {
        sel = "all";
    }
    const std::vector<HsCommData::HsObject> rois = plainRois(in.rois);
    const int toolCount = rp.searchCount;
    const bool edgeMode = rp.mode == "edge";
    int found = 0;
    double lastWidth = 0;
    double lastX = 0;
    double lastY = 0;
    objects_.clear();
    for (size_t i = 0; i < rois.size(); ++i) {
        double row = 0;
        double col = 0;
        double phi = 0;
        double l1 = 0;
        double l2 = 0;
        if (!caliper_rect(rois[i], &row, &col, &phi, &l1, &l2)) {
            continue;
        }
        HalconCpp::HTuple measure;
        HalconCpp::GenMeasureRectangle2(row, col, phi, l1, l2, img_w_, img_h_, "nearest_neighbor", &measure);
        if (edgeMode) {
            HalconCpp::HTuple rowE;
            HalconCpp::HTuple colE;
            HalconCpp::HTuple amp;
            HalconCpp::HTuple dist;
            HalconCpp::MeasurePos(image_, measure, rp.sigma, rp.threshold, trans.c_str(), sel.c_str(),
                &rowE, &colE, &amp, &dist);
            const int want = toolCount > 0 ? toolCount : 1;
            const int n = static_cast<int>(rowE.Length());
            const int take = n < want ? n : want;
            for (int k = 0; k < take; ++k) {
                const double cx = colE[k].D();
                const double cy = rowE[k].D();
                objects_.push_back(crossObject(cx, cy));
                lastX = cx;
                lastY = cy;
                ++found;
            }
        } else {
            HalconCpp::HTuple rowA;
            HalconCpp::HTuple colA;
            HalconCpp::HTuple ampA;
            HalconCpp::HTuple rowB;
            HalconCpp::HTuple colB;
            HalconCpp::HTuple ampB;
            HalconCpp::HTuple width;
            HalconCpp::HTuple intra;
            HalconCpp::MeasurePairs(image_, measure, rp.sigma, rp.threshold, trans.c_str(), sel.c_str(),
                &rowA, &colA, &ampA, &rowB, &colB, &ampB, &width, &intra);
            const int want = toolCount > 0 ? toolCount : 1;
            const int n = static_cast<int>(width.Length());
            const int take = n < want ? n : want;
            for (int k = 0; k < take; ++k) {
                objects_.push_back(lineObject(colA[k].D(), rowA[k].D(), colB[k].D(), rowB[k].D()));
                lastWidth = width[k].D();
                lastX = 0.5 * (colA[k].D() + colB[k].D());
                lastY = 0.5 * (rowA[k].D() + rowB[k].D());
                ++found;
            }
        }
        HalconCpp::CloseMeasure(measure);
    }
    found_ = found;
    if (found <= 0) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "卡尺未测到边缘";
        return shot_err_;
    }
    tol_.reset();
    tol_.put_int("count", found);
    tol_.put("width", lastWidth);
    paramSet(&tol_.values, ValueKey::X, lastX);
    paramSet(&tol_.values, ValueKey::Y, lastY);
    paramSet(&tol_.values, ValueKey::Angle, 0.0);
    paramSet(&tol_.values, ValueKey::Score, 1.0);
    shot_err_ = HS_OK;
    shot_msg_ = "卡尺完成";
    return HS_OK;
}

int HalCaliper::judge_tol(const AlgToolIn& in)
{
    tol_.judge(in.judge, nullptr);
    return HS_OK;
}

int HalCaliper::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK && found_ > 0;
    result.ok = result.runOk && tol_.ok;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK) {
        tol_.apply(&result);
        result.objects = objects_;
        for (size_t i = 0; i < objects_.size(); ++i) {
            NamedObject named;
            named.port = kPortRoi;
            named.index = static_cast<int>(i);
            named.object = objects_[i];
            result.portObjects.push_back(named);
        }
    }
    *out = result;
    return shot_err_;
}

int HalCaliper::execute(const AlgToolIn& in, AlgToolOut* out)
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
        out->ok = false;
        out->runOk = false;
        out->errCode = HS_ERR_OPERATOR;
        out->message = hExceptionText(e);
        return HS_ERR_OPERATOR;
    }
}

}
