/**
 * @file HalFindLine.cpp
 * @brief 亚像素边缘拟合直线（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalFindLine.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "ParamKeys.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <algorithm>
#include <cmath>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "locate.find_line";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版" },
};
constexpr const char kPortRoi[] = "roi";
constexpr const char kPortLine[] = "line";
constexpr const char kPortResultRoi[] = "resultRoi";
}

std::string HalFindLine::tool_name() const { return kName; }
AlgLibKind HalFindLine::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalFindLine::clone() const { return new HalFindLine(); }
std::string HalFindLine::version() const { return kVer; }
std::vector<VerNote> HalFindLine::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalFindLine::HalFindLine()
    : img_w_(0)
    , img_h_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&search_region_);
}

HalFindLine::~HalFindLine()
{
}

bool HalFindLine::longer_line(const HsCommData::LineP& a, const HsCommData::LineP& b)
{
    const double adx = a.b.x - a.a.x;
    const double ady = a.b.y - a.a.y;
    const double bdx = b.b.x - b.a.x;
    const double bdy = b.b.y - b.a.y;
    return adx * adx + ady * ady > bdx * bdx + bdy * bdy;
}

void HalFindLine::take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region)
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

int HalFindLine::tool_reset()
{
    lines_.clear();
    tol_.reset();
    HalconCpp::GenEmptyObj(&search_region_);
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}

int HalFindLine::proc_input(const AlgToolIn& in)
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
    if (in.param != nullptr && toolParaAs<ToolParaFindLine>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    take_region(in, kPortRoi, &search_region_);
    return HS_OK;
}

int HalFindLine::alg_run(const AlgToolIn& in)
{
    ToolParaFindLine local;
    const ToolParaFindLine* para = paraOf(in, &local);
    const ToolParaFindLine::Run& rp = para->run;
    const double sigma = rp.sigma < 0.4 ? 0.4 : rp.sigma;
    const int want = rp.searchCount > 0 ? rp.searchCount : 1;
    HalconCpp::HObject reduced;
    HalconCpp::HObject edges;
    HalconCpp::HObject segs;
    const HalconCpp::HObject* src = &image_;
    if (regionHasArea(search_region_)) {
        HalconCpp::ReduceDomain(image_, search_region_, &reduced);
        src = &reduced;
    }
    HalconCpp::EdgesSubPix(*src, &edges, "canny", sigma, static_cast<Hlong>(rp.low), static_cast<Hlong>(rp.high));
    HalconCpp::SegmentContoursXld(edges, &segs, "lines", 5.0, 4.0, 2.0);
    HalconCpp::HTuple segCount;
    HalconCpp::CountObj(segs, &segCount);
    const Hlong objCount = segCount.Length() > 0 ? segCount[0].L() : 0;
    std::vector<HsCommData::LineP> found;
    for (Hlong i = 1; i <= objCount; ++i) {
        HalconCpp::HObject one;
        HalconCpp::SelectObj(segs, &one, i);
        HalconCpp::HTuple length;
        HalconCpp::LengthXld(one, &length);
        if (length.Length() < 1 || length[0].D() < rp.minLength) {
            continue;
        }
        HalconCpp::HTuple rowBegin;
        HalconCpp::HTuple colBegin;
        HalconCpp::HTuple rowEnd;
        HalconCpp::HTuple colEnd;
        HalconCpp::HTuple nr;
        HalconCpp::HTuple nc;
        HalconCpp::HTuple dist;
        HalconCpp::FitLineContourXld(one, "tukey", -1, 0, 5, 2, &rowBegin, &colBegin, &rowEnd, &colEnd, &nr, &nc, &dist);
        HsCommData::LineP line;
        line.a = HsCommData::Point2d(colBegin[0].D(), rowBegin[0].D());
        line.b = HsCommData::Point2d(colEnd[0].D(), rowEnd[0].D());
        found.push_back(line);
    }
    std::sort(found.begin(), found.end(), longer_line);
    if (rp.select == "first" && !found.empty()) {
        const HsCommData::LineP keep = found.front();
        found.clear();
        found.push_back(keep);
    } else if (rp.select == "last" && !found.empty()) {
        const HsCommData::LineP keep = found.back();
        found.clear();
        found.push_back(keep);
    }
    const int take = static_cast<int>(found.size()) < want ? static_cast<int>(found.size()) : want;
    lines_.clear();
    for (int i = 0; i < take; ++i) {
        lines_.push_back(found[static_cast<size_t>(i)]);
    }
    if (lines_.empty()) {
        shot_err_ = HS_ERR_LINE_NONE;
        shot_msg_ = HsCommData::HsException(HS_ERR_LINE_NONE).errorMessage();
        return shot_err_;
    }
    shot_err_ = HS_OK;
    shot_msg_ = "找线完成";
    return HS_OK;
}

int HalFindLine::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    if (lines_.empty()) {
        return HS_OK;
    }
    const HsCommData::LineP& first = lines_.front();
    const double firstLen = std::hypot(first.b.x - first.a.x, first.b.y - first.a.y);
    double lengthSum = 0;
    double lengthMax = 0;
    for (size_t i = 0; i < lines_.size(); ++i) {
        const HsCommData::LineP& line = lines_[i];
        const double len = std::hypot(line.b.x - line.a.x, line.b.y - line.a.y);
        lengthSum += len;
        if (len > lengthMax) {
            lengthMax = len;
        }
    }
    tol_.put_int("count", static_cast<int>(lines_.size()));
    tol_.put("x", (first.a.x + first.b.x) * 0.5);
    tol_.put("y", (first.a.y + first.b.y) * 0.5);
    tol_.put("angle", std::atan2(first.b.y - first.a.y, first.b.x - first.a.x));
    tol_.put("length", firstLen);
    tol_.put("lengthSum", lengthSum);
    tol_.put("lengthMax", lengthMax);
    paramSet(&tol_.values, ValueKey::X, (first.a.x + first.b.x) * 0.5);
    paramSet(&tol_.values, ValueKey::Y, (first.a.y + first.b.y) * 0.5);
    paramSet(&tol_.values, ValueKey::Angle, std::atan2(first.b.y - first.a.y, first.b.x - first.a.x));
    paramSet(&tol_.values, ValueKey::Score, 1.0);
    tol_.judge(in.judge, nullptr);
    return HS_OK;
}

int HalFindLine::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK && !lines_.empty();
    result.ok = result.runOk && tol_.ok;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK && !lines_.empty()) {
        tol_.apply(&result);
        HalconCpp::HObject lineMask;
        bool maskFirst = true;
        for (size_t i = 0; i < lines_.size(); ++i) {
            const HsCommData::LineP& line = lines_[i];
            HsCommData::Pose2 pose;
            pose.tx = (line.a.x + line.b.x) * 0.5;
            pose.ty = (line.a.y + line.b.y) * 0.5;
            pose.setAngRad(std::atan2(line.b.y - line.a.y, line.b.x - line.a.x));
            result.poses.push_back(pose);
            NamedObject named;
            named.port = kPortLine;
            named.index = static_cast<int>(i);
            named.object = lineObject(line.a.x, line.a.y, line.b.x, line.b.y);
            result.portObjects.push_back(named);
            result.objects.push_back(named.object);
            HalconCpp::HObject strip;
            HalconCpp::GenRegionLine(&strip, line.a.y, line.a.x, line.b.y, line.b.x);
            HalconCpp::HObject thick;
            HalconCpp::DilationCircle(strip, &thick, 8.0);
            if (maskFirst) {
                lineMask = thick;
                maskFirst = false;
            } else {
                HalconCpp::HObject next;
                HalconCpp::Union2(lineMask, thick, &next);
                lineMask = next;
            }
        }
        if (!maskFirst && regionHasArea(lineMask)) {
            NamedObject region;
            region.port = kPortResultRoi;
            region.object = hObjectToRegionObject(lineMask);
            result.portObjects.push_back(region);
            result.objects.push_back(region.object);
        }
    }
    *out = result;
    return shot_err_;
}

int HalFindLine::execute(const AlgToolIn& in, AlgToolOut* out)
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
        out->errCode = HS_ERR_LINE_OPERATOR;
        out->message = hExceptionText(e);
        return HS_ERR_LINE_OPERATOR;
    }
}

}
