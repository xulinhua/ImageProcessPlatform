/**
 * @file FindLineTool.cpp
 * @brief 找线工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FindLineTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};
const char kToolName[] = "locate.find_line";
const char kDisplayName[] = "找线";
const char kDescription[] = "多 ROI，每框可设查找条数、极性与选择策略";
const char kRoi[] = "roi";
const char kLine[] = "line";
const char kResultRoi[] = "resultRoi";
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

Vision::FindLineTool::FindLineTool() {}
Vision::FindLineTool::~FindLineTool() {}

Vision::ToolBase* Vision::FindLineTool::clone() const
{
    FindLineTool* p = new FindLineTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

std::string Vision::FindLineTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::FindLineTool::type() const { return Vision::ToolType::Locate; }
std::string Vision::FindLineTool::display_name() const { return kDisplayName; }
std::string Vision::FindLineTool::description() const { return kDescription; }
std::string Vision::FindLineTool::version() const { return std::string(kToolVersion); }

std::vector<Vision::VerNote> Vision::FindLineTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

Vision::ParamMap Vision::FindLineTool::default_params() const { return defaultsFrom(cfg_param()); }

std::vector<Vision::ParamField> Vision::FindLineTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString("polarity", "极性", "all", Vision::paramChoices("all", "positive", "negative")));
    f.push_back(Vision::paramString("select", "选择", "best", Vision::paramChoices("best", "first", "last")));
    f.push_back(Vision::paramDouble("minLength", "最短长度", 20.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble("sigma", "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramInt("low", "低阈值", 20, true, 0, true, 255));
    f.push_back(Vision::paramInt("high", "高阈值", 40, true, 0, true, 255));
    f.push_back(Vision::paramInt("searchCount", "查找条数", 1, true, 1, true, 50));
    return f;
}

const Vision::ToolParaBase* Vision::FindLineTool::tool_param() const { return &param_; }

int Vision::FindLineTool::check_param(std::string* detail) const
{
    (void)detail;
    return HS_OK;
}

Vision::OutputSpec Vision::FindLineTool::cfg_output() const
{
    Vision::OutputSpec spec;
    const char* keys[] = { "count", "x", "y", "angle", "length", "lengthSum", "lengthMax" };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        Vision::OutputField field;
        field.key = keys[i];
        field.title = keys[i];
        spec.fields.push_back(field);
    }
    return spec;
}

Vision::TolSpec Vision::FindLineTool::cfg_tol() const
{
    Vision::TolSpec spec;
    auto add = [&](const char* key, double lo, double hi, bool integer) {
        Vision::TolField field;
        field.key = key;
        field.title = key;
        field.defMin = lo;
        field.defMax = hi;
        field.integer = integer;
        spec.items.push_back(field);
    };
    add("count", 1, 50, true);
    add("x", -100000, 100000, false);
    add("y", -100000, 100000, false);
    add("angle", -3.141592653589793, 3.141592653589793, false);
    add("length", 1, 100000, false);
    add("lengthSum", 1, 1.0e9, false);
    add("lengthMax", 1, 100000, false);
    return spec;
}

Vision::DisplaySpec Vision::FindLineTool::cfg_display() const
{
    Vision::DisplaySpec spec;
    Vision::GraphicSlot line;
    line.key = kLine;
    line.title = kLine;
    line.defEnable = true;
    spec.graphics.push_back(line);
    Vision::GraphicSlot region;
    region.key = kResultRoi;
    region.title = kResultRoi;
    region.defEnable = true;
    region.region = true;
    spec.graphics.push_back(region);
    auto addText = [&](const char* key, Vision::TextAnchor anchor) {
        Vision::TextSlot text;
        text.key = key;
        text.title = key;
        text.defAnchor = anchor;
        text.defEnable = true;
        spec.texts.push_back(text);
    };
    addText(kPose, Vision::TextAnchor::ResultPoint);
    addText("count", Vision::TextAnchor::ImageCorner);
    addText("length", Vision::TextAnchor::ImageCorner);
    addText("lengthSum", Vision::TextAnchor::ImageCorner);
    addText("lengthMax", Vision::TextAnchor::ImageCorner);
    addText(kVerdict, Vision::TextAnchor::ImageCorner);
    return spec;
}

Vision::ToolCaps Vision::FindLineTool::cfg_roi() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = kRoi;
    slot.maxCount = 50;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjLine, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    c.slotRules.push_back(slot);
    return c;
}

int Vision::FindLineTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

void Vision::FindLineTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
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
            one.port = kRoi;
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

int Vision::FindLineTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
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
    if (!Vision::toolTypeIsImageSource(type())) {
        out->io.images.clear();
    }
    return out->errCode;
}

std::vector<Vision::ParamField> Vision::FindLineTool::paramFields() const { return cfg_param(); }

Vision::ToolOut Vision::FindLineTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
