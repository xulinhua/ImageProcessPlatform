/**
 * @file FindRectTool.cpp
 * @brief 找矩形工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FindRectTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsConst.h>
#include <HsCommData/HsError.h>
#include <HsCommData/Graphic.h>
#include <cstdio>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};
const char kToolName[] = "locate.find_rect";
const char kDisplayName[] = "找矩形";
const char kDescription[] = "在搜索区里拟合旋转矩形，输出中心、角度、宽高与叠加图形";
const char kPose[] = "pose";
const char kVerdict[] = "verdict";
const char kResultRoi[] = "resultRoi";

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

Vision::FindRectTool::FindRectTool() {}
Vision::FindRectTool::~FindRectTool() {}

Vision::ToolBase* Vision::FindRectTool::clone() const
{
    FindRectTool* p = new FindRectTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

std::string Vision::FindRectTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::FindRectTool::type() const { return Vision::ToolType::Locate; }
std::string Vision::FindRectTool::display_name() const { return kDisplayName; }
std::string Vision::FindRectTool::description() const { return kDescription; }
std::string Vision::FindRectTool::version() const { return std::string(kToolVersion); }

std::vector<Vision::VerNote> Vision::FindRectTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

Vision::ParamMap Vision::FindRectTool::default_params() const { return defaultsFrom(cfg_param()); }

std::vector<Vision::ParamField> Vision::FindRectTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDouble(FindRectKey::sigma, "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramInt(FindRectKey::low, "低阈值", 20, true, 0, true, 255));
    f.push_back(Vision::paramInt(FindRectKey::high, "高阈值", 40, true, 0, true, 255));
    f.push_back(Vision::paramDouble(FindRectKey::minSide, "最短边", 10.0, true, 1.0, false, 0));
    f.push_back(Vision::paramDouble(FindRectKey::maxSide, "最长边(0不限)", 0.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble(FindRectKey::maxDist, "拟合误差", 2.0, true, 0.1, true, 50.0));
    return f;
}

const Vision::ToolParaBase* Vision::FindRectTool::tool_param() const { return &param_; }

int Vision::FindRectTool::check_param(std::string* detail) const
{
    (void)detail;
    return HS_OK;
}

Vision::OutputSpec Vision::FindRectTool::cfg_output() const
{
    Vision::OutputSpec spec;
    const char* keys[] = { FindRectKey::width, FindRectKey::height, FindRectKey::dist };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        Vision::OutputField field;
        field.key = keys[i];
        field.title = keys[i];
        spec.fields.push_back(field);
    }
    return spec;
}

Vision::TolSpec Vision::FindRectTool::cfg_tol() const
{
    Vision::TolSpec spec;
    auto add = [&](const char* key, double lo, double hi) {
        Vision::TolField field;
        field.key = key;
        field.title = key;
        field.defMin = lo;
        field.defMax = hi;
        spec.items.push_back(field);
    };
    add(FindRectKey::width, 1, 100000);
    add(FindRectKey::height, 1, 100000);
    add(FindRectKey::dist, 0, 50);
    return spec;
}

Vision::DisplaySpec Vision::FindRectTool::cfg_display() const
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

Vision::ToolCaps Vision::FindRectTool::cfg_roi() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = FindRectKey::roi;
    slot.title = "搜索区";
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect2, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
    c.slotRules.push_back(slot);
    return c;
}

int Vision::FindRectTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

void Vision::FindRectTool::log_result(Vision::ToolOut* out) const
{
    if (out == nullptr) {
        return;
    }
    if (!out->io.poses.empty()) {
        const HsCommData::Pose2& pose = out->io.poses[0].pose;
        char line[160];
        sprintf_s(line, "中心 X %.2f  Y %.2f  角度 %.2f",
            pose.tx, pose.ty, pose.angle * 180.0 / HsCommData::kPi);
        Vision::ToolTrace trace;
        trace.action = "结果";
        trace.detail = line;
        out->traces.push_back(trace);
        return;
    }
    Vision::ToolTrace trace;
    trace.action = "结果";
    trace.detail = out->message.empty() ? "未找到矩形" : out->message;
    out->traces.push_back(trace);
}

void Vision::FindRectTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
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
            one.port = FindRectKey::roi;
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

int Vision::FindRectTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
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

std::vector<Vision::ParamField> Vision::FindRectTool::paramFields() const { return cfg_param(); }

Vision::ToolOut Vision::FindRectTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
