/**
 * @file SectionTool.cpp
 * @brief 拟合平面带状截面
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SectionTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <cstdio>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = { { "1.0.0.0", "拟合平面带状截面" } };
const char kToolName[] = "measure3d.section";
const char kDisplayName[] = "截面";
const char kDescription[] = "拟合平面带状截面";
Vision::ParamMap defaultsFromFields(const std::vector<Vision::ParamField>& fields)
{
    Vision::ParamMap m;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].type == Vision::ParamFieldType::Device) continue;
        m.insert(fields[i].key, fields[i].def);
    }
    return m;
}
}

Vision::SectionTool::SectionTool() {}
Vision::SectionTool::~SectionTool() {}
Vision::ToolBase* Vision::SectionTool::clone() const
{
    SectionTool* p = new SectionTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
std::string Vision::SectionTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::SectionTool::type() const { return Vision::ToolType::Measure3D; }
std::string Vision::SectionTool::display_name() const { return kDisplayName; }
std::string Vision::SectionTool::description() const { return kDescription; }
std::string Vision::SectionTool::version() const { return std::string(kToolVersion); }
std::vector<Vision::VerNote> Vision::SectionTool::version_notes() const
{ return Vision::verNotesFrom(kVerNotes, 1); }
Vision::ParamMap Vision::SectionTool::default_params() const { return defaultsFromFields(cfg_param()); }
std::vector<Vision::ParamField> Vision::SectionTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDouble("band","带宽",1.0,true,0.0,false,0.0));
    return f;
}
const Vision::ToolParaBase* Vision::SectionTool::tool_param() const { return &param_; }
int Vision::SectionTool::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail) *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        return param_.loadCode();
    }
    return HS_OK;
}
Vision::OutputSpec Vision::SectionTool::cfg_output() const
{
    Vision::OutputSpec spec; Vision::OutputField f; f.key = "points"; f.title = "points";
    spec.fields.push_back(f); return spec;
}
Vision::TolSpec Vision::SectionTool::cfg_tol() const
{
    Vision::TolSpec spec; Vision::TolField f; f.key = "points"; f.title = "points";
    f.defMin = 0; f.defMax = 1.0e9; spec.items.push_back(f); return spec;
}
Vision::DisplaySpec Vision::SectionTool::cfg_display() const { return Vision::DisplaySpec(); }
Vision::ToolCaps Vision::SectionTool::cfg_roi() const { return Vision::ToolCaps(); }
std::vector<Vision::PortDesc> Vision::SectionTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
int Vision::SectionTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out) { return shot(in, out); }
void Vision::SectionTool::log_result(Vision::ToolOut* out) const
{
    if (!out) return;
    Vision::ToolTrace trace; trace.action = "result";
    trace.detail = out->message.empty() ? (out->ok ? "ok" : "fail") : out->message;
    out->traces.push_back(trace);
}
void Vision::SectionTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    ain->mode = in.mode;
    ain->clouds = in.inputIo.clouds;
    ain->poses3 = in.inputIo.poses3;
    ain->shapes3 = in.inputIo.shapes3;
    ain->images = in.inputIo.images;
    if (!in.inputIo.images.empty()) {
        ain->imagePtr = &in.inputIo.images[0].image;
        ain->imageNative = in.inputIo.images[0].native;
    } else {
        ain->imagePtr = &in.input;
        ain->imageNative = in.inputNative;
    }
    ain->param = &param_;
    ain->run = in.param.run;
    ain->teach = in.param.learn;
    ain->judge = in.param.judge;
    ain->solutionDir = in.solutionDir;
    ain->toolDataDir = in.toolDataDir;
}
int Vision::SectionTool::shot(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    if (!out) return HS_ERR_NULL;
    *out = Vision::ToolOut();
    Vision::ToolIn shotIn = in;
    if (shotIn.algLib != Vision::AlgLibPcl && shotIn.algLib != Vision::AlgLibOpen3d) shotIn.algLib = Vision::AlgLibPcl;
    attachAlg(shotIn.toolDataDir);
    param_.load(shotIn.param);
    if (param_.loadCode() != HS_OK) {
        out->ok = false; out->errCode = param_.loadCode();
        out->message = HsCommData::HsException(param_.loadCode()).errorMessage();
        return out->errCode;
    }
    Vision::AlgToolBase* alg = attachedAlg();
    if (!alg) { out->ok = false; out->errCode = HS_ERR_NO_ALG; out->message = "未登记点云算法"; return out->errCode; }
    Vision::AlgToolIn ain; fill_alg_in(shotIn, &ain);
    Vision::AlgToolOut algOut; alg->shot_init(ain, &algOut);
    const int code = alg->run(Vision::ExecMode::Auto, ain, &algOut);
    if (code != HS_OK && algOut.errCode == HS_OK) { algOut.errCode = code; algOut.ok = false; }
    out->ok = algOut.ok; out->errCode = algOut.errCode; out->error = algOut.errCode != HS_OK;
    out->message = algOut.message; out->values = algOut.values;
    out->io.clouds = algOut.clouds; out->io.poses3 = algOut.poses3; out->io.shapes3 = algOut.shapes3;
    out->io.tuples = algOut.tuples; out->io.images = algOut.images;
    if (!algOut.image.image.isNull()) out->image = algOut.image.image;
    out->traces = algOut.traces; out->syncIo(); log_result(out);
    return out->errCode;
}
Vision::ToolOut Vision::SectionTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out; execute(in, &out); return out;
}