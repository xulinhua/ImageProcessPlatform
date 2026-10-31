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
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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
    QGridLayout* cfgFile = group(col, QString::fromUtf8("配置文件"));
    QPushButton* importCfg = RemoteTheme::button(QString::fromUtf8("导入…"), "upload", 0, body);
    QPushButton* exportCfg = RemoteTheme::button(QString::fromUtf8("导出…"), "download", 0, body);
    addRow(cfgFile, QString::fromUtf8("control.json"), line(importCfg, exportCfg));
    QObject::connect(importCfg, &QPushButton::clicked, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("导入配置"), QString(), "*.json");
        if (path.isEmpty()) {
            return;
        }
        const QString exeCfg = RemoteFiles::exeFile("control.json");
        const QString bak = exeCfg + ".bak";
        if (QFile::exists(exeCfg)) {
            QFile::remove(bak);
            QFile::copy(exeCfg, bak);
        }
        if (QFile::copy(path, exeCfg)) {
            m_config->load();
            load(*m_config);
        }
    });
    QObject::connect(exportCfg, &QPushButton::clicked, [this]() {
        store();
        const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出配置"),
            QDir(m_config->shotDir).filePath("control-export.json"), "*.json");
        if (path.isEmpty()) {
            return;
        }
        const QString src = RemoteFiles::exeFile("control.json");
        QFile::remove(path);
        QFile::copy(src, path);
    });
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

    QGridLayout* order = group(col, QString::fromUtf8("连接顺序"));
    m_orderList = new QListWidget(body);
    m_orderList->setFixedWidth(420);
    m_orderList->addItem(QString::fromUtf8("已有反连 (reverse)"));
    m_orderList->addItem(QString::fromUtf8("局域网直连 (direct)"));
    m_orderList->addItem(QString::fromUtf8("UDP 发现 (udp)"));
    m_orderList->addItem(QString::fromUtf8("中继 (relay)"));
    m_orderList->setToolTip(QString::fromUtf8("设备未反连时按此顺序尝试；已有反连则直接会话"));
    QPushButton* up = RemoteTheme::button(QString::fromUtf8("上移"), 0, 0, body);
    QPushButton* down = RemoteTheme::button(QString::fromUtf8("下移"), 0, 0, body);
    QObject::connect(up, &QPushButton::clicked, [this]() {
        const int row = m_orderList->currentRow();
        if (row <= 0) {
            return;
        }
        QListWidgetItem* item = m_orderList->takeItem(row);
        m_orderList->insertItem(row - 1, item);
        m_orderList->setCurrentRow(row - 1);
    });
    QObject::connect(down, &QPushButton::clicked, [this]() {
        const int row = m_orderList->currentRow();
        if (row < 0 || row >= m_orderList->count() - 1) {
            return;
        }
        QListWidgetItem* item = m_orderList->takeItem(row);
        m_orderList->insertItem(row + 1, item);
        m_orderList->setCurrentRow(row + 1);
    });
    order->addWidget(line(m_orderList, up, down), 0, 0, 1, 2);
    m_udpPunch = new SwitchButton(body);
    addRow(order, QString::fromUtf8("UDP 发现"), line(m_udpPunch,
        new QLabel(QString::fromUtf8("勾选后按设备 ID 广播发现再直连；也可在上方排序"), body)));

    QGridLayout* found = group(col, QString::fromUtf8("发现与带宽"));
    m_discovery = new SwitchButton(body);
    m_bandwidth = new QSpinBox(body);
    m_bandwidth->setRange(0, 100000);
    m_bandwidth->setSuffix(QString::fromUtf8(" kbps"));
    m_bandwidth->setSpecialValueText(QString::fromUtf8("不限"));
    m_bandwidth->setFixedWidth(120);
    QLabel* udp = new QLabel(QString::fromUtf8("UDP 端口"), body);
    QLineEdit* udpPort = new QLineEdit(QString::number(RemoteConst::kDiscoverPort), body);
    udpPort->setReadOnly(true);
    udpPort->setFixedWidth(90);
    udpPort->setToolTip(QString::fromUtf8("发现端口固定，与被控端一致"));
    addRow(found, QString::fromUtf8("局域网发现"), line(m_discovery, udp, udpPort));
    addRow(found, QString::fromUtf8("带宽上限"), line(m_bandwidth,
        new QLabel(QString::fromUtf8("0 表示不限；会话下发时会略降帧率/长边"), body)));
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
    QGridLayout* picture = group(col, QString::fromUtf8("默认画面"));
    m_defaultQuality = new QComboBox(body);
    m_defaultQuality->addItem(QString::fromUtf8("流畅"));
    m_defaultQuality->addItem(QString::fromUtf8("均衡"));
    m_defaultQuality->addItem(QString::fromUtf8("清晰"));
    m_defaultQuality->addItem(QString::fromUtf8("自定义"));
    m_defaultQuality->setFixedWidth(120);
    m_customEdge = new QSpinBox(body);
    m_customEdge->setRange(480, 3840);
    m_customEdge->setSuffix(QString::fromUtf8(" px"));
    m_customFps = new QSpinBox(body);
    m_customFps->setRange(RemoteConst::kMaxFpsMin, RemoteConst::kMaxFpsMax);
    m_customFps->setSuffix(QString::fromUtf8(" fps"));
    m_customBitrate = new QSpinBox(body);
    m_customBitrate->setRange(100, 50000);
    m_customBitrate->setSuffix(QString::fromUtf8(" kbps"));
    m_defaultFps = new QSpinBox(body);
    m_defaultFps->setRange(RemoteConst::kMaxFpsMin, RemoteConst::kMaxFpsMax);
    m_defaultFps->setSuffix(QString::fromUtf8(" fps"));
    m_defaultFps->setFixedWidth(100);
    m_adaptive = new SwitchButton(body);
    addRow(picture, QString::fromUtf8("默认画质"), line(m_defaultQuality));
    addRow(picture, QString::fromUtf8("默认帧率上限"), line(m_defaultFps,
        new QLabel(QString::fromUtf8("新打开的会话用这个值，会话里还能再改"), body)));
    addRow(picture, QString::fromUtf8("网络差时自动调整"), line(m_adaptive,
        new QLabel(QString::fromUtf8("只在延迟超过 800 ms 时略降分辨率，帧率不低于 10"), body)));
    addRow(picture, QString::fromUtf8("自定义长边"), line(m_customEdge));
    addRow(picture, QString::fromUtf8("自定义帧率"), line(m_customFps));
    addRow(picture, QString::fromUtf8("自定义码率"), line(m_customBitrate));
    QGridLayout* codec = group(col, QString::fromUtf8("编码"));
    m_streamCodec = new QComboBox(body);
    m_streamCodec->addItem(QString::fromUtf8("沿用被控端"), 0);
    m_streamCodec->addItem(QString::fromUtf8("JPEG"), 1);
    m_streamCodec->addItem(QString::fromUtf8("H.264"), 2);
    m_streamCodec->addItem(QString::fromUtf8("原图 BGRA"), 3);
    m_streamCodec->setFixedWidth(140);
    m_streamCodec->setToolTip(QString::fromUtf8("新会话默认发送方式。会话工具条可随时改，方便对比带宽与画质"));
    addRow(codec, QString::fromUtf8("默认发送"), line(m_streamCodec,
        new QLabel(QString::fromUtf8("原图带宽很大；H.264 需 openh264.dll 或系统编码器"), body)));
    col->addStretch(1);
    return body;
}

