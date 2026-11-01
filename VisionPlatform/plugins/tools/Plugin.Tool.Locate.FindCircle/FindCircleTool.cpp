/**
 * @file FindCircleTool.cpp
 * @brief 找圆工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FindCircleTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};
const char kToolName[] = "locate.find_circle";
const char kDisplayName[] = "找圆";
const char kDescription[] = "在 ROI 内找圆，输出圆心、半径与叠加图形";
const char kRoi[] = "roi";
const char kCount[] = "count";
const char kResultRoi[] = "resultRoi";
const char kPose[] = "pose";
const char kVerdict[] = "verdict";
const char kRadius[] = "radius";

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

Vision::FindCircleTool::FindCircleTool() {}
Vision::FindCircleTool::~FindCircleTool() {}

Vision::ToolBase* Vision::FindCircleTool::clone() const
{
    FindCircleTool* p = new FindCircleTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

std::string Vision::FindCircleTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::FindCircleTool::type() const { return Vision::ToolType::Locate; }
std::string Vision::FindCircleTool::display_name() const { return kDisplayName; }
std::string Vision::FindCircleTool::description() const { return kDescription; }
std::string Vision::FindCircleTool::version() const { return std::string(kToolVersion); }

std::vector<Vision::VerNote> Vision::FindCircleTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

Vision::ParamMap Vision::FindCircleTool::default_params() const { return defaultsFrom(cfg_param()); }

std::vector<Vision::ParamField> Vision::FindCircleTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramInt("threshold", "阈值", 80, true, 0, true, 255));
    f.push_back(Vision::paramInt("minR", "最小半径", 8, true, 1, false, 0));
    f.push_back(Vision::paramInt("maxR", "最大半径", 400, true, 1, false, 0));
    f.push_back(Vision::paramBool("dark", "暗目标", true));
    f.push_back(Vision::paramDouble("sigma", "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramInt("low", "低阈值", 20, true, 0, true, 255));
    f.push_back(Vision::paramInt("high", "高阈值", 40, true, 0, true, 255));
    return f;
}

const Vision::ToolParaBase* Vision::FindCircleTool::tool_param() const { return &param_; }

int Vision::FindCircleTool::check_param(std::string* detail) const
{
    (void)detail;
    return HS_OK;
}

Vision::OutputSpec Vision::FindCircleTool::cfg_output() const
{
    Vision::OutputSpec spec;
    const char* keys[] = { kCount, "x", "y", kRadius };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        Vision::OutputField field;
        field.key = keys[i];
        field.title = keys[i];
        spec.fields.push_back(field);
    }
    return spec;
}

Vision::TolSpec Vision::FindCircleTool::cfg_tol() const
{
    Vision::TolSpec spec;
    Vision::TolField count;
    count.key = kCount;
    count.title = kCount;
    count.defMin = 1;
    count.defMax = 1;
    count.integer = true;
    spec.items.push_back(count);
    Vision::TolField radius;
    radius.key = kRadius;
    radius.title = kRadius;
    radius.defMin = 1;
    radius.defMax = 100000;
    spec.items.push_back(radius);
    return spec;
}

Vision::DisplaySpec Vision::FindCircleTool::cfg_display() const
{
    Vision::DisplaySpec spec;
    Vision::GraphicSlot result;
    result.key = kResultRoi;
    result.title = kResultRoi;
    result.defEnable = true;
    result.region = true;
    spec.graphics.push_back(result);
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

Vision::ToolCaps Vision::FindCircleTool::cfg_roi() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = kRoi;
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 1));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    c.slotRules.push_back(slot);
    return c;
}

int Vision::FindCircleTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

void Vision::FindCircleTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
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

int Vision::FindCircleTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
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

std::vector<Vision::ParamField> Vision::FindCircleTool::paramFields() const { return cfg_param(); }

Vision::ToolOut Vision::FindCircleTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
