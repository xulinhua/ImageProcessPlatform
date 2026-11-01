/**
 * @file HalOcr.cpp
 * @brief 字符识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalOcr.h"
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
const char kName[] = "identify.ocr";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalOcr::tool_name() const { return kName; }
AlgLibKind HalOcr::alg_lib() const { return AlgLibHal; }
AlgToolBase* HalOcr::clone() const { return new HalOcr(); }
std::string HalOcr::version() const { return kVer; }
std::vector<VerNote> HalOcr::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

HalOcr::TextModel::TextModel() : open(false) {}

HalOcr::TextModel::~TextModel()
{
    if (!open) {
        return;
    }
    try {
        HalconCpp::ClearTextModel(id);
    } catch (const HalconCpp::HException&) {
    }
}

HalOcr::TextResult::TextResult() : open(false) {}

HalOcr::TextResult::~TextResult()
{
    if (!open) {
        return;
    }
    try {
        HalconCpp::ClearTextResult(id);
    } catch (const HalconCpp::HException&) {
    }
}

HalOcr::HalOcr()
    : char_count_(0)
    , score_(0)
    , text_match_(true)
    , img_w_(0)
    , img_h_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&search_region_);
    HalconCpp::GenEmptyObj(&symbol_region_);
}

HalOcr::~HalOcr()
{
}

int HalOcr::tool_reset()
{
    text_.clear();
    pose_ = HsCommData::Pose2();
    char_count_ = 0;
    score_ = 0;
    text_match_ = true;
    tol_.reset();
    shot_err_ = 0;
    shot_msg_.clear();
    HalconCpp::GenEmptyObj(&symbol_region_);
    return HS_OK;
}

void HalOcr::take_region(const AlgToolIn& in, const char* port, HalconCpp::HObject* region)
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

int HalOcr::proc_input(const AlgToolIn& in)
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
    if (toolParaAs<ToolParaOcr>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    take_region(in, OcrKey::roi, &search_region_);
    return HS_OK;
}

int HalOcr::alg_run(const AlgToolIn& in)
{
    const ToolParaOcr* para = toolParaAs<ToolParaOcr>(in.param);
    if (para == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    const HalconCpp::HObject* src = &image_;
    HalconCpp::HObject reduced;
    if (regionHasArea(search_region_)) {
        HalconCpp::ReduceDomain(image_, search_region_, &reduced);
        src = &reduced;
    }
    TextModel model;
    try {
        HalconCpp::CreateTextModelReader("auto", para->run.font.c_str(), &model.id);
    } catch (const HalconCpp::HException& e) {
        shot_err_ = HS_ERR_OCR_NO_MODEL;
        shot_msg_ = para->run.font + ": " + hExceptionText(e);
        return shot_err_;
    }
    model.open = true;
    HalconCpp::SetTextModelParam(model.id, "polarity", para->run.polarity.c_str());
    if (para->run.minCharHeight > 0) {
        HalconCpp::SetTextModelParam(model.id, "min_char_height", para->run.minCharHeight);
    }
    if (para->run.maxCharHeight > 0) {
        HalconCpp::SetTextModelParam(model.id, "max_char_height", para->run.maxCharHeight);
    }
    TextResult ocrResult;
    HalconCpp::FindText(*src, model.id, &ocrResult.id);
    ocrResult.open = true;
    HalconCpp::HTuple classes;
    HalconCpp::GetTextResult(ocrResult.id, "class", &classes);
    const int found = static_cast<int>(classes.Length());
    if (found < 1) {
        shot_err_ = HS_ERR_OCR_NONE;
        shot_msg_ = HsCommData::HsException(HS_ERR_OCR_NONE).errorMessage();
        return shot_err_;
    }
    text_.clear();
    for (int i = 0; i < found; ++i) {
        text_ += hTupleText(classes, i);
    }
    char_count_ = found;
    HalconCpp::HTuple confidence;
    try {
        HalconCpp::GetTextResult(ocrResult.id, "confidence", &confidence);
    } catch (const HalconCpp::HException&) {
        confidence = HalconCpp::HTuple();
    }
    score_ = 0;
    for (Hlong i = 0; i < confidence.Length(); ++i) {
        score_ += confidence[i].D();
    }
    score_ = confidence.Length() > 0 ? score_ / static_cast<double>(confidence.Length()) : 0.0;
    HalconCpp::HObject chars;
    HalconCpp::GetTextObject(&chars, ocrResult.id, "all_lines");
    HalconCpp::Union1(chars, &symbol_region_);
    pose_ = HsCommData::Pose2();
    if (regionHasArea(symbol_region_)) {
        HalconCpp::HTuple row;
        HalconCpp::HTuple col;
        HalconCpp::HTuple phi;
        HalconCpp::HTuple len1;
        HalconCpp::HTuple len2;
        HalconCpp::SmallestRectangle2(symbol_region_, &row, &col, &phi, &len1, &len2);
        pose_.tx = col[0].D();
        pose_.ty = row[0].D();
        pose_.setAngRad(phi[0].D());
    }
    shot_err_ = HS_OK;
    shot_msg_ = "OCR done";
    return HS_OK;
}

int HalOcr::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    tol_.put_text(OcrKey::text, text_);
    tol_.put_int(OcrKey::count, char_count_);
    tol_.put("x", pose_.tx);
    tol_.put("y", pose_.ty);
    tol_.put("angle", pose_.angle);
    paramSet(&tol_.values, ValueKey::X, pose_.tx);
    paramSet(&tol_.values, ValueKey::Y, pose_.ty);
    paramSet(&tol_.values, ValueKey::Angle, pose_.angRad());
    paramSet(&tol_.values, ValueKey::Score, score_);
    tol_.judge(in.judge, nullptr);
    const ToolParaOcr* para = toolParaAs<ToolParaOcr>(in.param);
    text_match_ = para == nullptr || para->run.expected.empty() || para->run.expected == text_;
    if (!text_match_) {
        tol_.ok = false;
    }
    return HS_OK;
}

int HalOcr::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_TYPE;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK;
    result.ok = shot_err_ == HS_OK && tol_.ok && text_match_;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK) {
        tol_.apply(&result);
        result.poses.push_back(pose_);
        ResultLabel label;
        label.x = pose_.tx;
        label.y = pose_.ty;
        label.text = text_;
        result.labels.push_back(label);
        if (regionHasArea(symbol_region_)) {
            NamedObject item;
            item.port = OcrKey::resultRoi;
            item.object = hObjectToRegionObject(symbol_region_);
            result.portObjects.push_back(item);
            result.objects.push_back(item.object);
        }
        if (!text_match_) {
            result.message = "text mismatch: " + text_;
        } else if (!result.ok) {
            result.message = "OCR out of tolerance";
        }
    }
    *out = result;
    return shot_err_;
}

int HalOcr::execute(const AlgToolIn& in, AlgToolOut* out)
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
            out->errCode = HS_ERR_OCR_OPERATOR;
            out->message = hExceptionText(e);
        }
        return HS_ERR_OCR_OPERATOR;
    }
}

void HalOcr::init(const std::string&)
{
}

void HalOcr::save(const std::string&)
{
}

}
