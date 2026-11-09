/**
 * @file DeviceDialog.cpp
 * @brief 设备属性
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeviceDialog.h"
#include "WakeOnLan.h"
#include "AuditLog.h"
#include "DeviceStore.h"
#include "Messages.h"
#include "RemoteTheme.h"
#include "RemoteWidgets.h"
#include "SessionWindow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace Vision {

DeviceDialog::DeviceDialog(DeviceStore* store, AuditLog* audit, ControlSession* session, const QString& key,
    const ControlSession::Tile* tile, QWidget* parent)
    : QDialog(parent)
    , m_store(store)
    , m_audit(audit)
    , m_session(session)
    , m_key(key)
    , m_online(tile && tile->online && !tile->peerId.empty())
{
    // 留状态快照（不要画面），读出现有标签
    if (tile) {
        m_tile = *tile;
        m_tile.jpeg.clear();
    }
    const DeviceStore::Device* device = m_store->find(m_key);
    if (device) {
        m_tags = device->tags;
    }
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(QString::fromUtf8("设备属性 — %1").arg(device ? m_store->displayName(*device) : m_key));
    setWindowIcon(RemoteTheme::icon("info", QColor(RemoteTheme::kPrimary), 16));
    setMinimumWidth(560);

    // 页签与四页
    QTabBar* tabs = new QTabBar(this);
    tabs->addTab(QString::fromUtf8("常规"));
    tabs->addTab(QString::fromUtf8("连接"));
    tabs->addTab(QString::fromUtf8("画面"));
    tabs->addTab(QString::fromUtf8("统计"));
    tabs->setExpanding(false);
    tabs->setDrawBase(false);
    QWidget* tabLine = new QWidget(this);
    tabLine->setObjectName("white");
    tabLine->setAttribute(Qt::WA_StyledBackground);
    tabLine->setStyleSheet("QWidget#white{border-bottom:1px solid #F0F0F0;}");
    QHBoxLayout* tabRow = new QHBoxLayout(tabLine);
    tabRow->setContentsMargins(22, 0, 0, 0);
    tabRow->addWidget(tabs);
    tabRow->addStretch(1);

    QStackedWidget* pages = new QStackedWidget(this);
    pages->addWidget(buildGeneral());
    pages->addWidget(buildConnection());
    pages->addWidget(buildPicture());
    pages->addWidget(buildStats());

    // 底部按钮与整体布局
    QWidget* foot = new QWidget(this);
    foot->setObjectName("footer");
    foot->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* footRow = new QHBoxLayout(foot);
    footRow->setContentsMargins(22, 12, 22, 12);
    footRow->setSpacing(8);
    QPushButton* cancel = RemoteTheme::button(QString::fromUtf8("取消"), 0, 0, foot);
    QPushButton* ok = RemoteTheme::button(QString::fromUtf8("确定"), 0, "primary", foot);
    footRow->addStretch(1);
    footRow->addWidget(cancel);
    footRow->addWidget(ok);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(tabLine);
    root->addWidget(pages, 1);
    root->addWidget(foot);

    QObject::connect(tabs, &QTabBar::currentChanged, pages, &QStackedWidget::setCurrentIndex);
    QObject::connect(cancel, &QPushButton::clicked, [this]() { reject(); });
    QObject::connect(ok, &QPushButton::clicked, [this]() { accept(); });
}

QWidget* DeviceDialog::page(QGridLayout** grid)
{
    QWidget* body = new QWidget(this);
    body->setObjectName("white");
    body->setAttribute(Qt::WA_StyledBackground);
    QGridLayout* g = new QGridLayout(body);
    g->setContentsMargins(22, 18, 22, 18);
    g->setHorizontalSpacing(12);
    g->setVerticalSpacing(14);
    g->setColumnMinimumWidth(0, 84);
    g->setColumnStretch(1, 1);
    *grid = g;
    return body;
}

void DeviceDialog::addRow(QGridLayout* grid, int row, const QString& key, QWidget* value)
{
    QLabel* label = new QLabel(key, grid->parentWidget());
    label->setProperty("role", "key");
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(label, row, 0);
    grid->addWidget(value, row, 1);
}

QWidget* DeviceDialog::line(QWidget* parent, QWidget* a, QWidget* b, QWidget* c)
{
    QWidget* box = new QWidget(parent);
    QHBoxLayout* row = new QHBoxLayout(box);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    QWidget* items[] = { a, b, c };
    for (int i = 0; i < 3; ++i) {
        if (items[i]) {
            row->addWidget(items[i]);
        }
    }
    row->addStretch(1);
    return box;
}

QWidget* DeviceDialog::buildGeneral()
{
    QGridLayout* grid = 0;
    QWidget* body = page(&grid);
    const DeviceStore::Device* device = m_store->find(m_key);

    m_name = new QLineEdit(body);
    m_name->setText(device ? m_store->displayName(*device) : m_key);
    m_name->setPlaceholderText(QString::fromUtf8("留空则用被控端上报的名称"));

    // 设备 ID：优先在线上报的，其次 key 本身（不含冒号即为设备 ID）
    const QString id = !m_tile.deviceId.empty() ? QString::fromUtf8(m_tile.deviceId.c_str())
        : (m_key.contains(':') ? QString() : m_key);
    QLabel* idLabel = new QLabel(id.isEmpty() ? QString::fromUtf8("被控端未上报") : id, body);
    idLabel->setProperty("role", "mono");
    idLabel->setStyleSheet(id.isEmpty() ? "color:#8C8C8C;" : "font-size:15px;letter-spacing:1px;");
    idLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* copy = RemoteTheme::button(QString(), "copy", "icon", body);
    copy->setProperty("size", "sm");
    copy->setToolTip(QString::fromUtf8("复制"));
    copy->setEnabled(!id.isEmpty());
    QObject::connect(copy, &QPushButton::clicked, [id]() { QApplication::clipboard()->setText(id); });

    // 分组、备注、标签
    m_group = new QComboBox(body);
    m_group->addItem(QString::fromUtf8("未分组"), QString());
    const QStringList groups = m_store->groups();
    for (int i = 0; i < groups.size(); ++i) {
        m_group->addItem(groups.at(i), groups.at(i));
    }
    if (device) {
        const int at = m_group->findData(device->group);
        m_group->setCurrentIndex(at < 0 ? 0 : at);
    }

    m_remark = new QLineEdit(body);
    m_remark->setText(device ? device->remark : QString());
    m_remark->setPlaceholderText(QString::fromUtf8("工位、位置、用途"));

    m_tagBox = new QWidget(body);
    m_tagRow = new QHBoxLayout(m_tagBox);
    m_tagRow->setContentsMargins(0, 0, 0, 0);
    m_tagRow->setSpacing(6);
    rebuildTags();

    QLabel* section = new QLabel(QString::fromUtf8("连接与认证"), body);
    section->setProperty("role", "section");

    m_linkAuto = new QRadioButton(QString::fromUtf8("自动"), body);
    m_linkLan = new QRadioButton(QString::fromUtf8("仅局域网"), body);
    m_linkRelay = new QRadioButton(QString::fromUtf8("仅中继"), body);
    m_linkReverse = new QRadioButton(QString::fromUtf8("反连优先"), body);
    QButtonGroup* linkGroup = new QButtonGroup(body);
    linkGroup->addButton(m_linkAuto, 0);
    linkGroup->addButton(m_linkLan, 1);
    linkGroup->addButton(m_linkRelay, 2);
    linkGroup->addButton(m_linkReverse, 3);
    const int linkMode = device ? device->linkMode : 0;
    if (linkMode == 1) {
        m_linkLan->setChecked(true);
    } else if (linkMode == 2) {
        m_linkRelay->setChecked(true);
    } else if (linkMode == 3) {
        m_linkReverse->setChecked(true);
    } else {
        m_linkAuto->setChecked(true);
    }
    QWidget* linkRow = line(body, m_linkAuto, m_linkLan, m_linkRelay);
    QHBoxLayout* linkExtra = new QHBoxLayout();
    linkExtra->setContentsMargins(0, 0, 0, 0);
    linkExtra->addWidget(m_linkReverse);
    linkExtra->addStretch(1);
    QWidget* linkWrap = new QWidget(body);
    QVBoxLayout* linkCol = new QVBoxLayout(linkWrap);
    linkCol->setContentsMargins(0, 0, 0, 0);
    linkCol->setSpacing(4);
    linkCol->addWidget(linkRow);
    linkCol->addLayout(linkExtra);
    linkWrap->setToolTip(QString::fromUtf8("自动：跟全局连接顺序；仅局域网/仅中继会改拨号步骤"));

    QLabel* authHint = new QLabel(QString::fromUtf8("无人值守密码"), body);
    authHint->setProperty("role", "key");
    m_password = new QLineEdit(body);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setFixedWidth(150);
    m_password->setPlaceholderText(QString::fromUtf8("申请操作时可自动填入"));
    m_password->setText(device ? device->password : QString());
    m_rememberPassword = new QCheckBox(QString::fromUtf8("记住"), body);
    m_rememberPassword->setChecked(device && device->rememberPassword);
    QWidget* authRow = line(body, authHint, m_password, m_rememberPassword);

    m_quality = new QComboBox(body);
    m_quality->addItem(QString::fromUtf8("跟全局设置"), -1);
    m_quality->addItem(QString::fromUtf8("流畅"), 0);
    m_quality->addItem(QString::fromUtf8("均衡"), 1);
    m_quality->addItem(QString::fromUtf8("清晰"), 2);
    m_quality->addItem(QString::fromUtf8("自定义"), 3);
    m_quality->setFixedWidth(160);
    const int q = device ? device->quality : -1;
    const int qAt = m_quality->findData(q);
    m_quality->setCurrentIndex(qAt < 0 ? 0 : qAt);
    QWidget* qualityRow = line(body, m_quality);
    qualityRow->setToolTip(QString::fromUtf8("打开会话时的默认画质档"));

    m_highestPrivilege = new QComboBox(body);
    m_highestPrivilege->addItem(QString::fromUtf8("跟全局/账户"), -1);
    m_highestPrivilege->addItem(QString::fromUtf8("开启"), 1);
    m_highestPrivilege->addItem(QString::fromUtf8("关闭"), 0);
    m_highestPrivilege->setFixedWidth(160);
    const int hp = device ? device->highestPrivilege : -1;
    const int hpAt = m_highestPrivilege->findData(hp);
    m_highestPrivilege->setCurrentIndex(hpAt < 0 ? 0 : hpAt);
    m_highestPrivilege->setToolTip(QString::fromUtf8(
        "全局开时一律生效；全局关后本项优先，再跟账户。开则连上自动满权限操作，可手动释放"));

    m_mac = new QLineEdit(body);
    m_mac->setPlaceholderText("00:11:22:33:44:55");
    m_mac->setText(device ? device->mac : QString());
    m_mac->setFixedWidth(180);
    QPushButton* wake = RemoteTheme::button(QString::fromUtf8("远程唤醒"), "zap", 0, body);
    wake->setProperty("size", "sm");
    wake->setToolTip(QString::fromUtf8("向这个 MAC 发魔术包。机器要在 BIOS 里打开网络唤醒"));
    QObject::connect(wake, &QPushButton::clicked, [this]() {
        WakeOnLan wol;
        QString err;
        if (!wol.send(m_mac->text(), &err)) {
            QMessageBox::warning(this, windowTitle(), err);
        } else {
            QMessageBox::information(this, windowTitle(), QString::fromUtf8("已发送唤醒包"));
        }
    });

    QString peerFp;
    if (m_online && m_session) {
        peerFp = QString::fromUtf8(m_session->peerFingerprint(m_tile.peerId).c_str());
    }
    TagLabel* fingerprint = new TagLabel(
        peerFp.isEmpty() ? QString::fromUtf8(m_online ? "未握手" : "离线") : peerFp.left(16) + QString("…"),
        peerFp.isEmpty() ? "gray" : "green", body);
    fingerprint->setToolTip(peerFp.isEmpty()
        ? QString::fromUtf8("连接后显示被控端证书 SHA-256 指纹")
        : peerFp);

    // 系统：库里记录优先，在线时附显示器块数
    QString systemText = device && !device->system.isEmpty() ? device->system : QString::fromUtf8(m_tile.system.c_str());
    if (m_tile.monitors > 0 && m_online) {
        systemText += QString::fromUtf8("%1%2 块显示器").arg(systemText.isEmpty() ? "" : " · ").arg(m_tile.monitors);
    }
    QLabel* system = new QLabel(systemText.isEmpty() ? QString("-") : systemText, body);
    system->setProperty("role", "muted");
    system->setWordWrap(true);

    int row = 0;
    addRow(grid, row++, QString::fromUtf8("名称"), m_name);
    addRow(grid, row++, QString::fromUtf8("设备 ID"), line(body, idLabel, copy));
    addRow(grid, row++, QString::fromUtf8("分组"), m_group);
    addRow(grid, row++, QString::fromUtf8("备注"), m_remark);
    addRow(grid, row++, QString::fromUtf8("标签"), m_tagBox);
    grid->addWidget(section, row++, 0, 1, 2);
    addRow(grid, row++, QString::fromUtf8("连接方式"), linkWrap);
    addRow(grid, row++, QString::fromUtf8("认证"), authRow);
    addRow(grid, row++, QString::fromUtf8("默认画质"), qualityRow);
    addRow(grid, row++, QString::fromUtf8("最高权限"), line(body, m_highestPrivilege));
    addRow(grid, row++, "MAC", line(body, m_mac, wake));
    addRow(grid, row++, QString::fromUtf8("证书指纹"), line(body, fingerprint));
    addRow(grid, row++, QString::fromUtf8("系统"), system);
    grid->setRowStretch(row, 1);
    return body;
}

void DeviceDialog::rebuildTags()
{
    // 在标签按钮自己的 clicked 里调用，旧按钮只能延后删除
    while (QLayoutItem* item = m_tagRow->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    for (int i = 0; i < m_tags.size(); ++i) {
        const QString tag = m_tags.at(i);
        QPushButton* chip = new QPushButton(tag, m_tagBox);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setFocusPolicy(Qt::NoFocus);
        chip->setToolTip(QString::fromUtf8("点击删除"));
        chip->setStyleSheet(i == 0
            ? "QPushButton{background:#E6F4FF;color:#1677FF;border:none;border-radius:3px;padding:1px 6px;min-height:18px;font-size:11px;}"
            : "QPushButton{background:#F0F0F0;color:#595959;border:none;border-radius:3px;padding:1px 6px;min-height:18px;font-size:11px;}");
        QObject::connect(chip, &QPushButton::clicked, [this, tag]() {
            m_tags.removeAll(tag);
            rebuildTags();
        });
        m_tagRow->addWidget(chip);
    }
    QPushButton* add = new QPushButton(QString::fromUtf8("+ 标签"), m_tagBox);
    add->setCursor(Qt::PointingHandCursor);
    add->setFocusPolicy(Qt::NoFocus);
    add->setStyleSheet("QPushButton{background:#FFFFFF;color:#BFBFBF;border:1px dashed #D9D9D9;border-radius:3px;padding:1px 6px;min-height:18px;font-size:11px;}"
                       "QPushButton:hover{color:#1677FF;border-color:#1677FF;}");
    QObject::connect(add, &QPushButton::clicked, [this]() {
        bool ok = false;
        const QString text = QInputDialog::getText(this, QString::fromUtf8("添加标签"), QString::fromUtf8("标签"),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (ok && !text.isEmpty() && !m_tags.contains(text)) {
            m_tags.append(text);
            rebuildTags();
        }
    });
    m_tagRow->addWidget(add);
    m_tagRow->addStretch(1);
}

QWidget* DeviceDialog::buildConnection()
{
    QGridLayout* grid = 0;
    QWidget* body = page(&grid);
    const DeviceStore::Device* device = m_store->find(m_key);
    const QString address = !m_tile.address.empty() ? QString::fromUtf8(m_tile.address.c_str())
        : (device ? device->address : QString());
    QLabel* addressLabel = new QLabel(address.isEmpty() ? QString("-") : address, body);
    addressLabel->setProperty("role", "mono");
    TagLabel* state = new TagLabel(m_online ? QString::fromUtf8("在线") : QString::fromUtf8("离线"), m_online ? "green" : "gray", body);
    QLabel* way = new QLabel(QString::fromUtf8("反连：被控端主动连到本机监听端口"), body);
    QLabel* seen = new QLabel(device && device->lastSeen.isValid() ? device->lastSeen.toString("yyyy-MM-dd HH:mm:ss") : QString("-"), body);
    QLabel* opened = new QLabel(device && device->lastOpened.isValid() ? device->lastOpened.toString("yyyy-MM-dd HH:mm:ss") : QString("-"), body);
    QLabel* rtt = new QLabel(m_online && m_tile.rttMs >= 0 ? QString("%1 ms").arg(m_tile.rttMs) : QString("-"), body);
    QLabel* share = new QLabel(m_online ? SessionWindow::shareText(m_tile) : QString("-"), body);
    QString peerFp;
    if (m_online && m_session) {
        peerFp = QString::fromUtf8(m_session->peerFingerprint(m_tile.peerId).c_str());
    }
    QLabel* crypto = new QLabel(peerFp.isEmpty()
        ? QString::fromUtf8("TLS（Schannel）。连接后显示对端证书指纹")
        : QString::fromUtf8("TLS · 指纹 %1").arg(peerFp), body);
    crypto->setProperty("role", "muted");
    crypto->setWordWrap(true);
    crypto->setTextInteractionFlags(Qt::TextSelectableByMouse);

    int row = 0;
    addRow(grid, row++, QString::fromUtf8("地址"), addressLabel);
    addRow(grid, row++, QString::fromUtf8("状态"), line(body, state));
    addRow(grid, row++, QString::fromUtf8("连接方式"), way);
    addRow(grid, row++, QString::fromUtf8("当前共享"), share);
    addRow(grid, row++, QString::fromUtf8("延迟"), rtt);
    addRow(grid, row++, QString::fromUtf8("最后在线"), seen);
    addRow(grid, row++, QString::fromUtf8("最近打开"), opened);
    addRow(grid, row++, QString::fromUtf8("加密"), crypto);
    grid->setRowStretch(row, 1);
    return body;
}

QWidget* DeviceDialog::buildPicture()
{
    QGridLayout* grid = 0;
    QWidget* body = page(&grid);
    m_desktop = new QRadioButton(QString::fromUtf8("整个桌面"), body);
    m_window = new QRadioButton(QString::fromUtf8("指定窗口"), body);
    QButtonGroup* kind = new QButtonGroup(body);
    kind->addButton(m_desktop);
    kind->addButton(m_window);
    (m_tile.share == 1 ? m_desktop : m_window)->setChecked(true);
    m_monitor = new QComboBox(body);
    const int monitors = m_tile.monitors < 1 ? 1 : m_tile.monitors;
    for (int i = 0; i < monitors; ++i) {
        m_monitor->addItem(QString::fromUtf8("显示器 %1").arg(i + 1));
    }
    m_monitor->setCurrentIndex(m_tile.monitor < monitors ? m_tile.monitor : 0);
    m_monitor->setFixedWidth(160);
    m_title = new QLineEdit(QString::fromUtf8(m_tile.title.c_str()), body);
    m_title->setPlaceholderText(QString::fromUtf8("窗口标题包含的文字"));
    m_process = new QLineEdit(QString::fromUtf8(m_tile.process.c_str()), body);
    m_process->setPlaceholderText(QString::fromUtf8("例如 Vision.exe"));
    QPushButton* apply = RemoteTheme::button(QString::fromUtf8("下发到被控端"), "send", "primary", body);
    m_pictureHint = new QLabel(m_online
        ? QString::fromUtf8("被控端收到后立即切换，并写入它自己的配置文件。")
        : QString::fromUtf8("设备不在线，只能设置在线的被控端。"), body);
    m_pictureHint->setProperty("role", "small");
    m_pictureHint->setWordWrap(true);

    int row = 0;
    addRow(grid, row++, QString::fromUtf8("共享范围"), line(body, m_desktop, m_window));
    addRow(grid, row++, QString::fromUtf8("显示器"), line(body, m_monitor));
    addRow(grid, row++, QString::fromUtf8("窗口标题"), m_title);
    addRow(grid, row++, QString::fromUtf8("进程名"), m_process);
    addRow(grid, row++, QString(), line(body, apply));
    addRow(grid, row++, QString(), m_pictureHint);
    grid->setRowStretch(row, 1);

    auto sync = [this, apply]() {
        const bool desktop = m_desktop->isChecked();
        m_desktop->setEnabled(m_online);
        m_window->setEnabled(m_online);
        m_monitor->setEnabled(m_online && desktop);
        m_title->setEnabled(m_online && !desktop);
        m_process->setEnabled(m_online && !desktop);
        apply->setEnabled(m_online);
    };
    sync();
    QObject::connect(m_desktop, &QRadioButton::toggled, sync);
    QObject::connect(apply, &QPushButton::clicked, [this]() { applyPicture(); });
    return body;
}

void DeviceDialog::applyPicture()
{
    if (!m_online) {
        return;
    }
    ConfigMsg cfg;
    cfg.share = m_desktop->isChecked() ? 1 : 0;
    cfg.monitor = static_cast<uint8_t>(m_monitor->currentIndex() < 0 ? 0 : m_monitor->currentIndex());
    cfg.title = m_title->text().trimmed().toUtf8().constData();
    cfg.process = m_process->text().trimmed().toUtf8().constData();
    cfg.maxFps = static_cast<uint16_t>(RemoteConst::kMaxFpsDefault);
    cfg.jpegQuality = static_cast<uint8_t>(RemoteConst::kJpegQuality);
    cfg.longEdge = static_cast<uint16_t>(RemoteConst::kLongEdge);
    if (cfg.share != 1 && cfg.title.empty() && cfg.process.empty()) {
        m_pictureHint->setText(QString::fromUtf8("共享窗口时，窗口标题和进程名至少填一个。"));
        return;
    }
    m_session->pushConfig(m_tile.peerId, cfg);
    const QString detail = cfg.share == 1
        ? QString::fromUtf8("桌面 · 显示器 %1").arg(cfg.monitor + 1)
        : QString::fromUtf8("窗口 · %1 %2").arg(m_title->text().trimmed(), m_process->text().trimmed()).trimmed();
    m_audit->add("config.push", m_name->text().trimmed(), QString::fromUtf8(m_tile.deviceId.c_str()),
        SessionWindow::shareText(m_tile), detail);
    m_pictureHint->setText(QString::fromUtf8("已下发：%1").arg(detail));
}

QWidget* DeviceDialog::buildStats()
{
    QGridLayout* grid = 0;
    QWidget* body = page(&grid);
    const DeviceStore::Device* device = m_store->find(m_key);
    const QString name = device ? m_store->displayName(*device) : m_key;
    const QString id = QString::fromUtf8(m_tile.deviceId.c_str());
    const QDate today = QDate::currentDate();
    const std::vector<AuditLog::Entry> entries = m_audit->query(today.addDays(-29), today);
    int sessions = 0;
    int operates = 0;
    int rejects = 0;
    int shots = 0;
    qint64 seconds = 0;
    QDateTime last;
    // 归属本设备：设备 ID 与上报 ID 或 key 相同，或记录无 ID 时按显示名匹配
    for (size_t i = 0; i < entries.size(); ++i) {
        const AuditLog::Entry& e = entries[i];
        const bool mine = (!id.isEmpty() && e.deviceId == id) || (!m_key.contains(':') && e.deviceId == m_key)
            || (e.deviceId.isEmpty() && e.device == name);
        if (!mine) {
            continue;
        }
        if (e.event == "session.start") {
            sessions += 1;
            if (!last.isValid() || e.time > last) {
                last = e.time;
            }
        } else if (e.event == "session.end") {
            seconds += e.seconds;
        } else if (e.event == "operate.start") {
            operates += 1;
        } else if (e.event == "operate.reject") {
            rejects += 1;
        } else if (e.event == "screenshot") {
            shots += 1;
        }
    }
    QLabel* range = new QLabel(QString::fromUtf8("近 30 天，来自审计日志"), body);
    range->setProperty("role", "small");
    int row = 0;
    grid->addWidget(range, row++, 0, 1, 2);
    addRow(grid, row++, QString::fromUtf8("会话"), new QLabel(QString::number(sessions), body));
    addRow(grid, row++, QString::fromUtf8("在线时长"), new QLabel(AuditLog::duration(seconds), body));
    addRow(grid, row++, QString::fromUtf8("取得操作"), new QLabel(QString::number(operates), body));
    addRow(grid, row++, QString::fromUtf8("操作被拒"), new QLabel(QString::number(rejects), body));
    addRow(grid, row++, QString::fromUtf8("截图"), new QLabel(QString::number(shots), body));
    addRow(grid, row++, QString::fromUtf8("最近会话"), new QLabel(last.isValid() ? last.toString("yyyy-MM-dd HH:mm") : QString("-"), body));
    grid->setRowStretch(row, 1);
    return body;
}

void DeviceDialog::accept()
{
    DeviceStore::Device* device = m_store->find(m_key);
    if (device) {
        const QString name = m_name->text().trimmed();
        device->alias = name == device->name ? QString() : name;
        device->group = m_group->currentData().toString();
        device->remark = m_remark->text().trimmed();
        if (m_mac) {
            device->mac = m_mac->text().trimmed();
        }
        if (m_linkLan && m_linkLan->isChecked()) {
            device->linkMode = 1;
        } else if (m_linkRelay && m_linkRelay->isChecked()) {
            device->linkMode = 2;
        } else if (m_linkReverse && m_linkReverse->isChecked()) {
            device->linkMode = 3;
        } else {
            device->linkMode = 0;
        }
        device->quality = m_quality ? m_quality->currentData().toInt() : -1;
        device->highestPrivilege = m_highestPrivilege ? m_highestPrivilege->currentData().toInt() : -1;
        device->rememberPassword = m_rememberPassword && m_rememberPassword->isChecked();
        device->password = device->rememberPassword && m_password ? m_password->text() : QString();
        device->tags = m_tags;
        m_store->save();
    }
    QDialog::accept();
}

}
