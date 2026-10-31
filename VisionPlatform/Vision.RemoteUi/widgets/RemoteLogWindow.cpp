/**
 * @file RemoteLogWindow.cpp
 * @brief 运行日志窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteLogWindow.h"

#include "RemoteTheme.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cstring>

namespace Vision {

RemoteLogWindow::RemoteLogWindow(const QString& title, QWidget* parent)
    : QDialog(parent)
    , m_text(0)
    , m_pause(0)
    , m_pictureOnly(0)
    , m_hint(0)
    , m_timer(0)
    , m_seq(0)
    , m_shown(0)
{
    setWindowTitle(title);
    setWindowFlags(windowFlags() | Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(860, 520);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    QWidget* bar = new QWidget(this);
    QHBoxLayout* row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    m_pause = new QCheckBox(QString::fromUtf8("暂停"), bar);
    m_pictureOnly = new QCheckBox(QString::fromUtf8("只看画面链路"), bar);
    m_pictureOnly->setChecked(true);
    m_pictureOnly->setToolTip(QString::fromUtf8("只显示抓屏、编码、发送、接收、解码等耗时"));

    QPushButton* clear = RemoteTheme::button(QString::fromUtf8("清空"), "trash", 0, bar);
    QPushButton* folder = RemoteTheme::button(QString::fromUtf8("打开目录"), "folder", 0, bar);
    QPushButton* closeBtn = RemoteTheme::button(QString::fromUtf8("关闭"), 0, 0, bar);
    row->addWidget(m_pause);
    row->addWidget(m_pictureOnly);
    row->addStretch(1);
    row->addWidget(clear);
    row->addWidget(folder);
    row->addWidget(closeBtn);

    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setMaximumBlockCount(3000);
    m_text->setStyleSheet(QString::fromUtf8(
        "QPlainTextEdit{background:#1e1e1e;color:#d4d4d4;font-family:Consolas,'Microsoft YaHei UI';font-size:12px;}"));

    m_hint = new QLabel(this);
    m_hint->setProperty("role", "small");
    m_hint->setTextInteractionFlags(Qt::TextSelectableByMouse);

    root->addWidget(bar);
    root->addWidget(m_text, 1);
    root->addWidget(m_hint);

    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    QObject::connect(m_timer, &QTimer::timeout, [this]() { drain(); });
    QObject::connect(clear, &QPushButton::clicked, [this]() {
        RemoteLog::clear();
        m_text->clear();
        m_shown = 0;
        m_hint->setText(QString::fromUtf8("已清空"));
    });
    QObject::connect(folder, &QPushButton::clicked, [this]() { openFolder(); });
    QObject::connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    QObject::connect(m_pictureOnly, &QCheckBox::toggled, [this](bool) {
        m_seq = 0;
        m_text->clear();
        m_shown = 0;
        drain();
    });
}

void RemoteLogWindow::showAndRaise()
{
    show();
    raise();
    activateWindow();
    if (!m_timer->isActive()) {
        m_timer->start();
    }
    drain();
}

void RemoteLogWindow::closeEvent(QCloseEvent* event)
{
    hide();
    event->ignore();
}

bool RemoteLogWindow::acceptLine(const RemoteLog::Line& line) const
{
    if (!m_pictureOnly || !m_pictureOnly->isChecked()) {
        return true;
    }
    return std::strcmp(line.tag, "画面") == 0
        || std::strcmp(line.tag, "解码") == 0
        || std::strcmp(line.tag, "发送") == 0
        || std::strcmp(line.tag, "接收") == 0
        || std::strcmp(line.tag, "网络") == 0;
}

void RemoteLogWindow::drain()
{
    if (m_pause && m_pause->isChecked()) {
        return;
    }
    RemoteLog::Line batch[128];
    const size_t n = RemoteLog::pull(&m_seq, batch, 128);
    if (n == 0) {
        return;
    }
    const bool stick = m_text->verticalScrollBar()->value()
        >= m_text->verticalScrollBar()->maximum() - 4;
    QString chunk;
    chunk.reserve(static_cast<int>(n) * 96);
    for (size_t i = 0; i < n; ++i) {
        if (!acceptLine(batch[i])) {
            continue;
        }
        const char* lv = "I";
        if (batch[i].level == RemoteLog::Warn) {
            lv = "W";
        } else if (batch[i].level == RemoteLog::Error) {
            lv = "E";
        }
        chunk += QString::fromUtf8("%1 [%2] [%3] %4\n")
                     .arg(QString::fromUtf8(batch[i].time))
                     .arg(QString::fromUtf8(lv))
                     .arg(QString::fromUtf8(batch[i].tag))
                     .arg(QString::fromUtf8(batch[i].text));
        m_shown += 1;
    }
    if (!chunk.isEmpty()) {
        m_text->moveCursor(QTextCursor::End);
        m_text->insertPlainText(chunk);
        if (stick) {
            m_text->moveCursor(QTextCursor::End);
        }
    }
    m_hint->setText(QString::fromUtf8("已显示 %1 行 · %2")
                        .arg(m_shown)
                        .arg(QString::fromUtf8(RemoteLog::filePath())));
}

void RemoteLogWindow::openFolder()
{
    QString path = QString::fromUtf8(RemoteLog::filePath());
    if (path.isEmpty()) {
        return;
    }
    // 未开写盘时 filePath 是日志目录；开写盘时是具体文件
    const QFileInfo info(path);
    if (info.isFile()) {
        path = info.absolutePath();
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

}
