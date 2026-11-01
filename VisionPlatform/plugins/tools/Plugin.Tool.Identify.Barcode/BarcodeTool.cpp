/**
 * @file BarcodeTool.cpp
 * @brief 一维码识别工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "BarcodeTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};
const char kToolName[] = "identify.barcode";
const char kDisplayName[] = "一维码";
const char kDescription[] = "在搜索区里读一维码，输出文本、个数、码区和位姿";
const char kPose[] = "pose";
const char kVerdict[] = "verdict";

Vision::ParamMap defaultsFrom(const std::vector<Vision::ParamField>& fields)
{
    Vision::ParamMap m;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].type == Vision::ParamFieldType::Device) {
            continue;
        }
        m.insert(fields[i].key, fields[i].def);
    }
    return m;
}
}

Vision::BarcodeTool::BarcodeTool() {}
Vision::BarcodeTool::~BarcodeTool() {}

Vision::ToolBase* Vision::BarcodeTool::clone() const
{
    BarcodeTool* p = new BarcodeTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

std::string Vision::BarcodeTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::BarcodeTool::type() const { return Vision::ToolType::Identify; }
std::string Vision::BarcodeTool::display_name() const { return kDisplayName; }
std::string Vision::BarcodeTool::description() const { return kDescription; }
std::string Vision::BarcodeTool::version() const { return std::string(kToolVersion); }

std::vector<Vision::VerNote> Vision::BarcodeTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::vector<std::string> Vision::BarcodeTool::code_types() const
{
    std::vector<std::string> names;
    names.push_back(BarcodeKey::autoType);
    names.push_back("Code 128");
    names.push_back("Code 39");
    names.push_back("Code 93");
    names.push_back("EAN-13");
    names.push_back("EAN-8");
    names.push_back("UPC-A");
    names.push_back("2/5 Interleaved");
    return names;
}

Vision::ParamMap Vision::BarcodeTool::default_params() const
{
    return defaultsFrom(cfg_param());
}

std::vector<Vision::ParamField> Vision::BarcodeTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString(BarcodeKey::codeType, "码制", BarcodeKey::autoType, code_types()));
    f.push_back(Vision::paramString(BarcodeKey::polarity, "极性", BarcodeKey::any,
        Vision::paramChoices(BarcodeKey::any, BarcodeKey::darkOnLight, BarcodeKey::lightOnDark)));
    f.push_back(Vision::paramInt(BarcodeKey::timeout, "超时(ms)", 2000, true, 0, false, 0));
    f.push_back(Vision::paramInt(BarcodeKey::num, "最多个数", 1, true, 1, true, 99));
    return f;
}

const Vision::ToolParaBase* Vision::BarcodeTool::tool_param() const { return &param_; }

int Vision::BarcodeTool::check_param(std::string* detail) const
{
    (void)detail;
    return HS_OK;
}

Vision::OutputSpec Vision::BarcodeTool::cfg_output() const
{
    Vision::OutputSpec spec;
    Vision::OutputField code;
    code.key = BarcodeKey::code;
    code.title = BarcodeKey::code;
    spec.fields.push_back(code);
    Vision::OutputField count;
    count.key = BarcodeKey::count;
    count.title = BarcodeKey::count;
    spec.fields.push_back(count);
    return spec;
}

Vision::TolSpec Vision::BarcodeTool::cfg_tol() const
{
    Vision::TolSpec spec;
    Vision::TolField count;
    count.key = BarcodeKey::count;
    count.title = BarcodeKey::count;
    count.defMin = 1;
    count.defMax = 99;
    count.integer = true;
    spec.items.push_back(count);
    return spec;
}

Vision::DisplaySpec Vision::BarcodeTool::cfg_display() const
{
    Vision::DisplaySpec spec;
    Vision::GraphicSlot result;
    result.key = BarcodeKey::resultRoi;
    result.title = BarcodeKey::resultRoi;
    result.defEnable = true;
    result.region = true;
    spec.graphics.push_back(result);
    Vision::TextSlot code;
    code.key = BarcodeKey::code;
    code.title = BarcodeKey::code;
    code.defAnchor = Vision::TextAnchor::ResultPoint;
    code.defEnable = true;
    spec.texts.push_back(code);
    Vision::TextSlot pose;
    pose.key = kPose;
    pose.title = kPose;
    pose.defAnchor = Vision::TextAnchor::ResultPoint;
    pose.defEnable = true;
    spec.texts.push_back(pose);
    Vision::TextSlot verdict;
    verdict.key = kVerdict;
    verdict.title = kVerdict;
    verdict.defAnchor = Vision::TextAnchor::ImageCorner;
    verdict.defEnable = true;
    spec.texts.push_back(verdict);
    return spec;
}

Vision::ToolCaps Vision::BarcodeTool::cfg_roi() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = BarcodeKey::roi;
    slot.title = "搜索区";
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect2, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
    c.slotRules.push_back(slot);
    return c;
}

int Vision::BarcodeTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

void Vision::BarcodeTool::log_result(Vision::ToolOut* out) const
{
    if (out == nullptr) {
        return;
    }
    Vision::ToolTrace trace;
    trace.action = "结果";
    if (out->values.contains(BarcodeKey::code)) {
        trace.detail = out->values.value(BarcodeKey::code).toString();
    } else {
        trace.detail = out->message.empty() ? "未识别到一维码" : out->message;
    }
    out->traces.push_back(trace);
}

void Vision::BarcodeTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    ain->mode = in.mode;
    if (!in.inputIo.images.empty()) {
        ain->imagePtr = &in.inputIo.images[0].image;
        ain->imageNative = in.inputIo.images[0].native;
    } else {
        ain->imagePtr = &in.input;
        ain->imageNative = in.inputNative;
    }
    if (in.ownRoisResolved) {
        ain->rois = in.ownRois;
    } else {
        for (size_t i = 0; i < in.rois.size(); ++i) {
            Vision::PortRoi one;
            one.roi = in.rois[i];
            one.port = BarcodeKey::roi;
            one.index = static_cast<int>(i);
            ain->rois.push_back(one);
        }
    }
    ain->param = &param_;
    ain->run = in.param.run;
    ain->teach = in.param.learn;
    ain->judge = in.param.judge;
    ain->solutionDir = in.solutionDir;
    ain->toolDataDir = in.toolDataDir;
}

int Vision::BarcodeTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = Vision::ToolOut();
    attachAlg(in.toolDataDir);
    param_.load(in.param);
    Vision::AlgToolBase* alg = attachedAlg();
    if (alg == nullptr) {
        out->ok = false;
        out->errCode = HS_ERR_NO_ALG;
        out->message = "Algorithm not registered";
        return out->errCode;
    }
    Vision::AlgToolIn ain;
    fill_alg_in(in, &ain);
    Vision::AlgToolOut algOut;
    alg->shot_init(ain, &algOut);
    const int code = actionId.empty()
        ? alg->run(Vision::ExecMode::Auto, ain, &algOut)
        : alg->action(actionId, ain, &algOut);
    if (code != HS_OK && algOut.errCode == HS_OK) {
        algOut.errCode = code;
        algOut.ok = false;
    }
    out->ok = algOut.ok;
    out->errCode = algOut.errCode;
    out->error = algOut.errCode != HS_OK;
    out->message = algOut.message;
    out->values = algOut.values;
    out->objects = algOut.objects;
    out->traces = algOut.traces;
    for (size_t i = 0; i < algOut.poses.size(); ++i) {
        Vision::PortPose one;
        one.index = static_cast<int>(i);
        one.pose = algOut.poses[i];
        out->io.poses.push_back(one);
    }
    out->syncIo();
    log_result(out);
    if (!Vision::toolTypeIsImageSource(type())) {
        out->io.images.clear();
    }
    return out->errCode;
}

std::vector<Vision::ParamField> Vision::BarcodeTool::paramFields() const { return cfg_param(); }

Vision::ToolOut Vision::BarcodeTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
