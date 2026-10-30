/**
 * @file ToolEditorBase.h
 * @brief ToolEditorBase头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLEDITORBASE_H
#define VISION_TOOLEDITORBASE_H
#include "VisionToolUiExport.h"

#include "Solution.h"
#include <QWidget>

namespace Vision {

class Session;

/**
 * @class ToolEditorBase
 * @brief 工具参数页公共底座。派生类不要再声明 Q_OBJECT，改动接 onEdited
 */
class VISION_TOOLUI_API ToolEditorBase : public QWidget {
    Q_OBJECT
public:
    /** @brief 参数页底座，稍后绑定工具 */
    explicit ToolEditorBase(QWidget* parent = 0);
    /** @brief 虚析构 */
    virtual ~ToolEditorBase() {}

    /** @brief 绑定同一 ToolCfg（快捷面板与调试窗共用） */
    void bind(ToolCfg* tool, Session* session, Solution* sol);
    /** @brief 控件写回 ToolCfg（不标 dirty；用户编辑时由 onEdited 标记） */
    void apply();
    /** @brief 从 ToolCfg 重新灌入控件 */
    void reload();

signals:
    /** @brief 参数已改 */
    void edited();

protected slots:
    /** @brief 控件改动：写回并通知 */
    void onEdited();

protected:
    /** @brief 派生类灌入控件 */
    virtual void loadFromTool() = 0;
    /** @brief 派生类写回 ToolCfg */
    virtual void saveToTool() = 0;

    ToolCfg* m_tool;     ///< 当前工具，不拥有
    Session* m_session;     ///< 会话，不拥有
    Solution* m_sol;        ///< 方案，不拥有
    bool m_block;           ///< 灌入时挡住回写
};

}

#endif
