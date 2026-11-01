/**
 * @file TemplateTool.cpp
 * @brief 点云模板定位：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TemplateTool.h"
#include "SolutionDataPaths.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <cstdio>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "cloud template match aligned to TmplTool" },
};
const char kToolName[] = "locate3d.template";
const char kDisplayName[] = "Cloud Template";
const char kDescription[] = "ICP cloud template locate. Model under toolDataDir/pcl or open3d";

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

Vision::TemplateTool::TemplateTool() {}
Vision::TemplateTool::~TemplateTool() {}

Vision::ToolBase* Vision::TemplateTool::clone() const
{
    TemplateTool* p = new TemplateTool();
    p->copyInstanceDataFrom(*this);
    p->cloud_org_ = cloud_org_;
    p->cloud_org_ok_ = cloud_org_ok_;
    p->data_dir_ = data_dir_;
    return p;
}

std::string Vision::TemplateTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::TemplateTool::type() const { return Vision::ToolType::Locate3D; }
std::string Vision::TemplateTool::display_name() const { return kDisplayName; }
std::string Vision::TemplateTool::description() const { return kDescription; }
std::string Vision::TemplateTool::version() const { return std::string(kToolVersion); }
std::vector<Vision::VerNote> Vision::TemplateTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

Vision::ParamMap Vision::TemplateTool::default_params() const
{
    Vision::ParamMap m = defaultsFrom(cfg_teach());
    const Vision::ParamMap run = defaultsFrom(cfg_param());
    const std::vector<std::string> keys = run.keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        m.insert(keys[i], run.value(keys[i]));
    }
    return m;
}

std::vector<Vision::ParamField> Vision::TemplateTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDouble(Vision::Locate3dTmplKey::maxCorr, "Max corr", 5.0, true, 0.0, false, 0.0));
    f.push_back(Vision::paramInt(Vision::Locate3dTmplKey::maxIter, "Max iter", 50, true, 1, true, 500));
    f.push_back(Vision::paramDouble(Vision::Locate3dTmplKey::minFitness, "Fitness max", 1.0e9, true, 0.0, false, 0.0));
    return f;
}

std::vector<Vision::ParamField> Vision::TemplateTool::cfg_teach() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDouble(Vision::Locate3dTmplKey::leafSize, "Leaf size", 1.0, true, 0.0, false, 0.0));
    f.push_back(Vision::paramString(Vision::Locate3dTmplKey::modelFile, "Model file", "model.pcd", std::vector<std::string>()));
    return f;
}

const Vision::ToolParaBase* Vision::TemplateTool::tool_param() const { return &param_; }

int Vision::TemplateTool::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail != nullptr) {
            *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        }
        return param_.loadCode();
    }
    return HS_OK;
}

Vision::OutputSpec Vision::TemplateTool::cfg_output() const
{
    Vision::OutputSpec spec;
    Vision::OutputField fitness;
    fitness.key = Vision::Locate3dTmplKey::fitness;
    fitness.title = Vision::Locate3dTmplKey::fitness;
    spec.fields.push_back(fitness);
    Vision::OutputField count;
    count.key = Vision::Locate3dTmplKey::count;
    count.title = Vision::Locate3dTmplKey::count;
    spec.fields.push_back(count);
    return spec;
}

Vision::TolSpec Vision::TemplateTool::cfg_tol() const
{
    Vision::TolSpec spec;
    Vision::TolField fitness;
    fitness.key = Vision::Locate3dTmplKey::fitness;
    fitness.title = Vision::Locate3dTmplKey::fitness;
    fitness.defMin = 0;
    fitness.defMax = 10;
    spec.items.push_back(fitness);
    return spec;
}

Vision::DisplaySpec Vision::TemplateTool::cfg_display() const { return Vision::DisplaySpec(); }
Vision::ToolCaps Vision::TemplateTool::cfg_roi() const { return Vision::ToolCaps(); }

std::vector<Vision::PortDesc> Vision::TemplateTool::ports() const
{
    std::vector<Vision::PortDesc> p;
    Vision::PortDesc scene(Vision::PortDir::In, Vision::PortData::Cloud);
    scene.slot.name = Vision::Locate3dTmplKey::scene;
    p.push_back(scene);
    Vision::PortDesc tmpl(Vision::PortDir::In, Vision::PortData::Cloud);
    tmpl.slot.name = Vision::Locate3dTmplKey::tmpl;
    p.push_back(tmpl);
    p.push_back(Vision::PortDesc(Vision::PortDir::Out, Vision::PortData::Pose3));
    p.push_back(Vision::PortDesc(Vision::PortDir::Out, Vision::PortData::Tuple));
    p.push_back(Vision::PortDesc(Vision::PortDir::Out, Vision::PortData::Logic));
    return p;
}

int Vision::TemplateTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

std::vector<Vision::ToolAction> Vision::TemplateTool::cfg_action() const
{
    std::vector<Vision::ToolAction> actions;
    Vision::ToolAction teachAct;
    teachAct.id = Vision::Locate3dTmplKey::teachModel;
    teachAct.title = "Teach model";
    teachAct.place = Vision::ActionPlace::Teach;
    actions.push_back(teachAct);
    return actions;
}

int Vision::TemplateTool::action(const std::string& id, const Vision::ToolIn& in, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    if (id == Vision::Locate3dTmplKey::teachModel) {
        return teach_model(in, out);
    }
    return Vision::ToolBase::action(id, in, out);
}

int Vision::TemplateTool::save()
{
    return ToolBase::save();
}

void Vision::TemplateTool::clear(const std::string& toolDataDir)
{
    cloud_org_ = HsCommData::Cloud3();
    cloud_org_ok_ = false;
    data_dir_.clear();
    ToolBase::clear(toolDataDir);
}

void Vision::TemplateTool::log_result(Vision::ToolOut* out) const
{
    if (out == nullptr) {
        return;
    }
    if (out->io.poses3.empty()) {
        Vision::ToolTrace trace;
        trace.action = "result";
        trace.detail = out->message.empty() ? "no match" : out->message;
        out->traces.push_back(trace);
        return;
    }
    for (size_t i = 0; i < out->io.poses3.size(); ++i) {
        const HsCommData::Pose3& pose = out->io.poses3[i].pose;
        char line[192];
        sprintf_s(line, "target %u : T %.3f %.3f %.3f",
            static_cast<unsigned>(i), pose.tx, pose.ty, pose.tz);
        Vision::ToolTrace trace;
        trace.action = "result";
        trace.detail = line;
        out->traces.push_back(trace);
    }
}

int Vision::TemplateTool::teach_model(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    const int code = shot(in, Vision::Locate3dTmplKey::teachModel, out);
    if (out != nullptr && out->errCode == HS_OK) {
        keep_org_cloud(in);
    }
    return code;
}

void Vision::TemplateTool::keep_org_cloud(const Vision::ToolIn& in)
{
    if (in.inputIo.clouds.empty() || in.inputIo.clouds[0].cloud.isEmpty()) {
        return;
    }
    cloud_org_ = in.inputIo.clouds[0].cloud;
    cloud_org_ok_ = true;
    if (!in.toolDataDir.empty()) {
        data_dir_ = in.toolDataDir;
    }
}

void Vision::TemplateTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    ain->mode = in.mode;
    ain->clouds = in.inputIo.clouds;
    ain->poses3 = in.inputIo.poses3;
    ain->shapes3 = in.inputIo.shapes3;
    ain->param = &param_;
    ain->run = in.param.run;
    ain->teach = in.param.learn;
    ain->judge = in.param.judge;
    ain->solutionDir = in.solutionDir;
    ain->toolDataDir = in.toolDataDir;
}

int Vision::TemplateTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = Vision::ToolOut();
    Vision::ToolIn shotIn = in;
    if (shotIn.algLib != Vision::AlgLibPcl && shotIn.algLib != Vision::AlgLibOpen3d) {
        shotIn.algLib = Vision::AlgLibPcl;
    }
    attachAlg(shotIn.toolDataDir);
    param_.load(shotIn.param);
    if (param_.loadCode() != HS_OK) {
        out->ok = false;
        out->errCode = param_.loadCode();
        out->message = HsCommData::HsException(param_.loadCode()).errorMessage();
        return out->errCode;
    }
    Vision::AlgToolBase* alg = attachedAlg();
    if (alg == nullptr) {
        out->ok = false;
        out->errCode = HS_ERR_NO_ALG;
        out->message = "cloud alg not registered";
        return out->errCode;
    }
    Vision::AlgToolIn ain;
    fill_alg_in(shotIn, &ain);
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
    out->io.clouds = algOut.clouds;
    out->io.poses3 = algOut.poses3;
    out->io.shapes3 = algOut.shapes3;
    out->io.tuples = algOut.tuples;
    out->traces = algOut.traces;
    out->syncIo();
    log_result(out);
    return out->errCode;
}

std::vector<Vision::ParamField> Vision::TemplateTool::paramFields() const
{
    std::vector<Vision::ParamField> f = cfg_teach();
    const std::vector<Vision::ParamField> run = cfg_param();
    f.insert(f.end(), run.begin(), run.end());
    return f;
}

Vision::ToolOut Vision::TemplateTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}

Vision::ToolOut Vision::TemplateTool::teach(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    teach_model(in, &out);
    return out;
}