QWidget* SettingsDialog::buildSecurity()
{
    QVBoxLayout* col = 0;
    QWidget* body = page(&col);
    QGridLayout* link = group(col, QString::fromUtf8("传输"));
    addRow(link, QString::fromUtf8("加密"), line(new TagLabel(QString::fromUtf8("TLS（Schannel）"), "green", body)));
    QGridLayout* idle = group(col, QString::fromUtf8("空闲断开"));
    m_idle = new QSpinBox(body);
    m_idle->setRange(0, 1440);
    m_idle->setSuffix(QString::fromUtf8(" 分钟"));
    m_idle->setSpecialValueText(QString::fromUtf8("关闭"));
    addRow(idle, QString::fromUtf8("无操作断开"), line(m_idle,
        new QLabel(QString::fromUtf8("中控台无键鼠/会话活动超过此时间则断开所有在线会话"), body)));
    QGridLayout* mark = group(col, QString::fromUtf8("水印"));
    m_watermarkOn = new SwitchButton(body);
    m_watermarkText = new QLineEdit(body);
    m_watermarkText->setPlaceholderText(QString::fromUtf8("例如 内部使用"));
    addRow(mark, QString::fromUtf8("叠加水印"), line(m_watermarkOn));
    addRow(mark, QString::fromUtf8("水印文字"), line(m_watermarkText));
    QGridLayout* auth = group(col, QString::fromUtf8("认证"));
    m_highestPrivilege = new SwitchButton(body);
    m_highestPrivilege->setToolTip(QString::fromUtf8(
        "开：连上即有满权限操作权，跳过验证码/密码/现场确认；会话里可手动释放。关后按单台设备，再按登录账户。"));
    addRow(auth, QString::fromUtf8("最高权限"), line(m_highestPrivilege,
        new QLabel(QString::fromUtf8("全局优先；关后看设备属性，再看账户"), body)));
    QLabel* pinNote = new QLabel(QString::fromUtf8("会话信息页展示对端证书指纹；被控端可按指纹信任中控台"), body);
    pinNote->setProperty("role", "small");
    pinNote->setWordWrap(true);
    addRow(auth, QString::fromUtf8("证书指纹"), pinNote);
    QLabel* lockNote = new QLabel(QString::fromUtf8("连续 5 次认证失败锁定 5 分钟（被控端 RemoteAuth）"), body);
    lockNote->setProperty("role", "small");
    lockNote->setWordWrap(true);
    addRow(auth, QString::fromUtf8("失败锁定"), lockNote);
    QLabel* note = new QLabel(QString::fromUtf8(
        "最高权限关闭时，操作申请是否现场确认由被控端“安全”设置决定。"), body);
    note->setProperty("role", "small");
    note->setWordWrap(true);
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
    QPushButton* browseRecord = RemoteTheme::button(QString::fromUtf8("浏览…"), "folder", 0, body);
    addRow(rec, QString::fromUtf8("保存目录"), line(m_recordDir, browseRecord));
    QObject::connect(browseRecord, &QPushButton::clicked, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, QString::fromUtf8("录屏目录"), m_recordDir->text());
        if (!dir.isEmpty()) {
            m_recordDir->setText(QDir::toNativeSeparators(dir));
        }
    });
    addRow(rec, QString::fromUtf8("保留"), line(m_recordDays));
    col->addStretch(1);
    return body;
}

