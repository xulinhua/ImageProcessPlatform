/**
 * @file HalTexture.cpp
 * @brief 大核背景差纹理缺陷（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalTexture.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "detect.texture";
const char kRoiPort[] = "roi";
const char kResultRoi[] = "resultRoi";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalTexture::tool_name() const { return kName; }
AlgLibKind HalTexture::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalTexture::clone() const { return new HalTexture(); }
std::string HalTexture::version() const { return kVer; }
std::vector<VerNote> HalTexture::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalTexture::HalTexture()
    : img_w_(0)
    , img_h_(0)
    , blob_count_(0)
    , blob_area_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&roi_region_);
    HalconCpp::GenEmptyObj(&result_region_);
}

HalTexture::~HalTexture()
{
}

void HalTexture::init(const std::string&)
{
}

void HalTexture::save(const std::string&)
{
}

int HalTexture::tool_reset()
{
    blob_count_ = 0;
    blob_area_ = 0;
    tol_.reset();
    shot_err_ = HS_OK;
    shot_msg_.clear();
    HalconCpp::GenEmptyObj(&result_region_);
    return HS_OK;
}

void HalTexture::take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region)
{
    HalconCpp::GenEmptyObj(region);
    const std::string name = port != nullptr ? port : "";
    HalconCpp::HObject normals;
    HalconCpp::HObject masks;
    bool anyNormal = false;
    bool anyMask = false;
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use == RegionUse::Invalid || one.port != name) {
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
    for (size_t i = 0; i < in.regions.size(); ++i) {
        const PortRegion& one = in.regions[i];
        if (one.use == RegionUse::Invalid || one.port != name) {
            continue;
        }
        HalconCpp::HObject piece;
        try {
            piece = hsObjectToHObject(one.object, img_w_, img_h_);
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

int HalTexture::proc_input(const AlgToolIn& in)
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
    if (in.param != nullptr && toolParaAs<ToolParaTexture>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    take_region(in, kRoiPort, &roi_region_);
    return HS_OK;
}

int HalTexture::alg_run(const AlgToolIn& in)
{
    ToolParaTexture paraLocal;
    const ToolParaTexture* para = paraOf(in, &paraLocal);
    const ToolParaTexture::Run& rp = para->run;
    const HalconCpp::HObject* src = &image_;
    HalconCpp::HObject reduced;
    if (regionHasArea(roi_region_)) {
        HalconCpp::ReduceDomain(image_, roi_region_, &reduced);
        src = &reduced;
    }
    int kw = rp.kernelW;
    int kh = rp.kernelH;
    if (kw < 3) {
        kw = 3;
    }
    if (kh < 3) {
        kh = 3;
    }
    HalconCpp::HObject bg;
    if (rp.bgMethod == "median") {
        HalconCpp::MedianRect(*src, &bg, kw, kh);
    } else if (rp.bgMethod == "closing") {
        HalconCpp::GrayClosingRect(*src, &bg, kh, kw);
    } else {
        HalconCpp::MeanImage(*src, &bg, kw, kh);
    }
    HalconCpp::HObject diff;
    HalconCpp::AbsDiffImage(*src, bg, &diff, 1.0);
    HalconCpp::HObject bin;
    HalconCpp::Threshold(diff, &bin, rp.residThresh, 255.0);
    HalconCpp::HObject morphed = bin;
    if (rp.openR > 0.4) {
        HalconCpp::HObject opened;
        HalconCpp::OpeningCircle(morphed, &opened, rp.openR);
        morphed = opened;
    }
    HalconCpp::HObject conn;
    HalconCpp::Connection(morphed, &conn);
    HalconCpp::HObject sel;
    const double areaHi = rp.maxArea > 0.0 ? static_cast<double>(rp.maxArea) : 1.0e12;
    HalconCpp::SelectShape(conn, &sel, "area", "and", static_cast<double>(rp.minArea), areaHi);
    HalconCpp::HTuple nSel;
    HalconCpp::CountObj(sel, &nSel);
    if (nSel.Length() < 1 || nSel[0].L() < 1) {
        shot_err_ = HS_OK;
        shot_msg_ = "未找到连通域";
        blob_count_ = 0;
        blob_area_ = 0;
        HalconCpp::GenEmptyObj(&result_region_);
        return HS_OK;
    }
    HalconCpp::HTuple area;
    HalconCpp::HTuple row;
    HalconCpp::HTuple col;
    HalconCpp::AreaCenter(sel, &area, &row, &col);
    blob_count_ = static_cast<int>(nSel[0].L());
    blob_area_ = area.TupleSum().D();
    result_region_ = sel;
    shot_err_ = HS_OK;
    shot_msg_ = "纹理缺陷完成";
    return HS_OK;
}

int HalTexture::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    tol_.put_int("count", blob_count_);
    tol_.put("area", blob_area_);
    // 本工具只输出缺陷区域，无数值型工艺公差域；仍走 judge 以便 count/area 进判定表
    tol_.judge(in.judge, nullptr);
    return HS_OK;
}

int HalTexture::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_TYPE;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK;
    result.ok = shot_err_ == HS_OK && blob_count_ > 0 && tol_.ok;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK) {
        tol_.apply(&result);
        if (regionHasArea(result_region_)) {
            NamedObject named;
            named.port = kResultRoi;
            named.object = hObjectToRegionObject(result_region_);
            result.portObjects.push_back(named);
            result.objects.push_back(named.object);
        }
    }
    *out = result;
    return shot_err_;
}

int HalTexture::execute(const AlgToolIn& in, AlgToolOut* out)
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
