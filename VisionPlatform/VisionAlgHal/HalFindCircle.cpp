/**
 * @file HalFindCircle.cpp
 * @brief 亚像素边缘拟合圆（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalFindCircle.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "ParamKeys.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "locate.find_circle";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版" },
};
constexpr const char kPortRoi[] = "roi";
}

std::string HalFindCircle::tool_name() const { return kName; }
AlgLibKind HalFindCircle::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalFindCircle::clone() const { return new HalFindCircle(); }
std::string HalFindCircle::version() const { return kVer; }
std::vector<VerNote> HalFindCircle::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalFindCircle::HalFindCircle()
    : found_(false)
    , cx_(0)
    , cy_(0)
    , radius_(0)
    , img_w_(0)
    , img_h_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&search_region_);
}

HalFindCircle::~HalFindCircle()
{
}

void HalFindCircle::take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region)
{
    HalconCpp::GenEmptyObj(region);
    const std::string name = port != nullptr ? port : "";
    const bool search = name == kPortRoi;
    HalconCpp::HObject normals;
    HalconCpp::HObject masks;
    bool anyNormal = false;
    bool anyMask = false;
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use == RegionUse::Invalid) {
            continue;
        }
        const bool hit = one.port == name
            || (search && (one.port.empty() || one.port == kPortRoi));
        if (!hit) {
            continue;
        }
        HalconCpp::HObject piece;
        try {
            piece = hsObjectToHObject(one.roi, img_w_, img_h_);
        } catch (const HalconCpp::HException&) {
            continue;
        }
        if (hoIsEmpty(piece)) {
            continue;
        }
        HalconCpp::HObject* acc = one.use == RegionUse::Mask ? &masks : &normals;
        bool* any = one.use == RegionUse::Mask ? &anyMask : &anyNormal;
        if (!*any) {
            *acc = piece;
            *any = true;
        } else {
            HalconCpp::HObject joined;
            HalconCpp::ConcatObj(*acc, piece, &joined);
            *acc = joined;
        }
    }
    if (!anyNormal) {
        return;
    }
    HalconCpp::HObject merged;
    HalconCpp::Union1(normals, &merged);
    if (anyMask) {
        HalconCpp::HObject cover;
        HalconCpp::Union1(masks, &cover);
        HalconCpp::HObject left;
        HalconCpp::Difference(merged, cover, &left);
        merged = left;
    }
    *region = merged;
}

int HalFindCircle::tool_reset()
{
    tol_.reset();
    HalconCpp::GenEmptyObj(&search_region_);
    found_ = false;
    cx_ = 0;
    cy_ = 0;
    radius_ = 0;
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}

int HalFindCircle::proc_input(const AlgToolIn& in)
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
    if (in.param != nullptr && toolParaAs<ToolParaFindCircle>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    take_region(in, kPortRoi, &search_region_);
    return HS_OK;
}

int HalFindCircle::alg_run(const AlgToolIn& in)
{
    ToolParaFindCircle local;
    const ToolParaFindCircle* para = paraOf(in, &local);
    const ToolParaFindCircle::Run& rp = para->run;
    const double sigma = rp.sigma < 0.4 ? 0.4 : rp.sigma;
    const double minR = static_cast<double>(rp.minR);
    const double maxR = static_cast<double>(rp.maxR);
    HalconCpp::HObject reduced;
    const HalconCpp::HObject* src = &image_;
    if (regionHasArea(search_region_)) {
        HalconCpp::ReduceDomain(image_, search_region_, &reduced);
        src = &reduced;
    }
    HalconCpp::HObject edges;
    HalconCpp::EdgesSubPix(*src, &edges, "canny", sigma, static_cast<Hlong>(rp.low), static_cast<Hlong>(rp.high));
    HalconCpp::HObject segs;
    HalconCpp::SegmentContoursXld(edges, &segs, "lines_circles", 5.0, 4.0, 2.0);
    HalconCpp::HTuple n;
    HalconCpp::CountObj(segs, &n);
    const Hlong nObj = n.Length() > 0 ? n[0].L() : 0;
    double bestR = 0;
    double bestCx = 0;
    double bestCy = 0;
    bool found = false;
    for (Hlong i = 1; i <= nObj; ++i) {
        HalconCpp::HObject one;
        HalconCpp::SelectObj(segs, &one, i);
        HalconCpp::HTuple row;
        HalconCpp::HTuple col;
        HalconCpp::HTuple radius;
        HalconCpp::HTuple startPhi;
        HalconCpp::HTuple endPhi;
        HalconCpp::HTuple pointOrder;
        try {
            HalconCpp::FitCircleContourXld(one, "algebraic", -1, 0.0, 0, 3, 2.0, &row, &col, &radius,
                &startPhi, &endPhi, &pointOrder);
        } catch (const HalconCpp::HException&) {
            continue;
        }
        if (radius.Length() < 1 || col.Length() < 1 || row.Length() < 1) {
            continue;
        }
        const double rVal = radius[0].D();
        if (rVal < minR || rVal > maxR) {
            continue;
        }
        if (!found || rVal > bestR) {
            found = true;
            bestR = rVal;
            bestCx = col[0].D();
            bestCy = row[0].D();
        }
    }
    found_ = found;
    cx_ = bestCx;
    cy_ = bestCy;
    radius_ = bestR;
    if (!found) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "未找到圆";
        return shot_err_;
    }
    tol_.reset();
    tol_.put("cx", bestCx);
    tol_.put("cy", bestCy);
    tol_.put("r", bestR);
    paramSet(&tol_.values, ValueKey::X, bestCx);
    paramSet(&tol_.values, ValueKey::Y, bestCy);
    paramSet(&tol_.values, ValueKey::Angle, 0.0);
    paramSet(&tol_.values, ValueKey::Score, 1.0);
    paramSet(&tol_.values, ValueKey::R, bestR);
    shot_err_ = HS_OK;
    shot_msg_ = "找圆完成";
    return HS_OK;
}

int HalFindCircle::judge_tol(const AlgToolIn& in)
{
    tol_.judge(in.judge, nullptr);
    return HS_OK;
}

int HalFindCircle::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK && found_;
    result.ok = result.runOk && tol_.ok;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK && found_) {
        tol_.apply(&result);
        HsCommData::Pose2 pose;
        pose.tx = cx_;
        pose.ty = cy_;
        pose.setAngRad(0.0);
        result.poses.push_back(pose);
        HsCommData::HsObject circle = circleObject(cx_, cy_, radius_);
        result.objects.push_back(circle);
        NamedObject named;
        named.port = kPortRoi;
        named.object = circle;
        result.portObjects.push_back(named);
        result.objects.push_back(crossObject(cx_, cy_));
    }
    *out = result;
    return shot_err_;
}

int HalFindCircle::execute(const AlgToolIn& in, AlgToolOut* out)
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
