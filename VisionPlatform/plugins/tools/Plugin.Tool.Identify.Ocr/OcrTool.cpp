/**
 * @file OcrTool.cpp
 * @brief 字符识别工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "OcrTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};
const char kToolName[] = "identify.ocr";
const char kDisplayName[] = "字符识别";
const char kDescription[] = "在搜索区里读字符，输出文本、字符数、字符区和位姿；填了期望文本则比对";
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

Vision::OcrTool::OcrTool() {}
Vision::OcrTool::~OcrTool() {}

Vision::ToolBase* Vision::OcrTool::clone() const
{
    OcrTool* p = new OcrTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

std::string Vision::OcrTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::OcrTool::type() const { return Vision::ToolType::Identify; }
std::string Vision::OcrTool::display_name() const { return kDisplayName; }
std::string Vision::OcrTool::description() const { return kDescription; }
std::string Vision::OcrTool::version() const { return std::string(kToolVersion); }

std::vector<Vision::VerNote> Vision::OcrTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::vector<std::string> Vision::OcrTool::fonts() const
{
    std::vector<std::string> names;
    names.push_back(OcrKey::defaultFont);
    names.push_back("Universal_0-9_NoRej.occ");
    names.push_back("Industrial_0-9A-Z_NoRej.omc");
    names.push_back("Document_0-9A-Z_NoRej.omc");
    names.push_back("DotPrint_0-9A-Z_NoRej.omc");
    names.push_back("SEMI_NoRej.omc");
    return names;
}

Vision::ParamMap Vision::OcrTool::default_params() const { return defaultsFrom(cfg_param()); }

std::vector<Vision::ParamField> Vision::OcrTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString(OcrKey::font, "字体", OcrKey::defaultFont, fonts()));
    f.push_back(Vision::paramString(OcrKey::polarity, "极性", OcrKey::darkOnLight,
        Vision::paramChoices(OcrKey::darkOnLight, OcrKey::lightOnDark, OcrKey::both)));
    f.push_back(Vision::paramInt(OcrKey::minCharHeight, "最小字高(0自动)", 0, true, 0, false, 0));
    f.push_back(Vision::paramInt(OcrKey::maxCharHeight, "最大字高(0自动)", 0, true, 0, false, 0));
    f.push_back(Vision::paramString(OcrKey::expected, "期望文本", std::string(), std::vector<std::string>()));
    return f;
}

const Vision::ToolParaBase* Vision::OcrTool::tool_param() const { return &param_; }

int Vision::OcrTool::check_param(std::string* detail) const
{
    (void)detail;
    return HS_OK;
}

Vision::OutputSpec Vision::OcrTool::cfg_output() const
{
    Vision::OutputSpec spec;
    Vision::OutputField text;
    text.key = OcrKey::text;
    text.title = OcrKey::text;
    spec.fields.push_back(text);
    Vision::OutputField count;
    count.key = OcrKey::count;
    count.title = OcrKey::count;
    spec.fields.push_back(count);
    return spec;
}

Vision::TolSpec Vision::OcrTool::cfg_tol() const
{
    Vision::TolSpec spec;
    Vision::TolField count;
    count.key = OcrKey::count;
    count.title = OcrKey::count;
    count.defMin = 1;
    count.defMax = 999;
    count.integer = true;
    spec.items.push_back(count);
    return spec;
}

Vision::DisplaySpec Vision::OcrTool::cfg_display() const
{
    Vision::DisplaySpec spec;
    Vision::GraphicSlot result;
    result.key = OcrKey::resultRoi;
    result.title = OcrKey::resultRoi;
    result.defEnable = true;
    result.region = true;
    spec.graphics.push_back(result);
    Vision::TextSlot text;
    text.key = OcrKey::text;
    text.title = OcrKey::text;
    text.defAnchor = Vision::TextAnchor::ResultPoint;
    text.defEnable = true;
    spec.texts.push_back(text);
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

Vision::ToolCaps Vision::OcrTool::cfg_roi() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = OcrKey::roi;
    slot.title = "搜索区";
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect2, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
    c.slotRules.push_back(slot);
    return c;
}

int Vision::OcrTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

void Vision::OcrTool::log_result(Vision::ToolOut* out) const
{
    if (out == nullptr) {
        return;
    }
    Vision::ToolTrace trace;
    trace.action = "结果";
    if (out->values.contains(OcrKey::text)) {
        trace.detail = out->values.value(OcrKey::text).toString();
    } else {
        trace.detail = out->message.empty() ? "未识别到字符" : out->message;
    }
    out->traces.push_back(trace);
}

void Vision::OcrTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
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
            one.port = OcrKey::roi;
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

int Vision::OcrTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
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

std::vector<Vision::ParamField> Vision::OcrTool::paramFields() const { return cfg_param(); }

Vision::ToolOut Vision::OcrTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
