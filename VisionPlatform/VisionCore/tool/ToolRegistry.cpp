/**
 * @file ToolRegistry.cpp
 * @brief ToolRegistry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolBase.h"
#include "VisionVersion.h"

namespace Vision {

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
    }
    return p;
}

// 默认无说明
std::string ToolBase::description() const
{
    return std::string();
}

// 默认无参数
ParamMap ToolBase::defaultParams() const
{
    return ParamMap();
}

// 默认端口由类型决定
std::vector<PortDesc> ToolBase::ports() const
{
    return defaultPortsForType(type());
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

// 用描述与默认参数新建一条工具记录
ToolCfg ToolBase::createInstance(const ObjectId& deviceId) const
{
    ToolCfg t;
    t.id = ObjectId::create();
    const ToolDesc d = desc();
    t.toolName = d.toolName;
    t.type = d.type;
    t.name = d.displayName;
    t.params = defaultParams();
    t.enabled = true;
    t.deviceId = deviceId;
    return t;
}

// 进程内单例
ToolRegistry& ToolRegistry::instance()
{
    static ToolRegistry g;
    return g;
}

// 释放全部已注册 ToolBase
ToolRegistry::~ToolRegistry()
{
    for (int i = 0; i < static_cast<int>(m_tools.size()); ++i) {
        if (m_tools[static_cast<size_t>(i)]) {
            delete m_tools[static_cast<size_t>(i)];
            m_tools[static_cast<size_t>(i)] = nullptr;
        }
    }
    m_tools.clear();
}

// 按 toolName 去重后接管所有权；重复则删除传入指针
void ToolRegistry::add(ToolBase* tool)
{
    if (!tool) {
        return;
    }
    if (find(tool->toolName())) {
        if (tool) {
            delete tool;
            tool = nullptr;
        }
        return;
    }
    m_tools.push_back(tool);
}

// 按名称查找（别名）
ToolBase* ToolRegistry::findByName(const std::string& toolName) const
{
    return find(toolName);
}

// 按 toolName 精确查找
ToolBase* ToolRegistry::find(const std::string& toolName) const
{
    for (int i = 0; i < static_cast<int>(m_tools.size()); ++i) {
        if (m_tools[static_cast<size_t>(i)]->toolName() == toolName) {
            return m_tools[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 返回该类型的第一个实现
ToolBase* ToolRegistry::findByType(ToolType type) const
{
    for (int i = 0; i < static_cast<int>(m_tools.size()); ++i) {
        if (m_tools[static_cast<size_t>(i)]->type() == type) {
            return m_tools[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 全部已注册实现
std::vector<ToolBase*> ToolRegistry::all() const
{
    return m_tools;
}

// 按名称创建实例；未注册返回空记录
ToolCfg ToolRegistry::create(const std::string& toolName, const ObjectId& deviceId) const
{
    ToolBase* t = find(toolName);
    if (!t) {
        return ToolCfg();
    }
    return t->createInstance(deviceId);
}

// 复制记录并换新 id；已摆过的节点错开一点以免叠在原处
ToolCfg ToolRegistry::duplicate(const ToolCfg& src) const
{
    ToolCfg t = src;
    t.id = ObjectId::create();
    if (t.canvasPlaced) {
        t.canvasX += 28;
        t.canvasY += 28;
    }
    return t;
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
        out.push_back(v);
    }
    return out;
}

}
