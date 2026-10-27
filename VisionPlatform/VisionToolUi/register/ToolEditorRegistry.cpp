/**
 * @file ToolEditorRegistry.cpp
 * @brief ToolEditorRegistry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditorRegistry.h"
#include "ToolEditors.h"

namespace Vision {

// 进程内唯一登记表
ToolEditorRegistry& ToolEditorRegistry::instance()
{
    static ToolEditorRegistry g;
    return g;
}

// 登记工具名到参数页工厂。通用页不按名字登记
void ToolEditorRegistry::add(const std::string& toolName, ToolEditorFactory factory)
{
    if (factory) {
        m_map[toolName] = factory;
    }
}

// 参数页一律按工具壳的参数说明生成
ToolEditorBase* ToolEditorRegistry::create(const std::string& toolName, QWidget* parent)
{
    if (toolName == "preprocess.filter" || toolName == "detect.blob") {
        return createStepEditor(toolName, parent);
    }
    return createGenericEditor(parent);
}

// 不再登记专用参数页
void registerToolEditors()
{
}

}
