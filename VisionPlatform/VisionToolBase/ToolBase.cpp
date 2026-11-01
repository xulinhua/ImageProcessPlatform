/**
 * @file ToolBase.cpp
 * @brief ToolBase实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolBase.h"
#include "AlgLibKind.h"
#include "AlgToolBase.h"
#include "ToolGeom.h"
#include <HsCommData/HsError.h>

namespace {

const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};

}

namespace Vision {

// 空实例
ToolBase::ToolBase()
    : m_alg(nullptr)
{
}

// 释放实例
ToolBase::~ToolBase()
{
    if (m_alg) {
        m_alg->destroy();
        m_alg = nullptr;
    }
}

// 身份，落盘
ToolInfo& ToolBase::info()
{
    return m_info;
}

// 身份，落盘
const ToolInfo& ToolBase::info() const
{
    return m_info;
}

// 写入身份
void ToolBase::setInfo(const ToolInfo& value)
{
    m_info = value;
}

// 输入连线，落盘
std::vector<ToolInputLink>& ToolBase::links()
{
    return m_links;
}

// 输入连线，落盘
const std::vector<ToolInputLink>& ToolBase::links() const
{
    return m_links;
}

// 写入输入连线
void ToolBase::setLinks(const std::vector<ToolInputLink>& value)
{
    m_links = value;
}

// 参数，落盘
ToolParam& ToolBase::param()
{
    return m_param;
}

// 参数，落盘
const ToolParam& ToolBase::param() const
{
    return m_param;
}

// 写入参数
void ToolBase::setPara(const ToolParam& value)
{
    m_param = value;
}

// 本拍 IO，仅运行态
ToolIo& ToolBase::io()
{
    return m_io;
}

// 本拍 IO，仅运行态
const ToolIo& ToolBase::io() const
{
    return m_io;
}

// 写入本拍 IO
void ToolBase::setIo(const ToolIo& value)
{
    m_io = value;
}

// 运行态数值，不落盘
ParamMap& ToolBase::runtime()
{
    return m_runtime;
}

// 运行态数值，不落盘
const ParamMap& ToolBase::runtime() const
{
    return m_runtime;
}

// 写入运行态数值
void ToolBase::setRuntime(const ParamMap& value)
{
    m_runtime = value;
}
// 按工具类型给出默认端口表
std::vector<PortDesc> defaultPortsForType(ToolType type)
{
    std::vector<PortDesc> p;
    // 未列出的大类（如 Undefined）返回空表
    switch (type) {
    // 2D 图像链：采集出图，预处理图进图出，定位/检测/结果处理再带位姿、区域和判定口
    case ToolType::Acquire:
        p.push_back(PortDesc(PortDir::Out, PortData::Image));
        break;
    case ToolType::Preprocess:
        p.push_back(PortDesc(PortDir::In, PortData::Image));
        p.push_back(PortDesc(PortDir::Out, PortData::Image));
        break;
    case ToolType::Locate:
        p.push_back(PortDesc(PortDir::In, PortData::Image));
        p.push_back(PortDesc(PortDir::In, PortData::Pose));
        p.push_back(PortDesc(PortDir::Out, PortData::Region));
        p.push_back(PortDesc(PortDir::Out, PortData::Roi));
        p.push_back(PortDesc(PortDir::Out, PortData::Contour));
        p.push_back(PortDesc(PortDir::Out, PortData::Pose));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        break;
    case ToolType::Detect:
        p.push_back(PortDesc(PortDir::In, PortData::Image));
        p.push_back(PortDesc(PortDir::In, PortData::Pose));
        p.push_back(PortDesc(PortDir::In, PortData::Region));
        p.push_back(PortDesc(PortDir::In, PortData::Roi));
        p.push_back(PortDesc(PortDir::Out, PortData::Region));
        p.push_back(PortDesc(PortDir::Out, PortData::Contour));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        break;
    case ToolType::RstProcess:
        p.push_back(PortDesc(PortDir::In, PortData::Image));
        p.push_back(PortDesc(PortDir::In, PortData::Region));
        p.push_back(PortDesc(PortDir::In, PortData::Roi));
        p.push_back(PortDesc(PortDir::Out, PortData::Region));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        break;
    // 逻辑与通信只走判定口；标定图像和位姿都进出
    case ToolType::Logic:
        p.push_back(PortDesc(PortDir::In, PortData::Logic));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        break;
    case ToolType::Comm:
        p.push_back(PortDesc(PortDir::In, PortData::Logic));
        break;
    case ToolType::Calib:
        p.push_back(PortDesc(PortDir::In, PortData::Image));
        p.push_back(PortDesc(PortDir::Out, PortData::Image));
        p.push_back(PortDesc(PortDir::In, PortData::Pose));
        p.push_back(PortDesc(PortDir::Out, PortData::Pose));
        break;
    // 3D 链：点云、三维位姿、三维形状和数值元组
    case ToolType::Acquire3D:
        p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
        p.push_back(PortDesc(PortDir::Out, PortData::Pose3));
        p.push_back(PortDesc(PortDir::Out, PortData::Shape3));
        break;
    case ToolType::Prepare3D:
        p.push_back(PortDesc(PortDir::In, PortData::Cloud));
        p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
        break;
    case ToolType::Measure3D:
        p.push_back(PortDesc(PortDir::In, PortData::Cloud));
        p.push_back(PortDesc(PortDir::Out, PortData::Shape3));
        p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
        p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        break;
    case ToolType::Locate3D:
        p.push_back(PortDesc(PortDir::In, PortData::Cloud));
        p.push_back(PortDesc(PortDir::Out, PortData::Pose3));
        p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        break;
    case ToolType::Detect3D:
        p.push_back(PortDesc(PortDir::In, PortData::Cloud));
        p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
        p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        break;
    case ToolType::Calib3D:
        p.push_back(PortDesc(PortDir::In, PortData::Cloud));
        p.push_back(PortDesc(PortDir::Out, PortData::Pose3));
        p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
        break;
    // 推理与读码：图进，判定、区域或位姿出
    case ToolType::Infer:
        p.push_back(PortDesc(PortDir::In, PortData::Image));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
        p.push_back(PortDesc(PortDir::Out, PortData::Region));
        p.push_back(PortDesc(PortDir::Out, PortData::Roi));
        break;
    case ToolType::Identify:
        p.push_back(PortDesc(PortDir::In, PortData::Image));
        p.push_back(PortDesc(PortDir::In, PortData::Pose));
        p.push_back(PortDesc(PortDir::In, PortData::Roi));
        p.push_back(PortDesc(PortDir::Out, PortData::Region));
        p.push_back(PortDesc(PortDir::Out, PortData::Pose));
        p.push_back(PortDesc(PortDir::Out, PortData::Logic));
        break;
    }
    return p;
}

// 默认无说明
std::string ToolBase::description() const
{
    return std::string();
}

// 默认工具版本
std::string ToolBase::version() const
{
    return std::string(kToolVersion);
}

// 示教一次。默认空结果
ToolOut ToolBase::teach(const ToolIn&)
{
    return ToolOut();
}

// 打断长时间运行。默认空
void ToolBase::stop()
{
}
// 版本变更，只追加
std::vector<VerNote> ToolBase::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

// 默认无参数
ParamMap ToolBase::defaultParams() const
{
    return ParamMap();
}

// 默认无参数说明
std::vector<ParamField> ToolBase::paramFields() const
{
    return std::vector<ParamField>();
}

// 按参数说明拼缺省参数
ParamMap ToolBase::defaultsFrom(const std::vector<ParamField>& fields) const
{
    ParamMap m;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].type == ParamFieldType::Device) {
            continue;
        }
        m.insert(fields[i].key, fields[i].def);
    }
    return m;
}

// 默认端口由类型决定
std::vector<PortDesc> ToolBase::ports() const
{
    return defaultPortsForType(type());
}

std::vector<PortDesc> ToolBase::cfg_port() const
{
    return ports();
}

// 默认能力：采集、逻辑、通信和标定不画搜索区
ToolCaps ToolBase::capabilities() const
{
    ToolCaps c;
    const ToolType t = type();
    if (t == ToolType::Acquire || t == ToolType::Acquire3D || t == ToolType::Prepare3D
        || t == ToolType::Measure3D || t == ToolType::Locate3D || t == ToolType::Detect3D
        || t == ToolType::Calib3D || t == ToolType::Logic
        || t == ToolType::Comm || t == ToolType::Calib) {
        c.usesRoi = false;
    }
    return c;
}

// 组装工具箱条目
ToolDesc ToolBase::desc() const
{
    ToolDesc d;
    d.toolName = toolName();
    d.type = type();
    d.displayName = displayName();
    d.description = description();
    d.version = version();
    d.ports = ports();
    d.paramKeys = defaultParams().keys();
    return d;
}

// 在创建本对象的模块内释放
void ToolBase::destroy()
{
    delete this;
}

// 从另一实例抄四件套与 runtime
void ToolBase::copyInstanceDataFrom(const ToolBase& other)
{
    m_info = other.m_info;
    m_links = other.m_links;
    m_param = other.m_param;
    m_io = other.m_io;
    m_runtime = other.m_runtime;
}

// 写入身份/连线/参数/runtime
void ToolBase::applyInstanceData(const ToolInfo& i,
                                 const std::vector<ToolInputLink>& l,
                                 const ToolParam& p,
                                 const ParamMap& rt)
{
    m_info = i;
    m_links = l;
    m_param = p;
    m_runtime = rt;
}

// 清空运行态 io 与 runtime
void ToolBase::clearRuntime()
{
    m_io = ToolIo();
    m_runtime = ParamMap();
}

// 已注册工具版本表（供关于对话框）
std::vector<VersionInfo> toolVersions()
{
    std::vector<VersionInfo> out;
    const std::vector<ToolBase*> tools = ToolRegistry::instance().all();
    for (size_t i = 0; i < tools.size(); ++i) {
        const ToolDesc d = tools[i]->desc();
        VersionInfo v;
        v.id = d.toolName;
        v.name = d.displayName;
        v.version = d.version;
        v.modified = ToolRegistry::instance().pluginStamp(d.toolName);
        const std::vector<AlgLibKind> libs = AlgToolBaseRegistry::instance().libs(d.toolName);
        v.algoTool = !libs.empty();
        for (size_t k = 0; k < libs.size(); ++k) {
            if (algLibKindValid(static_cast<int>(libs[k]))) {
                v.hasAlgLib[static_cast<int>(libs[k])] = true;
            }
        }
        out.push_back(v);
    }
    return out;
}

// 缺省不刷新，子类按自己的参数类填写
bool ToolBase::initToolParam(ToolParam& src)
{
    const ToolParaBase* param = toolParam();
    std::string oldv = src.paraVersion;
    if (oldv.empty() || oldv == "0.0.0.0") {
        oldv = "1.0.0.0";
    }
    const std::string neu = param ? std::string(param->paraVersion()) : std::string("1.0.0.0");
    if (ToolParaBase::compareVersion(oldv, neu) > 0) {
        m_paramOk = false;
        m_paramBlock = "para newer than tool";
        return false;
    }
    if (param) {
        bool need = false;
        param->preview(oldv, neu, 0, &need);
        if (need) {
            m_paramOk = false;
            m_paramBlock = "need confirm";
            return false;
        }
    }
    return adaptParam(src, oldv, neu, 0);
}

bool ToolBase::adaptParam(ToolParam& src, const std::string& oldVer, const std::string& newVer, std::string* detail)
{
    ToolParaBase* param = const_cast<ToolParaBase*>(toolParam());
    if (!param) {
        src.paraVersion = newVer.empty() ? std::string("1.0.0.0") : newVer;
        if (src.paraVersion == "0.0.0.0") {
            src.paraVersion = "1.0.0.0";
        }
        m_paramOk = true;
        m_paramBlock.clear();
        if (detail) {
            detail->clear();
        }
        return true;
    }
    std::string err;
    const std::string goal = newVer.empty() ? std::string(param->paraVersion()) : newVer;
    if (!param->adaptParam(&src, oldVer, goal, &err)) {
        m_paramOk = false;
        m_paramBlock = err;
        if (detail) {
            *detail = err;
        }
        return false;
    }
    param->load(src);
    m_paramOk = true;
    m_paramBlock.clear();
    if (detail) {
        detail->clear();
    }
    return true;
}

// 缺省没有参数对象
const ToolParaBase* ToolBase::toolParam() const
{
    return 0;
}

// 参数是否就绪
bool ToolBase::paramOk() const
{
    return m_paramOk;
}



// 参数阻塞原因
const std::string& ToolBase::paramBlock() const
{
    return m_paramBlock;
}

void ToolBase::attachAlg(const std::string& toolDataDir)
{
    if (m_alg != nullptr && m_algDir == toolDataDir) {
        return;
    }
    if (m_alg != nullptr) {
        m_alg->destroy();
        m_alg = nullptr;
    }
    m_algDir = toolDataDir;
    AlgToolBase* proto = AlgToolBaseRegistry::instance().get(toolName());
    if (proto == nullptr) {
        return;
    }
    m_alg = proto->clone();
    if (m_alg != nullptr) {
        m_alg->init(toolDataDir);
    }
}

AlgToolBase* ToolBase::attachedAlg() const
{
    return m_alg;
}

int ToolBase::saveAlg()
{
    if (m_alg != nullptr && !m_algDir.empty()) {
        m_alg->save(m_algDir);
    }
    return HS_OK;
}

std::string ToolBase::tool_name() const { return toolName(); }
std::string ToolBase::display_name() const { return displayName(); }
std::vector<VerNote> ToolBase::version_notes() const { return versionNotes(); }
ParamMap ToolBase::default_params() const { return defaultParams(); }
std::vector<ParamField> ToolBase::cfg_param() const { return paramFields(); }
std::vector<ParamField> ToolBase::cfg_param(const ToolParam&) const { return cfg_param(); }
std::vector<ParamField> ToolBase::cfg_teach() const { return std::vector<ParamField>(); }
const ToolParaBase* ToolBase::tool_param() const { return toolParam(); }
int ToolBase::check_param(std::string*) const { return HS_OK; }
OutputSpec ToolBase::cfg_output() const { return OutputSpec(); }
TolSpec ToolBase::cfg_tol() const { return TolSpec(); }
DisplaySpec ToolBase::cfg_display() const { return DisplaySpec(); }
ToolCaps ToolBase::cfg_roi() const { return capabilities(); }
std::vector<ToolAction> ToolBase::cfg_action() const { return std::vector<ToolAction>(); }
int ToolBase::execute(const ToolIn& in, ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = execute(in);
    if (out->errCode != HS_OK) {
        return out->errCode;
    }
    return out->ok ? HS_OK : HS_ERR_OPERATOR;
}
int ToolBase::action(const std::string&, const ToolIn&, ToolOut* out)
{
    if (out != nullptr) {
        out->ok = false;
        out->errCode = HS_ERR_OPERATOR;
        out->message = "unknown action";
    }
    return HS_ERR_OPERATOR;
}
int ToolBase::save() { return saveAlg(); }
void ToolBase::clear(const std::string& toolDataDir) { clearAlg(toolDataDir); }
void ToolBase::log_result(ToolOut*) const {}
void ToolBase::disp_extra(const ToolIn&, ToolOut*) const {}

void ToolBase::build_alg_in(const ToolIn& in, AlgToolIn* ain, const std::string& defaultRoiPort) const
{
    if (ain == nullptr) {
        return;
    }
    ain->mode = in.mode;
    if (!in.inputIo.images.empty()) {
        ain->imagePtr = &in.inputIo.images[0].image;
        ain->imageNative = in.inputIo.images[0].native;
        for (size_t i = 1; i < in.inputIo.images.size(); ++i) {
            ain->images.push_back(in.inputIo.images[i]);
        }
    } else {
        ain->imagePtr = &in.input;
        ain->imageNative = in.inputNative;
    }
    ain->teach = in.param.learn;
    if (in.ownRoisResolved) {
        ain->rois = in.ownRois;
    } else {
        for (size_t i = 0; i < in.rois.size(); ++i) {
            PortRoi one;
            one.roi = in.rois[i];
            one.index = static_cast<int>(i);
            if (!defaultRoiPort.empty()) {
                one.port = defaultRoiPort;
            }
            ain->rois.push_back(one);
        }
    }
    ain->poses = plainPoses(in.inputIo.poses);
    for (size_t i = 0; i < in.inputIo.regions.size(); ++i) {
        const PortRegion& n = in.inputIo.regions[i];
        if (n.use == RegionUse::Invalid) {
            continue;
        }
        if (n.use == RegionUse::Mask) {
            ain->masks.push_back(n);
        } else {
            ain->regions.push_back(n);
        }
    }
    if (in.ownRoisResolved) {
        for (size_t i = 0; i < in.ownRegions.size(); ++i) {
            ain->regions.push_back(in.ownRegions[i]);
        }
    }
    ain->contours = in.inputIo.contours;
    ain->clouds = in.inputIo.clouds;
    ain->poses3 = in.inputIo.poses3;
    ain->shapes3 = in.inputIo.shapes3;
    ain->run = in.param.run;
    ain->judge = in.param.judge;
    ain->solutionDir = in.solutionDir;
    ain->toolDataDir = in.toolDataDir;
    std::vector<HsCommData::HsObject> followed;
    std::vector<int> followAt;
    for (size_t i = 0; i < ain->rois.size(); ++i) {
        if (ain->rois[i].use != RegionUse::Normal) {
            continue;
        }
        followed.push_back(ain->rois[i].roi);
        followAt.push_back(static_cast<int>(i));
    }
    HsCommData::Pose2 pose;
    if (!in.ownRoisResolved && primaryAlgPose(ain->poses, &pose) && !followed.empty()) {
        applyPoseToRois(&followed, ain->run, pose);
        for (size_t i = 0; i < followAt.size(); ++i) {
            ain->rois[static_cast<size_t>(followAt[i])].roi = followed[i];
        }
    }
}

int ToolBase::run_attached_shot(const ToolIn& in, const std::string& actionId, AlgToolIn* ain, bool passImage, ToolOut* out)
{
    if (out == nullptr || ain == nullptr) {
        return HS_ERR_NULL;
    }
    AlgToolBase* alg = attachedAlg();
    if (alg == nullptr) {
        out->ok = false;
        out->errCode = HS_ERR_NO_ALG;
        out->message = "未登记图像算法";
        return out->errCode;
    }
    AlgToolOut algOut;
    alg->shot_init(*ain, &algOut);
    const int code = actionId.empty()
        ? alg->run(ExecMode::Auto, *ain, &algOut)
        : alg->action(actionId, *ain, &algOut);
    if (code != HS_OK && algOut.errCode == HS_OK) {
        algOut.errCode = code;
        algOut.ok = false;
    }
    apply_alg_out(in, algOut, passImage, out);
    return out->errCode;
}

void ToolBase::apply_alg_out(const ToolIn& in, const AlgToolOut& r, bool passImage, ToolOut* out) const
{
    if (out == nullptr) {
        return;
    }
    out->ok = r.ok && r.errCode == HS_OK;
    out->errCode = r.errCode;
    out->error = r.errCode != HS_OK;
    out->message = r.message;
    out->values = r.values;
    out->objects = r.objects;
    out->traces = r.traces;
    out->infos = r.infos;
    out->labels = r.labels;
    out->io.clouds = r.clouds;
    out->io.poses3 = r.poses3;
    out->io.shapes3 = r.shapes3;
    out->io.tuples = r.tuples;
    out->io.poses.clear();
    for (size_t i = 0; i < r.poses.size(); ++i) {
        PortPose one;
        one.index = static_cast<int>(i);
        one.pose = r.poses[i];
        out->io.poses.push_back(one);
    }
    if (passImage && !r.image.image.isNull()) {
        out->image = r.image.image;
        out->imageNative = r.image.native;
    } else {
        out->image = in.input;
        out->imageNative = in.inputNative;
    }
    out->syncIo();
}

void ToolBase::clearAlg(const std::string& toolDataDir)
{
    if (m_alg != nullptr) {
        m_alg->clear(toolDataDir);
        m_alg->destroy();
        m_alg = nullptr;
        m_algDir.clear();
    }
}



}
