/**
 * @file AddDeviceDialog.cpp
 * @brief 添加设备
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AddDeviceDialog.h"
#include "ControlConfig.h"
#include "DeviceStore.h"
#include "RemoteTheme.h"
#include "RemoteWidgets.h"

#include <QApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegExp>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace Vision {

AddDeviceDialog::AddDeviceDialog(const DeviceStore* store, const ControlConfig* config,
    const std::function<std::vector<HelloMsg>()>& scanner, const QString& group, QWidget* parent)
    : QDialog(parent)
    , m_store(store)
    , m_config(config)
    , m_scanner(scanner)
    , m_deviceId(0)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(QString::fromUtf8("添加设备"));
    setWindowIcon(RemoteTheme::icon("plus", QColor(RemoteTheme::kPrimary), 16));
    resize(700, 520);

    // 页签：局域网扫描 / 按 IP / 按设备 ID（中继 OPEN）
    m_tabs = new QTabBar(this);
    m_tabs->addTab(QString::fromUtf8("局域网扫描"));
    m_tabs->addTab(QString::fromUtf8("按 IP 地址"));
    m_tabs->addTab(QString::fromUtf8("按设备 ID"));
    m_tabs->setTabToolTip(2, QString::fromUtf8("通过中继 OPEN 命令连接，需在设置里启用中继"));
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    QWidget* tabLine = new QWidget(this);
    tabLine->setObjectName("white");
    tabLine->setAttribute(Qt::WA_StyledBackground);
    tabLine->setStyleSheet("QWidget#white{border-bottom:1px solid #F0F0F0;}");
    QHBoxLayout* tabRow = new QHBoxLayout(tabLine);
    tabRow->setContentsMargins(22, 0, 0, 0);
    tabRow->addWidget(m_tabs);
    tabRow->addStretch(1);

    QStackedWidget* pages = new QStackedWidget(this);
    pages->addWidget(buildScan());
    pages->addWidget(buildAddress());
    pages->addWidget(buildId());

    // 分组与认证行
    QWidget* groupBar = new QWidget(this);
    groupBar->setObjectName("white");
    groupBar->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* groupRow = new QHBoxLayout(groupBar);
    groupRow->setContentsMargins(22, 0, 22, 16);
    groupRow->setSpacing(8);
    m_group = new QComboBox(groupBar);
    m_group->addItem(QString::fromUtf8("未分组"), QString());
    const QStringList groups = m_store->groups();
    for (int i = 0; i < groups.size(); ++i) {
        m_group->addItem(groups.at(i), groups.at(i));
    }
    const int at = m_group->findData(group);
    m_group->setCurrentIndex(at < 0 ? 0 : at);
    m_group->setFixedWidth(180);
    QComboBox* auth = new QComboBox(groupBar);
    auth->addItem(QString::fromUtf8("验证码 / 密码 / 信任设备"));
    auth->setFixedWidth(180);
    auth->setToolTip(QString::fromUtf8("申请操作时在会话窗口选择认证方式"));
    QLabel* groupLabel = new QLabel(QString::fromUtf8("加入分组"), groupBar);
    groupLabel->setProperty("role", "key");
    QLabel* authLabel = new QLabel(QString::fromUtf8("认证"), groupBar);
    authLabel->setProperty("role", "key");
    groupRow->addWidget(groupLabel);
    groupRow->addWidget(m_group);
    groupRow->addSpacing(12);
    groupRow->addWidget(authLabel);
    groupRow->addWidget(auth);
    groupRow->addStretch(1);

    // 底部：计数、取消、添加
    QWidget* foot = new QWidget(this);
    foot->setObjectName("footer");
    foot->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* footRow = new QHBoxLayout(foot);
    footRow->setContentsMargins(22, 12, 22, 12);
    footRow->setSpacing(8);
    m_count = new QLabel(foot);
    m_count->setProperty("role", "small");
    QPushButton* cancel = RemoteTheme::button(QString::fromUtf8("取消"), 0, 0, foot);
    m_ok = RemoteTheme::button(QString::fromUtf8("添加"), 0, "primary", foot);
    footRow->addWidget(m_count);
    footRow->addStretch(1);
    footRow->addWidget(cancel);
    footRow->addWidget(m_ok);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(tabLine);
    root->addWidget(pages, 1);
    root->addWidget(groupBar);
    root->addWidget(foot);

    // 信号；首次扫描用 singleShot 推迟，先让对话框显示出来
    QObject::connect(m_tabs, &QTabBar::currentChanged, [this, pages](int index) {
        pages->setCurrentIndex(index);
        syncFoot();
    });
    QObject::connect(cancel, &QPushButton::clicked, [this]() { reject(); });
    QObject::connect(m_ok, &QPushButton::clicked, [this]() {
        if (!picks().empty()) {
            accept();
        }
    });
    syncFoot();
    QTimer::singleShot(0, this, [this]() { rescan(); });
}

QWidget* AddDeviceDialog::buildScan()
{
    QWidget* body = new QWidget(this);
    body->setObjectName("white");
    body->setAttribute(Qt::WA_StyledBackground);
    QVBoxLayout* col = new QVBoxLayout(body);
    col->setContentsMargins(22, 18, 22, 14);
    col->setSpacing(12);

    // 筛选框、重新扫描、扫描提示
    QHBoxLayout* top = new QHBoxLayout();
    top->setSpacing(8);
    m_filter = new QLineEdit(body);
    m_filter->setPlaceholderText(QString::fromUtf8("筛选名称或 IP"));
    m_filter->addAction(RemoteTheme::icon("search", QColor(RemoteTheme::kMuted), 14), QLineEdit::LeadingPosition);
    QPushButton* again = RemoteTheme::button(QString::fromUtf8("重新扫描"), "refresh", 0, body);
    top->addWidget(m_filter, 1);
    top->addWidget(again);
    col->addLayout(top);

    QHBoxLayout* hintRow = new QHBoxLayout();
    hintRow->setSpacing(6);
    QLabel* wifi = new QLabel(body);
    wifi->setPixmap(RemoteTheme::pixmap("wifi", QColor(RemoteTheme::kMuted), 13, devicePixelRatioF()));
    m_hint = new QLabel(body);
    m_hint->setProperty("role", "small");
    hintRow->addWidget(wifi);
    hintRow->addWidget(m_hint, 1);
    col->addLayout(hintRow);

    // 结果表：勾选、名称、IP、设备 ID、系统、状态
    m_table = new QTableWidget(body);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(QStringList() << QString() << QString::fromUtf8("名称") << QString::fromUtf8("IP 地址")
                                                     << QString::fromUtf8("设备 ID") << QString::fromUtf8("系统") << QString::fromUtf8("状态"));
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(36);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(0, 34);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->resizeSection(2, 120);
    m_table->horizontalHeader()->resizeSection(3, 110);
    m_table->horizontalHeader()->resizeSection(4, 130);
    m_table->horizontalHeader()->resizeSection(5, 70);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->setStyleSheet("QTableWidget{border:1px solid #F0F0F0;border-radius:6px;}");
    col->addWidget(m_table, 1);

    // 改筛选即重填；点行内其他列也能切换勾选
    QObject::connect(again, &QPushButton::clicked, [this]() { rescan(); });
    QObject::connect(m_filter, &QLineEdit::textChanged, [this](const QString&) { fill(); });
    QObject::connect(m_table, &QTableWidget::itemChanged, [this](QTableWidgetItem*) { syncFoot(); });
    QObject::connect(m_table, &QTableWidget::cellClicked, [this](int row, int column) {
        QTableWidgetItem* check = m_table->item(row, 0);
        if (column != 0 && check && (check->flags() & Qt::ItemIsEnabled)) {
            check->setCheckState(check->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        }
    });
    return body;
}

QWidget* AddDeviceDialog::buildAddress()
{
    QWidget* body = new QWidget(this);
    body->setObjectName("white");
    body->setAttribute(Qt::WA_StyledBackground);
    QGridLayout* grid = new QGridLayout(body);
    grid->setContentsMargins(22, 18, 22, 18);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(14);
    m_ipName = new QLineEdit(body);
    m_ipName->setPlaceholderText(QString::fromUtf8("例如 检测二号"));
    m_ip = new QLineEdit(body);
    m_ip->setPlaceholderText("192.168.1.12");
    QLabel* nameLabel = new QLabel(QString::fromUtf8("名称"), body);
    nameLabel->setProperty("role", "key");
    QLabel* ipLabel = new QLabel(QString::fromUtf8("IP 地址"), body);
    ipLabel->setProperty("role", "key");
    QLabel* note = new QLabel(QString::fromUtf8(
        "按共享整个桌面登记。被控端需要把中控台地址设成本机、端口 %1，连上后自动出画面。").arg(RemoteConst::kPort), body);
    note->setProperty("role", "small");
    note->setWordWrap(true);
    grid->addWidget(nameLabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(m_ipName, 0, 1);
    grid->addWidget(ipLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(m_ip, 1, 1);
    grid->addWidget(note, 2, 1);
    grid->setColumnMinimumWidth(0, 84);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(3, 1);
    QObject::connect(m_ip, &QLineEdit::textChanged, [this](const QString&) { syncFoot(); });
    return body;
}

QWidget* AddDeviceDialog::buildId()
{
    QWidget* body = new QWidget(this);
    body->setObjectName("white");
    body->setAttribute(Qt::WA_StyledBackground);
    QGridLayout* grid = new QGridLayout(body);
    grid->setContentsMargins(22, 18, 22, 18);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(14);
    m_deviceId = new QLineEdit(body);
    m_deviceId->setPlaceholderText(QString::fromUtf8("例如 123 456 789"));
    QLabel* idLabel = new QLabel(QString::fromUtf8("设备 ID"), body);
    idLabel->setProperty("role", "key");
    const QString relay = m_config && m_config->relayOn && !m_config->relayHost.isEmpty()
        ? QString::fromUtf8("中继 %1:%2").arg(m_config->relayHost).arg(m_config->relayPort)
        : QString::fromUtf8("请先在设置 → 网络里启用中继");
    QLabel* note = new QLabel(QString::fromUtf8("登记后打开会话时会按连接顺序尝试直连与中继 OPEN。") + QString::fromUtf8("\n") + relay, body);
    note->setProperty("role", "small");
    note->setWordWrap(true);
    grid->addWidget(idLabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(m_deviceId, 0, 1);
    grid->addWidget(note, 1, 1);
    grid->setColumnMinimumWidth(0, 84);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(2, 1);
    QObject::connect(m_deviceId, &QLineEdit::textChanged, [this](const QString&) { syncFoot(); });
    return body;
}

QString AddDeviceDialog::keyOf(const HelloMsg& hello)
{
    return DeviceStore::keyFor(QString::fromUtf8(hello.deviceId.c_str()), hello.share,
        QString::fromUtf8(hello.address.c_str()), QString::fromUtf8(hello.source.c_str()));
}

void AddDeviceDialog::rescan()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_hint->setText(QString::fromUtf8("正在通过 UDP %1 广播扫描…").arg(RemoteConst::kDiscoverPort));
    m_hint->repaint();
    m_found = m_scanner ? m_scanner() : std::vector<HelloMsg>();
    QApplication::restoreOverrideCursor();
    int added = 0;
    for (size_t i = 0; i < m_found.size(); ++i) {
        if (m_store->find(keyOf(m_found[i]))) {
            added += 1;
        }
    }
    m_hint->setText(m_found.empty()
        ? QString::fromUtf8("没有发现被控端。确认被控端已启动，且与本机在同一网段")
        : QString::fromUtf8("通过 UDP %1 广播发现 %2 台被控端，%3 台已在列表中").arg(RemoteConst::kDiscoverPort).arg(m_found.size()).arg(added));
    fill();
}

void AddDeviceDialog::fill()
{
    const QString query = m_filter->text().trimmed().toLower();
    m_table->blockSignals(true);
    m_table->setRowCount(0);
    for (size_t i = 0; i < m_found.size(); ++i) {
        const HelloMsg& hello = m_found[i];
        const QString name = QString::fromUtf8(hello.name.c_str());
        const QString address = QString::fromUtf8(hello.address.c_str());
        if (!query.isEmpty() && !name.toLower().contains(query) && !address.contains(query)) {
            continue;
        }
        const bool added = m_store->find(keyOf(hello)) != 0;
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        QTableWidgetItem* check = new QTableWidgetItem();
        check->setData(Qt::UserRole, static_cast<int>(i));
        check->setFlags(added ? Qt::ItemFlags() : (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable));
        check->setCheckState(added ? Qt::Unchecked : Qt::Checked);
        m_table->setItem(row, 0, check);
        const QString cells[] = { name, address, QString::fromUtf8(hello.deviceId.c_str()), QString::fromUtf8(hello.system.c_str()) };
        for (int c = 0; c < 4; ++c) {
            QTableWidgetItem* item = new QTableWidgetItem(cells[c].isEmpty() ? QString("-") : cells[c]);
            if (added) {
                item->setForeground(QColor("#BFBFBF"));
            }
            if (c == 1 || c == 2) {
                item->setFont(QFont("Consolas", 9));
            }
            m_table->setItem(row, c + 1, item);
        }
        QWidget* box = new QWidget(m_table);
        QHBoxLayout* boxRow = new QHBoxLayout(box);
        boxRow->setContentsMargins(10, 0, 0, 0);
        boxRow->addWidget(new TagLabel(added ? QString::fromUtf8("已添加") : QString::fromUtf8("可添加"), added ? "gray" : "green", box));
        boxRow->addStretch(1);
        m_table->setCellWidget(row, 5, box);
    }
    m_table->blockSignals(false);
    syncFoot();
}

std::vector<AddDeviceDialog::Pick> AddDeviceDialog::picks() const
{
    std::vector<Pick> out;
    if (m_tabs->currentIndex() == 2) {
        const QString id = m_deviceId ? m_deviceId->text().trimmed() : QString();
        if (id.isEmpty()) {
            return out;
        }
        Pick pick;
        pick.deviceId = id;
        pick.key = id;
        pick.address = m_config && m_config->relayOn ? m_config->relayHost : QString();
        if (!m_store->find(pick.key)) {
            out.push_back(pick);
        }
        return out;
    }
    if (m_tabs->currentIndex() == 1) {
        const QString ip = m_ip->text().trimmed();
        if (QRegExp("^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$").exactMatch(ip)) {
            Pick pick;
            pick.address = ip;
            pick.alias = m_ipName->text().trimmed();
            pick.key = DeviceStore::keyFor(QString(), 1, ip, QString());
            if (!m_store->find(pick.key)) {
                out.push_back(pick);
            }
        }
        return out;
    }
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem* check = m_table->item(row, 0);
        if (!check || check->checkState() != Qt::Checked) {
            continue;
        }
        const int index = check->data(Qt::UserRole).toInt();
        if (index < 0 || index >= static_cast<int>(m_found.size())) {
            continue;
        }
        const HelloMsg& hello = m_found[static_cast<size_t>(index)];
        Pick pick;
        pick.key = keyOf(hello);
        pick.name = QString::fromUtf8(hello.name.c_str());
        pick.address = QString::fromUtf8(hello.address.c_str());
        pick.system = QString::fromUtf8(hello.system.c_str());
        out.push_back(pick);
    }
    return out;
}

QString AddDeviceDialog::group() const
{
    return m_group->currentData().toString();
}

void AddDeviceDialog::syncFoot()
{
    const size_t n = picks().size();
    if (m_tabs->currentIndex() == 1) {
        m_count->setText(n == 0 ? QString::fromUtf8("填写有效且未添加过的 IPv4 地址") : QString());
        m_ok->setText(QString::fromUtf8("添加"));
    } else if (m_tabs->currentIndex() == 2) {
        m_count->setText(n == 0 ? QString::fromUtf8("填写未添加过的设备 ID") : QString());
        m_ok->setText(QString::fromUtf8("添加"));
    } else {
        m_count->setText(QString::fromUtf8("已选 %1 台").arg(n));
        m_ok->setText(n == 0 ? QString::fromUtf8("添加") : QString::fromUtf8("添加 %1 台").arg(n));
    }
    m_ok->setEnabled(n > 0);
}

}
