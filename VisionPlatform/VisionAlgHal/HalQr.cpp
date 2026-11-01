/**
 * @file HalQr.cpp
 * @brief 二维码识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalQr.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "HalTupleConvert.h"
#include "ParamKeys.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "identify.qr";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalQr::tool_name() const { return kName; }
AlgLibKind HalQr::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalQr::clone() const { return new HalQr(); }
std::string HalQr::version() const { return kVer; }
std::vector<VerNote> HalQr::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalQr::QrModel::QrModel() : open(false) {}

HalQr::QrModel::~QrModel()
{
    if (!open) {
        return;
    }
    try {
        HalconCpp::ClearDataCode2dModel(id);
    } catch (const HalconCpp::HException&) {
    }
}

HalQr::HalQr()
    : img_w_(0)
    , img_h_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&search_region_);
    HalconCpp::GenEmptyObj(&symbol_region_);
}

HalQr::~HalQr()
{
}

int HalQr::tool_reset()
{
    codes_.clear();
    poses_.clear();
    tol_.reset();
    shot_err_ = 0;
    shot_msg_.clear();
    HalconCpp::GenEmptyObj(&symbol_region_);
    return HS_OK;
}

void HalQr::take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region)
{
    HalconCpp::GenEmptyObj(region);
    const std::string name = port != nullptr ? port : "";
    HalconCpp::HObject normals;
    HalconCpp::HObject masks;
    bool anyNormal = false;
    bool anyMask = false;
    for (size_t i = 0; i < in.rois.size(); ++i) {
        const PortRoi& one = in.rois[i];
        if (one.use == RegionUse::Invalid) {
            continue;
        }
        if (one.port != name) {
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

std::string HalQr::polarity_of(const std::string& raw) const
{
    if (raw == QrKey::darkOnLight || raw == QrKey::lightOnDark) {
        return raw;
    }
    return QrKey::any;
}

int HalQr::proc_input(const AlgToolIn& in)
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
    if (toolParaAs<ToolParaQr>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    take_region(in, QrKey::roi, &search_region_);
    return HS_OK;
}

int HalQr::alg_run(const AlgToolIn& in)
{
    const ToolParaQr* para = toolParaAs<ToolParaQr>(in.param);
    if (para == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    int num = para->run.num;
    if (num < 1) {
        num = 1;
    }
    if (num > 99) {
        num = 99;
    }
    int timeout = para->run.timeout;
    if (timeout < 0) {
        timeout = 0;
    }
    QrModel model;
    HalconCpp::CreateDataCode2dModel("QR Code", HalconCpp::HTuple(), HalconCpp::HTuple(), &model.id);
    model.open = model.id.Length() > 0;
    HalconCpp::SetDataCode2dParam(model.id, "default_parameters", "enhanced_recognition");
    HalconCpp::SetDataCode2dParam(model.id, "polarity", polarity_of(para->run.polarity).c_str());
    HalconCpp::SetDataCode2dParam(model.id, "timeout", timeout);
    if (para->run.moduleMin > 0.5) {
        HalconCpp::SetDataCode2dParam(model.id, "module_size_min", para->run.moduleMin);
    }
    const HalconCpp::HObject* src = &image_;
    HalconCpp::HObject reduced;
    if (regionHasArea(search_region_)) {
        HalconCpp::ReduceDomain(image_, search_region_, &reduced);
        src = &reduced;
    }
    HalconCpp::HObject symbols;
    HalconCpp::HTuple handles;
    HalconCpp::HTuple decoded;
    HalconCpp::FindDataCode2d(*src, &symbols, model.id, "stop_after_result_num", num, &handles, &decoded);
    const int found = static_cast<int>(decoded.Length());
    if (found < 1) {
        shot_err_ = HS_ERR_QR_NONE;
        shot_msg_ = HsCommData::HsException(HS_ERR_QR_NONE).errorMessage();
        return shot_err_;
    }
    HalconCpp::HTuple orient;
    try {
        HalconCpp::GetDataCode2dResults(model.id, handles, "orientation", &orient);
    } catch (const HalconCpp::HException&) {
        orient = HalconCpp::HTuple();
    }
    HalconCpp::HTuple row;
    HalconCpp::HTuple col;
    HalconCpp::HTuple phi;
    HalconCpp::HTuple len1;
    HalconCpp::HTuple len2;
    try {
        HalconCpp::SmallestRectangle2Xld(symbols, &row, &col, &phi, &len1, &len2);
    } catch (const HalconCpp::HException&) {
        row = HalconCpp::HTuple();
    }
    HalconCpp::HObject filled;
    HalconCpp::GenRegionContourXld(symbols, &filled, "filled");
    HalconCpp::Union1(filled, &symbol_region_);
    for (int i = 0; i < found; ++i) {
        codes_.push_back(hTupleText(decoded, i));
        HsCommData::Pose2 pose;
        if (col.Length() > i && row.Length() > i) {
            pose.tx = col[i].D();
            pose.ty = row[i].D();
        }
        if (orient.Length() > i) {
            pose.setAngRad(orient[i].D());
        } else if (phi.Length() > i) {
            pose.setAngRad(phi[i].D());
        }
        poses_.push_back(pose);
    }
    shot_err_ = HS_OK;
    shot_msg_ = "QR done";
    return HS_OK;
}

int HalQr::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    std::string joined;
    for (size_t i = 0; i < codes_.size(); ++i) {
        if (!joined.empty()) {
            joined += "; ";
        }
        joined += codes_[i];
    }
    tol_.put_text(QrKey::code, joined);
    tol_.put_int(QrKey::count, static_cast<int>(codes_.size()));
    if (!poses_.empty()) {
        tol_.put("x", poses_[0].tx);
        tol_.put("y", poses_[0].ty);
        tol_.put("angle", poses_[0].angle);
        paramSet(&tol_.values, ValueKey::X, poses_[0].tx);
        paramSet(&tol_.values, ValueKey::Y, poses_[0].ty);
        paramSet(&tol_.values, ValueKey::Angle, poses_[0].angRad());
    }
    tol_.judge(in.judge, nullptr);
    return HS_OK;
}

int HalQr::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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
        result.poses = poses_;
        if (regionHasArea(symbol_region_)) {
            NamedObject symbol;
            symbol.port = QrKey::resultRoi;
            symbol.object = hObjectToRegionObject(symbol_region_);
            result.portObjects.push_back(symbol);
            result.objects.push_back(symbol.object);
        }
        if (!result.ok && shot_err_ == HS_OK) {
            result.message = "QR out of tolerance";
        }
    }
    *out = result;
    return shot_err_;
}

int HalQr::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_TYPE;
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
            out->errCode = HS_ERR_QR_OPERATOR;
            out->message = hExceptionText(e);
        }
        return HS_ERR_QR_OPERATOR;
    }
}

void HalQr::init(const std::string&)
{
}

void HalQr::save(const std::string&)
{
}

}