QWidget* SettingsDialog::buildLog(const QString& auditDir)
{
    QVBoxLayout* col = 0;
    QWidget* body = page(&col);
    QGridLayout* run = group(col, QString::fromUtf8("运行日志"));
    m_logDisk = new SwitchButton(body);
    m_logDisk->setToolTip(QString::fromUtf8("默认关闭。打开后写入 exe名\\log，按间隔滚动；关时仍可在窗口看内存日志"));
    m_logPictureDisk = new SwitchButton(body);
    m_logPictureDisk->setToolTip(QString::fromUtf8("画面/发送/接收/解码 的 Info 是否落盘。量大，排查时再开"));
    m_logRotate = new QComboBox(body);
    m_logRotate->addItem(QString::fromUtf8("15 分钟"), 15);
    m_logRotate->addItem(QString::fromUtf8("30 分钟"), 30);
    m_logRotate->addItem(QString::fromUtf8("1 小时"), 60);
    m_logRotate->addItem(QString::fromUtf8("2 小时"), 120);
    m_logRotate->setFixedWidth(120);
    m_logKeepDays = new QSpinBox(body);
    m_logKeepDays->setRange(1, 365);
    m_logKeepDays->setSuffix(QString::fromUtf8(" 天"));
    m_logKeepDays->setFixedWidth(100);
    addRow(run, QString::fromUtf8("写盘"), line(m_logDisk));
    addRow(run, QString::fromUtf8("画面链路写盘"), line(m_logPictureDisk));
    addRow(run, QString::fromUtf8("滚动间隔"), line(m_logRotate));
    addRow(run, QString::fromUtf8("保留"), line(m_logKeepDays));
    const QString logDir = RemoteFiles::logDir();
    QLabel* runDir = new QLabel(QDir::toNativeSeparators(logDir), body);
    runDir->setProperty("role", "mono");
    runDir->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* openRun = RemoteTheme::button(QString::fromUtf8("打开目录"), "folder", 0, body);
    openRun->setProperty("size", "sm");
    addRow(run, QString::fromUtf8("目录"), line(runDir, openRun));
    QObject::connect(openRun, &QPushButton::clicked, [logDir]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
    });
    auto syncPicture = [this]() {
        if (m_logPictureDisk) {
            m_logPictureDisk->setEnabled(m_logDisk && m_logDisk->isChecked());
        }
    };
    QObject::connect(m_logDisk, &SwitchButton::toggled, syncPicture);
    syncPicture();

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
    m_defaultQuality->setCurrentIndex(config.defaultQuality);
    m_defaultFps->setValue(config.defaultFps);
    {
        const int idx = m_streamCodec->findData(config.streamCodec);
        m_streamCodec->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_adaptive->setChecked(config.adaptive);
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
    if (m_idle) {
        m_idle->setValue(config.idleMinutes);
    }
    if (m_bandwidth) {
        m_bandwidth->setValue(config.bandwidthKbps);
    }
    if (m_watermarkOn) {
        m_watermarkOn->setChecked(config.watermarkOn);
    }
    if (m_watermarkText) {
        m_watermarkText->setText(config.watermarkText);
    }
    if (m_customEdge) {
        m_customEdge->setValue(config.customLongEdge);
    }
    if (m_customFps) {
        m_customFps->setValue(config.customFps);
    }
    if (m_customBitrate) {
        m_customBitrate->setValue(config.customBitrateKbps);
    }
    if (m_udpPunch) {
        m_udpPunch->setChecked(config.udpPunch);
    }
    if (m_highestPrivilege) {
        m_highestPrivilege->setChecked(config.highestPrivilege);
    }
    if (m_logDisk) {
        m_logDisk->setChecked(config.logDisk);
    }
    if (m_logPictureDisk) {
        m_logPictureDisk->setChecked(config.logPictureDisk);
        m_logPictureDisk->setEnabled(config.logDisk);
    }
    if (m_logRotate) {
        const int at = m_logRotate->findData(config.logRotateMinutes);
        m_logRotate->setCurrentIndex(at >= 0 ? at : m_logRotate->findData(60));
    }
    if (m_logKeepDays) {
        m_logKeepDays->setValue(config.logKeepDays > 0 ? config.logKeepDays : 7);
    }
    if (m_orderList) {
        m_orderList->clear();
        for (int i = 0; i < config.connectOrder.size(); ++i) {
            const QString step = config.connectOrder.at(i);
            if (step == "reverse") {
                m_orderList->addItem(QString::fromUtf8("已有反连 (reverse)"));
            } else if (step == "direct") {
                m_orderList->addItem(QString::fromUtf8("局域网直连 (direct)"));
            } else if (step == "udp") {
                m_orderList->addItem(QString::fromUtf8("UDP 发现 (udp)"));
            } else if (step == "relay") {
                m_orderList->addItem(QString::fromUtf8("中继 (relay)"));
            }
        }
        if (config.udpPunch) {
            bool hasUdp = false;
            for (int i = 0; i < m_orderList->count(); ++i) {
                if (m_orderList->item(i)->text().contains("udp")) {
                    hasUdp = true;
                    break;
                }
            }
            if (!hasUdp) {
                const int at = m_orderList->count() < 2 ? m_orderList->count() : 2;
                m_orderList->insertItem(at, QString::fromUtf8("UDP 发现 (udp)"));
            }
        }
        if (m_orderList->count() == 0) {
            m_orderList->addItem(QString::fromUtf8("已有反连 (reverse)"));
            m_orderList->addItem(QString::fromUtf8("局域网直连 (direct)"));
            m_orderList->addItem(QString::fromUtf8("中继 (relay)"));
        }
    }
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
    m_config->defaultQuality = m_defaultQuality->currentIndex();
    m_config->defaultFps = m_defaultFps->value();
    m_config->streamCodec = m_streamCodec->currentData().toInt();
    m_config->adaptive = m_adaptive->isChecked();
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
    if (m_idle) {
        m_config->idleMinutes = m_idle->value();
    }
    if (m_bandwidth) {
        m_config->bandwidthKbps = m_bandwidth->value();
    }
    if (m_watermarkOn) {
        m_config->watermarkOn = m_watermarkOn->isChecked();
    }
    if (m_watermarkText) {
        m_config->watermarkText = m_watermarkText->text().trimmed();
    }
    if (m_customEdge) {
        m_config->customLongEdge = m_customEdge->value();
    }
    if (m_customFps) {
        m_config->customFps = m_customFps->value();
    }
    if (m_customBitrate) {
        m_config->customBitrateKbps = m_customBitrate->value();
    }
    if (m_orderList) {
        m_config->connectOrder.clear();
        for (int i = 0; i < m_orderList->count(); ++i) {
            const QString text = m_orderList->item(i)->text();
            if (text.contains("reverse")) {
                m_config->connectOrder.append("reverse");
            } else if (text.contains("direct")) {
                m_config->connectOrder.append("direct");
            } else if (text.contains("udp")) {
                m_config->connectOrder.append("udp");
            } else if (text.contains("relay")) {
                m_config->connectOrder.append("relay");
            }
        }
    }
    if (m_udpPunch) {
        m_config->udpPunch = m_udpPunch->isChecked();
        if (m_config->udpPunch && !m_config->connectOrder.contains("udp")) {
            const int at = m_config->connectOrder.indexOf("direct");
            m_config->connectOrder.insert(at >= 0 ? at : m_config->connectOrder.size(), "udp");
        }
        if (!m_config->udpPunch) {
            m_config->connectOrder.removeAll("udp");
        }
    }
    if (m_highestPrivilege) {
        m_config->highestPrivilege = m_highestPrivilege->isChecked();
    }
    if (m_logDisk) {
        m_config->logDisk = m_logDisk->isChecked();
    }
    if (m_logPictureDisk) {
        m_config->logPictureDisk = m_logPictureDisk->isChecked();
    }
    if (m_logRotate) {
        m_config->logRotateMinutes = m_logRotate->currentData().toInt();
    }
    if (m_logKeepDays) {
        m_config->logKeepDays = m_logKeepDays->value();
    }
    m_config->save();
    m_config->purgeRecords();
    m_config->applyAutoStart();
    m_config->applyRunLog();
    if (m_applied) {
        m_applied(moved);
    }
}

}
