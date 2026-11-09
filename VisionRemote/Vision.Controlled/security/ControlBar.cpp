/**
 * @file ControlBar.cpp
 * @brief 远程操作提示条
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ControlBar.h"
#include "RemoteTheme.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>

namespace Vision {

ControlBar::ControlBar(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus)
    , m_chat(0)
    , m_drag(false)
    , m_dragX(0)
    , m_placed(false)
{
    // 显示时不激活，避免抢走现场或远程正在操作的窗口焦点
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // 深色底、下边两角圆，按钮样式只在条内生效
    QFrame* box = new QFrame(this);
    box->setObjectName("controlBar");
    box->setStyleSheet(
        "QFrame#controlBar{background:rgba(20,24,31,235);border-bottom-left-radius:10px;border-bottom-right-radius:10px;}"
        "QFrame#controlBar QLabel{color:#FFFFFF;}"
        "QFrame#controlBar QPushButton{background:rgba(255,255,255,30);border:none;color:#FFFFFF;min-height:26px;font-size:12px;padding:0 10px;}"
        "QFrame#controlBar QPushButton:hover{background:rgba(255,255,255,55);color:#FFFFFF;}"
        "QFrame#controlBar QPushButton:disabled{background:rgba(255,255,255,14);color:#8C8C8C;}"
        "QFrame#controlBar QPushButton[kind=\"dangerFill\"]{background:#FF4D4F;}"
        "QFrame#controlBar QPushButton[kind=\"dangerFill\"]:hover{background:#FF7875;}");

    // 把手、状态点、文字，以及聊天、暂停操作、断开
    QLabel* grip = new QLabel(QString::fromUtf8("⋮⋮"), box);
    grip->setStyleSheet("color:#8C8C8C;");
    grip->setCursor(Qt::SizeHorCursor);
    m_live = new QLabel(box);
    m_live->setFixedSize(14, 14);
    m_text = new QLabel(box);
    m_chat = RemoteTheme::button(QString::fromUtf8("聊天"), 0, 0, box);
    m_chat->setIcon(RemoteTheme::icon("chat", QColor("#FFFFFF"), 13));
    m_chat->setEnabled(false);
    m_chat->setToolTip(QString::fromUtf8("向中控台发一条文字"));
    m_pause = RemoteTheme::button(QString::fromUtf8("暂停操作"), 0, 0, box);
    m_pause->setIcon(RemoteTheme::icon("pause", QColor("#FFFFFF"), 13));
    m_pause->setToolTip(QString::fromUtf8("收回操作权，对方回到只看"));
    QPushButton* close = RemoteTheme::button(QString::fromUtf8("断开"), 0, "dangerFill", box);
    close->setIcon(RemoteTheme::icon("x", QColor("#FFFFFF"), 13));
    close->setToolTip(QString::fromUtf8("断开中控台，直到在被控端窗口点“重新连接”"));

    QHBoxLayout* row = new QHBoxLayout(box);
    row->setContentsMargins(16, 8, 10, 8);
    row->setSpacing(12);
    row->addWidget(grip);
    row->addWidget(m_live);
    row->addWidget(m_text);
    row->addWidget(m_chat);
    row->addWidget(m_pause);
    row->addWidget(close);

    QHBoxLayout* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(box);

    // 按钮只转发给外部回调，会话逻辑在 AgentSession
    QObject::connect(m_chat, &QPushButton::clicked, [this]() {
        if (m_chatFn) {
            m_chatFn();
        }
    });
    QObject::connect(m_pause, &QPushButton::clicked, [this]() {
        if (m_pauseFn) {
            m_pauseFn();
        }
    });
    QObject::connect(close, &QPushButton::clicked, [this]() {
        if (m_disconnectFn) {
            m_disconnectFn();
        }
    });
}

void ControlBar::setState(const QString& controller, bool paused)
{
    const QString who = controller.isEmpty() ? QString::fromUtf8("中控台") : controller;
    const QString text = paused
        ? QString::fromUtf8("现场正在使用，%1 的远程操作已暂停").arg(who)
        : QString::fromUtf8("%1 正在远程操作此电脑").arg(who);
    if (m_text->text() != text) {
        m_text->setText(text);
        m_live->setPixmap(RemoteTheme::pixmap("record", QColor(paused ? "#FAAD14" : "#FF4D4F"), 14, devicePixelRatioF()));
        adjustSize();
    }
}

void ControlBar::setOnPause(const std::function<void()>& pause)
{
    m_pauseFn = pause;
}

void ControlBar::setOnDisconnect(const std::function<void()>& disconnect)
{
    m_disconnectFn = disconnect;
}

void ControlBar::setOnChat(const std::function<void()>& chat)
{
    m_chatFn = chat;
    if (m_chat) {
        m_chat->setEnabled(static_cast<bool>(chat));
    }
}

void ControlBar::showAtTop()
{
    if (isVisible()) {
        return;
    }
    adjustSize();
    if (!m_placed) {
        const QRect area = QApplication::primaryScreen()->geometry();
        move(area.left() + (area.width() - width()) / 2, area.top());
        m_placed = true;
    }
    show();
    raise();
}

void ControlBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_drag = true;
        m_dragX = event->globalPos().x() - x();
    }
}

void ControlBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drag) {
        move(event->globalPos().x() - m_dragX, y());
    }
}

void ControlBar::mouseReleaseEvent(QMouseEvent* event)
{
    (void)event;
    m_drag = false;
}

}
