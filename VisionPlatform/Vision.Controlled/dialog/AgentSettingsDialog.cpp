/**
 * @file AgentSettingsDialog.cpp
 * @brief 被控端设置对话框
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentSettingsDialog.h"
#include "RemoteTheme.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Vision {

AgentSettingsDialog::AgentSettingsDialog(QWidget* body, const std::function<void()>& apply, QWidget* parent)
    : QDialog(parent)
    , m_body(body)
    , m_oldParent(body ? body->parentWidget() : 0)
    , m_nav(0)
    , m_apply(apply)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(QString::fromUtf8("设置"));
    setWindowIcon(RemoteTheme::icon("sliders", QColor(RemoteTheme::kPrimary), 16));
    resize(860, 620);

    m_nav = new QListWidget(this);
    m_nav->setObjectName("nav");
    m_nav->setFixedWidth(160);
    m_nav->setIconSize(QSize(15, 15));
    m_nav->setFocusPolicy(Qt::NoFocus);
    const char* const items[][2] = {
        { "link", "连接" }, { "shield", "安全" }, { "monitor", "画面" }, { "sliders", "服务" }
    };
    for (int i = 0; i < 4; ++i) {
        QListWidgetItem* item = new QListWidgetItem(
            RemoteTheme::icon(items[i][0], QColor("#595959")), QString::fromUtf8(items[i][1]), m_nav);
        item->setSizeHint(QSize(140, 38));
    }

    QStackedWidget* pages = 0;
    if (m_body) {
        pages = m_body->findChild<QStackedWidget*>("settingsPages");
        m_body->setParent(this);
        m_body->show();
    }

    QHBoxLayout* main = new QHBoxLayout();
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    main->addWidget(m_nav);
    if (m_body) {
        main->addWidget(m_body, 1);
    }

    QWidget* foot = new QWidget(this);
    foot->setObjectName("footer");
    foot->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* footRow = new QHBoxLayout(foot);
    footRow->setContentsMargins(20, 12, 20, 12);
    footRow->setSpacing(8);
    QPushButton* cancel = RemoteTheme::button(QString::fromUtf8("取消"), 0, 0, foot);
    QPushButton* applyBtn = RemoteTheme::button(QString::fromUtf8("应用"), 0, 0, foot);
    QPushButton* ok = RemoteTheme::button(QString::fromUtf8("确定"), 0, "primary", foot);
    applyBtn->setToolTip(QString::fromUtf8("保存配置；改了反连地址或端口会重连"));
    ok->setToolTip(QString::fromUtf8("保存并关闭"));
    footRow->addStretch(1);
    footRow->addWidget(cancel);
    footRow->addWidget(applyBtn);
    footRow->addWidget(ok);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(main, 1);
    root->addWidget(foot);

    if (pages) {
        QObject::connect(m_nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    }
    QObject::connect(cancel, &QPushButton::clicked, [this]() { reject(); });
    QObject::connect(applyBtn, &QPushButton::clicked, [this]() {
        if (m_apply) {
            m_apply();
        }
    });
    QObject::connect(ok, &QPushButton::clicked, [this]() {
        if (m_apply) {
            m_apply();
        }
        accept();
    });
    m_nav->setCurrentRow(0);
}

AgentSettingsDialog::~AgentSettingsDialog()
{
    if (m_body) {
        m_body->setParent(m_oldParent);
        m_body->hide();
    }
}

void AgentSettingsDialog::setPage(int page)
{
    if (m_nav && page >= 0 && page < m_nav->count()) {
        m_nav->setCurrentRow(page);
    }
}

}
