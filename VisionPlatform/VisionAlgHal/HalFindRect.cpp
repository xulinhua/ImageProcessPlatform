/**
 * @file HalFindRect.cpp
 * @brief 找旋转矩形（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalFindRect.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "ParamKeys.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <cmath>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "locate.find_rect";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalFindRect::tool_name() const { return kName; }
AlgLibKind HalFindRect::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalFindRect::clone() const { return new HalFindRect(); }
std::string HalFindRect::version() const { return kVer; }
std::vector<VerNote> HalFindRect::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalFindRect::HalFindRect()
    : found_(false)
    , img_w_(0)
    , img_h_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&search_region_);
}

HalFindRect::~HalFindRect()
{
}

void HalFindRect::take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region)
{
    HalconCpp::GenEmptyObj(region);
    const std::string name = port != nullptr ? port : "";
    const bool search = name == FindRectKey::roi;
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
            || (search && (one.port.empty() || one.port == FindRectKey::roi));
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

bool HalFindRect::fit_one(const HalconCpp::HObject& contour, Fit* fit) const
{
    if (fit == nullptr) {
        return false;
    }
    HalconCpp::HTuple row;
    HalconCpp::HTuple col;
    HalconCpp::HTuple phi;
    HalconCpp::HTuple len1;
    HalconCpp::HTuple len2;
    HalconCpp::HTuple order;
    HalconCpp::HTuple dist;
    try {
        HalconCpp::FitRectangle2ContourXld(contour, "tukey", -1, 0, 0, 3, 2, &row, &col, &phi, &len1, &len2, &order);
        if (row.Length() < 1) {
            return false;
        }
        HalconCpp::DistRectangle2ContourPointsXld(contour, 0, row[0], col[0], phi[0], len1[0], len2[0], &dist);
    } catch (const HalconCpp::HException&) {
        return false;
    }
    double sum = 0;
    const Hlong n = dist.Length();
    for (Hlong i = 0; i < n; ++i) {
        sum += std::fabs(dist[i].D());
    }
    fit->row = row[0].D();
    fit->col = col[0].D();
    fit->phi = phi[0].D();
    fit->len1 = len1[0].D();
    fit->len2 = len2[0].D();
    fit->dist = n > 0 ? sum / static_cast<double>(n) : 0.0;
    return true;
}

int HalFindRect::tool_reset()
{
    tol_.reset();
    HalconCpp::GenEmptyObj(&search_region_);
    best_ = Fit();
    found_ = false;
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}

int HalFindRect::proc_input(const AlgToolIn& in)
{
    HalconCpp::HObject gray;
    int w = 0;
    int h = 0;
    std::string err;
    HalconCpp::HObject* pGray = &gray;
    if (!inputToGray(in.imageNative.ptr, static_cast<int>(in.imageNative.algLib), inputImage(in), pGray, &w, &h, &err)) {
        shot_err_ = HS_ERR_IMAGE_EMPTY;
        shot_msg_ = err.empty() ? HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage() : err;
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
    if (in.param != nullptr && toolParaAs<ToolParaFindRect>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    take_region(in, FindRectKey::roi, &search_region_);
    return HS_OK;
}

int HalFindRect::alg_run(const AlgToolIn& in)
{
    ToolParaFindRect local;
    const ToolParaFindRect* para = paraOf(in, &local);
    HalconCpp::HObject reduced;
    const HalconCpp::HObject* src = &image_;
    if (regionHasArea(search_region_)) {
        HalconCpp::ReduceDomain(image_, search_region_, &reduced);
        src = &reduced;
    }
    HalconCpp::HObject edges;
    HalconCpp::EdgesSubPix(*src, &edges, "canny", para->run.sigma,
        static_cast<Hlong>(para->run.low), static_cast<Hlong>(para->run.high));
    HalconCpp::HObject joined;
    HalconCpp::UnionAdjacentContoursXld(edges, &joined, 10, 1, "attr_keep");
    HalconCpp::HObject candidates;
    HalconCpp::SelectContoursXld(joined, &candidates, "contour_length", 4.0 * para->run.minSide, 1e9, -0.5, 0.5);
    HalconCpp::HTuple n;
    HalconCpp::CountObj(candidates, &n);
    const Hlong count = n.Length() > 0 ? n[0].L() : 0;
    Fit best;
    bool found = false;
    for (Hlong i = 1; i <= count; ++i) {
        HalconCpp::HObject one;
        HalconCpp::SelectObj(candidates, &one, i);
        Fit fit;
        if (!fit_one(one, &fit)) {
            continue;
        }
        const double shortSide = 2.0 * (fit.len1 < fit.len2 ? fit.len1 : fit.len2);
        const double longSide = 2.0 * (fit.len1 > fit.len2 ? fit.len1 : fit.len2);
        if (shortSide < para->run.minSide || fit.dist > para->run.maxDist) {
            continue;
        }
        if (para->run.maxSide > 0.0 && longSide > para->run.maxSide) {
            continue;
        }
        if (!found || fit.len1 * fit.len2 > best.len1 * best.len2) {
            best = fit;
            found = true;
        }
    }
    found_ = found;
    best_ = best;
    if (!found) {
        shot_err_ = HS_ERR_RECT_NONE;
        shot_msg_ = HsCommData::HsException(HS_ERR_RECT_NONE).errorMessage();
        return shot_err_;
    }
    shot_err_ = HS_OK;
    shot_msg_ = "找矩形完成";
    return HS_OK;
}

int HalFindRect::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    if (!found_) {
        return HS_OK;
    }
    paramSet(&tol_.values, ValueKey::X, best_.col);
    paramSet(&tol_.values, ValueKey::Y, best_.row);
    paramSet(&tol_.values, ValueKey::Angle, best_.phi);
    paramSet(&tol_.values, ValueKey::Score, 1.0 / (1.0 + best_.dist));
    tol_.put(FindRectKey::width, 2.0 * best_.len1);
    tol_.put(FindRectKey::height, 2.0 * best_.len2);
    tol_.put(FindRectKey::dist, best_.dist);
    ToolParaFindRect local;
    const ToolParaFindRect* para = paraOf(in, &local);
    tol_.judge(para->judge, nullptr);
    return HS_OK;
}

int HalFindRect::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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
        pose.tx = best_.col;
        pose.ty = best_.row;
        pose.setAngRad(best_.phi);
        result.poses.push_back(pose);
        HsCommData::Rect2P rect;
        rect.c = HsCommData::Point2d(best_.col, best_.row);
        rect.l1 = best_.len1;
        rect.l2 = best_.len2;
        rect.phi = best_.phi;
        result.objects.push_back(HsCommData::HsObject(rect));
        result.objects.push_back(crossObject(best_.col, best_.row));
        NamedObject named;
        named.port = FindRectKey::roi;
        named.object = HsCommData::HsObject(rect);
        result.portObjects.push_back(named);
        if (!result.ok) {
            result.message = "找矩形公差未通过";
        }
    }
    *out = result;
    return shot_err_;
}

int HalFindRect::execute(const AlgToolIn& in, AlgToolOut* out)
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
        out->errCode = HS_ERR_RECT_OPERATOR;
        out->message = hExceptionText(e);
        return HS_ERR_RECT_OPERATOR;
    }
}

}
