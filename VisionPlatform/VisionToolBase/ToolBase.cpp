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

namespace {

const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};

}

namespace Vision {

// 空实例
ToolBase::ToolBase()
{
}

// 释放实例
ToolBase::~ToolBase()
{
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
    switch (type) {
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

// 默认端口由类型决定
std::vector<PortDesc> ToolBase::ports() const
{
    return defaultPortsForType(type());
}

// 默认能力：采集、逻辑、通信和标定不画搜索区
ToolCaps ToolBase::capabilities() const
{
    ToolCaps c;
    const ToolType t = type();
    if (t == ToolType::Acquire || t == ToolType::Logic || t == ToolType::Comm || t == ToolType::Calib) {
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



}
