/**
 * @file ToolEditorBase.cpp
 * @brief ToolEditorBase实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditorBase.h"
#include "ToolParamUi.h"
#include "RecipeDraft.h"
#include "Session.h"

namespace Vision {

// 参数页底座，稍后绑定工具
ToolEditorBase::ToolEditorBase(QWidget* parent)
    : QWidget(parent)
    , m_tool(nullptr)
    , m_session(nullptr)
    , m_sol(nullptr)
    , m_block(false)
{
    initToolParamEditor(this);
}

// 记下指针并灌入
void ToolEditorBase::bind(ToolCfg* tool, Session* session, Solution* sol)
{
    m_tool = tool;
    m_session = session;
    m_sol = sol;
    const bool canEdit = session && session->has(PermEditSolution);
    setEnabled(canEdit && tool);
    reload();
    applyParamControlStyle(this);
}

// 从 ToolCfg 灌入
void ToolEditorBase::reload()
{
    m_block = true;
    loadFromTool();
    m_block = false;
}

// 写回 ToolCfg（不标 dirty；用户改控件时由 onEdited 标记）
void ToolEditorBase::apply()
{
    if (!m_tool || m_block) {
        return;
    }
    saveToTool();
}

// 用户改控件
void ToolEditorBase::onEdited()
{
    if (m_block || !m_tool) {
        return;
    }
    ToolRecipe before;
    before.loadFrom(*m_tool);
    apply();
    ToolRecipe after;
    after.loadFrom(*m_tool);
    if (m_sol && !before.equals(after)) {
        m_sol->dirty = true;
    }
    emit edited();
}

}

#include "moc_ToolEditorBase.cpp"
