/**
 * @file AgentSettingsDialog.h
 * @brief 被控端设置：承载连接 / 安全 / 画面等配置面板
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_AGENT_SETTINGS_DIALOG_H
#define VISION_CONTROLLED_AGENT_SETTINGS_DIALOG_H

#include <QDialog>
#include <functional>

class QListWidget;
class QWidget;

namespace Vision {

/**
 * @class AgentSettingsDialog
 * @brief 把主窗拆出的设置面板嵌进对话框；关闭时面板归还主窗，控件指针仍由 AgentWindow 持有
 */
class AgentSettingsDialog : public QDialog {
public:
    /**
     * @param body 设置面板，原父为 AgentWindow，打开时改挂到本对话框
     * @param apply 点「应用」或「确定」时回调（保存并按需重连），可空
     */
    AgentSettingsDialog(QWidget* body, const std::function<void()>& apply, QWidget* parent = 0);
    ~AgentSettingsDialog() override;

    /** @brief 打开时选中的页：0 连接，1 安全，2 画面，3 服务 */
    void setPage(int page);

private:
    QWidget* m_body;                    ///< 设置面板，不拥有
    QWidget* m_oldParent;               ///< 打开前的父对象，关闭时归还
    QListWidget* m_nav;                 ///< 左侧导航
    std::function<void()> m_apply;      ///< 应用回调
};

}

#endif
