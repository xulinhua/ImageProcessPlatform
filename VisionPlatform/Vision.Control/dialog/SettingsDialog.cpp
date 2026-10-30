/**
 * @file SettingsDialog.cpp
 * @brief 设置
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SettingsDialog.h"
#include "ControlConfig.h"
#include "Messages.h"
#include "RemoteFiles.h"
#include "RemoteTheme.h"
#include "RemoteWidgets.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace Vision {

SettingsDialog::SettingsDialog(ControlConfig* config, bool listening, const QString& auditDir,
    const std::function<void(bool)>& applied, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_applied(applied)
    , m_nav(new QListWidget(this))
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(QString::fromUtf8("设置"));
    setWindowIcon(RemoteTheme::icon("sliders", QColor(RemoteTheme::kPrimary), 16));
    resize(960, 640);

    // 左侧导航，顺序与右侧页一致
    m_nav->setObjectName("nav");
    m_nav->setFixedWidth(180);
    m_nav->setIconSize(QSize(15, 15));
    m_nav->setFocusPolicy(Qt::NoFocus);
    const char* const items[][2] = {
        { "sliders", "常规" }, { "globe", "网络" }, { "monitor", "画面" },
        { "shield", "安全" }, { "film", "录屏" }, { "list", "日志" }
    };
    for (int i = 0; i < 6; ++i) {
        QListWidgetItem* item = new QListWidgetItem(RemoteTheme::icon(items[i][0], QColor("#595959")), QString::fromUtf8(items[i][1]), m_nav);
        item->setSizeHint(QSize(160, 38));
    }

    QStackedWidget* pages = new QStackedWidget(this);
    pages->addWidget(buildGeneral());
    pages->addWidget(buildNetwork(listening));
    pages->addWidget(buildPicture());
    pages->addWidget(buildSecurity());
    pages->addWidget(buildRecord());
    pages->addWidget(buildLog(auditDir));

    QHBoxLayout* main = new QHBoxLayout();
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    main->addWidget(m_nav);
    main->addWidget(pages, 1);

    // 底部：恢复默认、取消、应用、确定
    QWidget* foot = new QWidget(this);
    foot->setObjectName("footer");
    foot->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* footRow = new QHBoxLayout(foot);
    footRow->setContentsMargins(20, 12, 20, 12);
    footRow->setSpacing(8);
    QPushButton* reset = RemoteTheme::button(QString::fromUtf8("恢复默认"), 0, 0, foot);
    QPushButton* cancel = RemoteTheme::button(QString::fromUtf8("取消"), 0, 0, foot);
    QPushButton* apply = RemoteTheme::button(QString::fromUtf8("应用"), 0, 0, foot);
    QPushButton* ok = RemoteTheme::button(QString::fromUtf8("确定"), 0, "primary", foot);
    footRow->addWidget(reset);
    footRow->addStretch(1);
    footRow->addWidget(cancel);
    footRow->addWidget(apply);
    footRow->addWidget(ok);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(main, 1);
    root->addWidget(foot);

    // 恢复默认只改控件，要应用或确定才写回
    load(*m_config);
    QObject::connect(m_nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    QObject::connect(reset, &QPushButton::clicked, [this]() { load(ControlConfig()); });
    QObject::connect(cancel, &QPushButton::clicked, [this]() { reject(); });
    QObject::connect(apply, &QPushButton::clicked, [this]() { store(); });
    QObject::connect(ok, &QPushButton::clicked, [this]() {
        store();
        accept();
    });
    m_nav->setCurrentRow(0);
}

void SettingsDialog::setPage(int page)
{
    m_nav->setCurrentRow(page);
}

QWidget* SettingsDialog::page(QVBoxLayout** col)
{
    QWidget* body = new QWidget(this);
    body->setObjectName("white");
    body->setAttribute(Qt::WA_StyledBackground);
    QVBoxLayout* c = new QVBoxLayout(body);
    c->setContentsMargins(28, 20, 28, 20);
    c->setSpacing(18);
    *col = c;
    return body;
}

QGridLayout* SettingsDialog::group(QVBoxLayout* col, const QString& title)
{
    QLabel* head = new QLabel(title, col->parentWidget());
    head->setProperty("role", "section");
    col->addWidget(head);
    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(2, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(12);
    grid->setColumnMinimumWidth(0, 110);
    grid->setColumnStretch(1, 1);
    col->addLayout(grid);
    return grid;
}

void SettingsDialog::addRow(QGridLayout* grid, const QString& key, QWidget* value)
{
    const int row = grid->rowCount();
    QLabel* label = new QLabel(key, value->parentWidget());
    label->setProperty("role", "key");
    grid->addWidget(label, row, 0);
    grid->addWidget(value, row, 1);
}

QWidget* SettingsDialog::line(QWidget* a, QWidget* b, QWidget* c, QWidget* d)
{
    QWidget* box = new QWidget(a->parentWidget());
    QHBoxLayout* row = new QHBoxLayout(box);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    QWidget* items[] = { a, b, c, d };
    for (int i = 0; i < 4; ++i) {
        if (items[i]) {
            row->addWidget(items[i]);
        }
    }
    row->addStretch(1);
    return box;
}

QWidget* SettingsDialog::buildGeneral()
{
    QVBoxLayout* col = 0;
    QWidget* body = page(&col);
    QGridLayout* start = group(col, QString::fromUtf8("启动与托盘"));
    m_autoStart = new SwitchButton(body);
    m_hidden = new SwitchButton(body);
    m_tray = new SwitchButton(body);
    addRow(start, QString::fromUtf8("开机自动启动"), line(m_autoStart));
    addRow(start, QString::fromUtf8("启动时隐藏到托盘"), line(m_hidden));
    addRow(start, QString::fromUtf8("关闭时最小化到托盘"), line(m_tray));
    QGridLayout* who = group(col, QString::fromUtf8("身份"));
    QLabel* name = new QLabel(RemoteFiles::selfName(), body);
    QLabel* note = new QLabel(QString::fromUtf8("被控端会显示这个名字，取自 Windows 用户名和计算机名"), body);
    note->setProperty("role", "small");
    addRow(who, QString::fromUtf8("中控台显示名"), line(name, note));
    col->addStretch(1);
    return body;
}

QWidget* SettingsDialog::buildNetwork(bool listening)
{
    QVBoxLayout* col = 0;
    QWidget* body = page(&col);

    // 反连监听：状态取打开对话框时的
    QGridLayout* listen = group(col, QString::fromUtf8("反连监听"));
    m_host = new QLineEdit(body);
    m_host->setFixedWidth(200);
    m_host->setProperty("role", "mono");
    m_port = new QSpinBox(body);
    m_port->setRange(1, 65535);
    m_port->setFixedWidth(90);
    QLabel* portLabel = new QLabel(QString::fromUtf8("端口"), body);
    QLabel* state = new QLabel(listening ? QString::fromUtf8("✓ 监听中") : QString::fromUtf8("未监听"), body);
    state->setStyleSheet(listening ? "color:#389E0D;font-size:12px;" : "color:#CF1322;font-size:12px;");
    addRow(listen, QString::fromUtf8("监听地址"), line(m_host, portLabel, m_port, state));

    QGridLayout* relay = group(col, QString::fromUtf8("中继服务器"));
    m_relayOn = new SwitchButton(body);
    m_relayHost = new QLineEdit(body);
    m_relayHost->setFixedWidth(200);
    m_relayHost->setPlaceholderText("relay.company.local");
    m_relayPort = new QSpinBox(body);
    m_relayPort->setRange(1, 65535);
    m_relayPort->setValue(19200);
    m_relayPort->setFixedWidth(90);
    m_relayKey = new QLineEdit(body);
    m_relayKey->setEchoMode(QLineEdit::Password);
    m_relayKey->setFixedWidth(200);
    addRow(relay, QString::fromUtf8("启用中继"), line(m_relayOn));
    addRow(relay, QString::fromUtf8("地址"), line(m_relayHost, new QLabel(QString::fromUtf8("端口"), body), m_relayPort));
    addRow(relay, QString::fromUtf8("注册口令"), line(m_relayKey));

    // 连接顺序：占位，目前只有反连
    QGridLayout* order = group(col, QString::fromUtf8("连接顺序"));
    QWidget* orderBox = new QWidget(body);
    QVBoxLayout* orderCol = new QVBoxLayout(orderBox);
    orderCol->setContentsMargins(0, 0, 0, 0);
    orderCol->setSpacing(6);
    const char* const steps[] = { "已有反连", "局域网直连", "中继" };
    for (int i = 0; i < 3; ++i) {
        QFrame* item = new QFrame(orderBox);
        item->setStyleSheet("QFrame{border:1px solid #F0F0F0;border-radius:4px;}");
        item->setFixedWidth(420);
        QHBoxLayout* itemRow = new QHBoxLayout(item);
        itemRow->setContentsMargins(10, 6, 10, 6);
        itemRow->setSpacing(10);
        QLabel* n = new QLabel(QString::number(i + 1), item);
        n->setAlignment(Qt::AlignCenter);
        n->setFixedSize(20, 20);
        n->setStyleSheet("border:none;background:#E6F4FF;color:#1677FF;border-radius:10px;font-size:12px;");
        QCheckBox* use = new QCheckBox(QString::fromUtf8(steps[i]), item);
        use->setStyleSheet("border:none;");
        use->setChecked(true);
        use->setEnabled(false);
        use->setToolTip(QString::fromUtf8("顺序固定：已有反连，然后直连 2 秒，再中继"));
        itemRow->addWidget(n);
        itemRow->addWidget(use);
        itemRow->addStretch(1);
        if (i == 1) {
            QLabel* timeout = new QLabel(QString::fromUtf8("超时 2 秒"), item);
            timeout->setStyleSheet("border:none;color:#8C8C8C;font-size:12px;");
            itemRow->addWidget(timeout);
        }
        orderCol->addWidget(item);
    }
    orderBox->setToolTip(QString::fromUtf8("设备已在线就用反连；否则按地址直连 2 秒；再不行走中继"));
    order->addWidget(orderBox, 0, 0, 1, 2);

    // 发现与带宽：发现开关生效，UDP 端口只读，带宽上限占位
    QGridLayout* found = group(col, QString::fromUtf8("发现与带宽"));
    m_discovery = new SwitchButton(body);
    QLabel* udp = new QLabel(QString::fromUtf8("UDP 端口"), body);
    QLineEdit* udpPort = new QLineEdit(QString::number(RemoteConst::kDiscoverPort), body);
    udpPort->setReadOnly(true);
    udpPort->setFixedWidth(90);
    udpPort->setToolTip(QString::fromUtf8("发现端口固定，与被控端一致"));
    addRow(found, QString::fromUtf8("局域网发现"), line(m_discovery, udp, udpPort));
    QSlider* cap = new QSlider(Qt::Horizontal, body);
    cap->setRange(1, 10);
    cap->setValue(4);
    cap->setFixedWidth(220);
    QSpinBox* capValue = new QSpinBox(body);
    capValue->setRange(1, 10);
    capValue->setValue(4);
    capValue->setFixedWidth(70);
    QWidget* capRow = line(cap, capValue, new QLabel("Mbps", body));
    RemoteTheme::markLater(capRow, QString::fromUtf8("带宽上限在第三期（子码流）"));
    addRow(found, QString::fromUtf8("监控墙带宽上限"), capRow);
    col->addStretch(1);
    return body;
}

QWidget* SettingsDialog::buildPicture()
{
    QVBoxLayout* col = 0;
    QWidget* body = page(&col);
    QGridLayout* single = group(col, QString::fromUtf8("会话窗口"));
    m_fit = new SwitchButton(body);
    addRow(single, QString::fromUtf8("默认适应窗口"), line(m_fit, new QLabel(QString::fromUtf8("关闭时按 1:1 显示"), body)));
    QGridLayout* wall = group(col, QString::fromUtf8("监控墙"));
    m_rows = new QSpinBox(body);
    m_rows->setRange(1, 4);
    m_rows->setFixedWidth(60);
    m_cols = new QSpinBox(body);
    m_cols->setRange(1, 4);
    m_cols->setFixedWidth(60);
    addRow(wall, QString::fromUtf8("默认布局"), line(new QLabel(QString::fromUtf8("行"), body), m_rows, new QLabel(QString::fromUtf8("列"), body), m_cols));
    m_tour = new QSpinBox(body);
    m_tour->setRange(3, 600);
    m_tour->setSuffix(QString::fromUtf8(" 秒"));
    m_tour->setFixedWidth(90);
    addRow(wall, QString::fromUtf8("轮巡间隔"), line(m_tour));
    QGridLayout* shot = group(col, QString::fromUtf8("截图"));
    m_shotDir = new QLineEdit(body);
    m_shotDir->setMinimumWidth(360);
    QPushButton* browse = RemoteTheme::button(QString::fromUtf8("浏览…"), "folder", 0, body);
    addRow(shot, QString::fromUtf8("保存目录"), line(m_shotDir, browse));
    QObject::connect(browse, &QPushButton::clicked, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, QString::fromUtf8("截图目录"), m_shotDir->text());
        if (!dir.isEmpty()) {
            m_shotDir->setText(QDir::toNativeSeparators(dir));
        }
    });
    QGridLayout* codec = group(col, QString::fromUtf8("编码"));
    QLabel* now = new QLabel(QString::fromUtf8("JPEG · 会话内可调画质与帧率上限（默认 %1 fps）")
                                 .arg(RemoteConst::kMaxFpsDefault), body);
    TagLabel* later = new TagLabel(QString::fromUtf8("H.264 在第三期"), "gray", body);
    addRow(codec, QString::fromUtf8("当前"), line(now, later));
    col->addStretch(1);
    return body;
}

QWidget* SettingsDialog::buildSecurity()
{
    QVBoxLayout* col = 0;
    QWidget* body = page(&col);
    QGridLayout* link = group(col, QString::fromUtf8("传输"));
    addRow(link, QString::fromUtf8("加密"), line(new TagLabel(QString::fromUtf8("TLS（Schannel）"), "green", body)));
    QGridLayout* auth = group(col, QString::fromUtf8("认证"));
    QLabel* pinNote = new QLabel(QString::fromUtf8("会话信息页展示对端证书指纹；被控端可按指纹信任中控台"), body);
    pinNote->setProperty("role", "small");
    pinNote->setWordWrap(true);
    addRow(auth, QString::fromUtf8("证书指纹"), pinNote);
    QLabel* lockNote = new QLabel(QString::fromUtf8("连续 5 次认证失败锁定 5 分钟（被控端 RemoteAuth）"), body);
    lockNote->setProperty("role", "small");
    lockNote->setWordWrap(true);
    addRow(auth, QString::fromUtf8("失败锁定"), lockNote);
    QLabel* note = new QLabel(QString::fromUtf8("操作申请是否需要现场确认，由被控端的“安全”设置决定。"), body);
    note->setProperty("role", "small");
    col->addWidget(note);
    col->addStretch(1);
    return body;
}

QWidget* SettingsDialog::buildRecord()
{
    QVBoxLayout* col = 0;
    QWidget* body = page(&col);
    QGridLayout* rec = group(col, QString::fromUtf8("会话录屏"));
    m_autoRecord = new SwitchButton(body);
    m_recordDir = new QLineEdit(body);
    m_recordDir->setMinimumWidth(360);
    m_recordDays = new QSpinBox(body);
    m_recordDays->setRange(1, 3650);
    m_recordDays->setSuffix(QString::fromUtf8(" 天"));
    m_recordDays->setFixedWidth(90);
    addRow(rec, QString::fromUtf8("取得操作时自动录制"), line(m_autoRecord));
    addRow(rec, QString::fromUtf8("保存目录"), line(m_recordDir));
    addRow(rec, QString::fromUtf8("保留"), line(m_recordDays));
    col->addStretch(1);
    return body;
}

QWidget* SettingsDialog::buildLog(const QString& auditDir)
{
    QVBoxLayout* col = 0;
    QWidget* body = page(&col);
    QGridLayout* audit = group(col, QString::fromUtf8("审计日志"));
    m_auditDays = new QSpinBox(body);
    m_auditDays->setRange(7, 3650);
    m_auditDays->setSuffix(QString::fromUtf8(" 天"));
    m_auditDays->setFixedWidth(100);
    addRow(audit, QString::fromUtf8("保留"), line(m_auditDays));
    QLabel* dir = new QLabel(QDir::toNativeSeparators(auditDir), body);
    dir->setProperty("role", "mono");
    dir->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* open = RemoteTheme::button(QString::fromUtf8("打开目录"), "folder", 0, body);
    open->setProperty("size", "sm");
    addRow(audit, QString::fromUtf8("目录"), line(dir, open));
    QObject::connect(open, &QPushButton::clicked, [auditDir]() { QDesktopServices::openUrl(QUrl::fromLocalFile(auditDir)); });
    col->addStretch(1);
    return body;
}

void SettingsDialog::load(const ControlConfig& config)
{
    m_autoStart->setChecked(config.autoStart);
    m_hidden->setChecked(config.startHidden);
    m_tray->setChecked(config.trayOnClose);
    m_host->setText(config.host);
    m_port->setValue(config.port);
    m_discovery->setChecked(config.discovery);
    m_fit->setChecked(config.fitDefault);
    m_rows->setValue(config.rows);
    m_cols->setValue(config.cols);
    m_tour->setValue(config.tourSeconds);
    m_relayOn->setChecked(config.relayOn);
    m_relayHost->setText(config.relayHost);
    m_relayPort->setValue(config.relayPort > 0 ? config.relayPort : 19200);
    m_relayKey->setText(config.relayKey);
    m_shotDir->setText(QDir::toNativeSeparators(config.shotDir));
    m_autoRecord->setChecked(config.autoRecord);
    m_recordDir->setText(QDir::toNativeSeparators(config.recordDir.isEmpty() ? config.shotDir : config.recordDir));
    m_recordDays->setValue(config.recordDays > 0 ? config.recordDays : 90);
    m_auditDays->setValue(config.auditDays);
}

void SettingsDialog::store()
{
    const QString host = m_host->text().trimmed().isEmpty() ? QString("0.0.0.0") : m_host->text().trimmed();
    const bool moved = host != m_config->host || m_port->value() != m_config->port;
    m_config->autoStart = m_autoStart->isChecked();
    m_config->startHidden = m_hidden->isChecked();
    m_config->trayOnClose = m_tray->isChecked();
    m_config->host = host;
    m_config->port = m_port->value();
    m_config->discovery = m_discovery->isChecked();
    m_config->fitDefault = m_fit->isChecked();
    m_config->rows = m_rows->value();
    m_config->cols = m_cols->value();
    m_config->tourSeconds = m_tour->value();
    m_config->relayOn = m_relayOn->isChecked();
    m_config->relayHost = m_relayHost->text().trimmed();
    m_config->relayPort = m_relayPort->value();
    m_config->relayKey = m_relayKey->text();
    m_config->shotDir = m_shotDir->text().trimmed();
    m_config->autoRecord = m_autoRecord->isChecked();
    m_config->recordDir = m_recordDir->text().trimmed();
    m_config->recordDays = m_recordDays->value();
    m_config->auditDays = m_auditDays->value();
    m_config->save();
    m_config->purgeRecords();
    m_config->applyAutoStart();
    if (m_applied) {
        m_applied(moved);
    }
}

}
