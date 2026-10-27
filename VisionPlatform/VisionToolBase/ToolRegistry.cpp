/**
 * @file ToolRegistry.cpp
 * @brief ToolRegistry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolBase.h"
#include "ToolRegistry.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"

namespace Vision {
// 进程内单例（本 DLL 内一份）
ToolRegistry& ToolRegistry::instance()
{
    static ToolRegistry g;
    return g;
}

// 释放全部已注册原型
ToolRegistry::~ToolRegistry()
{
    for (int i = 0; i < static_cast<int>(m_tools.size()); ++i) {
        if (m_tools[static_cast<size_t>(i)]) {
            m_tools[static_cast<size_t>(i)]->destroy();
            m_tools[static_cast<size_t>(i)] = 0;
        }
    }
    m_tools.clear();
}

// 按 toolName 去重后接管原型；重复则 destroy 传入指针
void ToolRegistry::add(ToolBase* tool)
{
    if (!tool) {
        return;
    }
    if (find(tool->toolName())) {
        tool->destroy();
        return;
    }
    if (!m_incomingStamp.empty()) {
        m_stamps[tool->toolName()] = m_incomingStamp;
    }
    m_tools.push_back(tool);
}

// 记下或清除当前正在加载的工具 DLL 时间戳
void ToolRegistry::setIncomingPluginStamp(const std::string& stamp)
{
    m_incomingStamp = stamp;
}

// 已登记工具的 DLL 修改时间
std::string ToolRegistry::pluginStamp(const std::string& toolName) const
{
    std::map<std::string, std::string>::const_iterator it = m_stamps.find(toolName);
    if (it == m_stamps.end()) {
        return std::string();
    }
    return it->second;
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

// 全部已注册原型
std::vector<ToolBase*> ToolRegistry::all() const
{
    return m_tools;
}

// 按注册名 clone 出新实例
ToolBase* ToolRegistry::create(const std::string& toolName) const
{
    ToolBase* proto = find(toolName);
    if (!proto) {
        return 0;
    }
    return proto->clone();
}

}
