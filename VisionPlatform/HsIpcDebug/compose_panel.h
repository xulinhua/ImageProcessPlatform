/**
 * @file compose_panel.h
 * @brief 按消息类型切换发送表单
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HS_IPC_DEBUG_COMPOSE_PANEL_H
#define HS_IPC_DEBUG_COMPOSE_PANEL_H

#include "owned_msg.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QCheckBox;

/** @brief 表单基类。窗口只调用 fill，不关心字段 */
class MessageForm : public QWidget {
public:
    explicit MessageForm(QWidget* parent = 0) : QWidget(parent) {}
    /** @brief 填入报文。失败时 err 说明缺什么 */
    virtual bool fill(OwnedMsg* msg, QString* err) const = 0;
};

/** @brief 左中：消息类型和对应字段，加上发送按钮 */
class ComposePanel : public QWidget {
public:
    explicit ComposePanel(QWidget* parent = 0);

    /** @brief 按当前页填一条报文 */
    bool fill(OwnedMsg* msg, QString* err) const;
    QPushButton* sendButton() const { return m_send; }

private:
    void onType(int index);

    QComboBox* m_type;
    QStackedWidget* m_stack;
    QPushButton* m_send;
    MessageForm* m_signal;
    MessageForm* m_stats;
    MessageForm* m_image;
    MessageForm* m_file;
    MessageForm* m_ack;
    MessageForm* m_empty;
};

#endif
