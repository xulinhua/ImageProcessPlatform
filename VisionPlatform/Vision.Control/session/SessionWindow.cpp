/**
 * @file SessionWindow.cpp
 * @brief 远程会话窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SessionWindow.h"
#include "AuditLog.h"
#include "ControlConfig.h"
#include "FrameView.h"
#include "Messages.h"
#include "RemoteFiles.h"
#include "RemoteTheme.h"
#include "RemoteWidgets.h"

#include <QApplication>
#include <QClipboard>
#include <QAction>
#include <QCloseEvent>
#include <QMenu>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QImage>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegExp>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

#include <windows.h>

namespace Vision {

SessionWindow::SessionWindow(ControlSession* session, const ControlConfig* config, AuditLog* audit, const QString& key,
    QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_config(config)
    , m_audit(audit)
    , m_key(key)
    , m_online(false)
    , m_wasOnline(false)
    , m_wasOperating(false)
    , m_seen(0)
    , m_thumbSeen(0)
    , m_monitorCount(-1)
    , m_monitorShare(-1)
    , m_full(false)
    , m_flashUntil(0)
    , m_fps(0)
    , m_kbps(0)
    , m_floatBar(0)
    , m_quality(0)
    , m_codec(0)
    , m_maxFps(0)
    , m_infoCrypto(0)
    , m_infoFingerprint(0)
    , m_view(new FrameView(this))
    , m_record(0)
    , m_privacy(0)
    , m_chatEdit(0)
    , m_chatKind(0)
    , m_extraViewers(0)
    , m_viewerSeen(0)
    , m_clipSeen(0)
    , m_chatSeen(0)
    , m_recording(false)
    , m_recordArmed(false)
    , m_cursorSeen(0)
    , m_clipPullId(0)
    , m_clipPulling(false)
    , m_clipSawBusy(false)
    , m_avi(0)
    , m_aviFrames(0)
    , m_aviW(0)
    , m_aviH(0)
    , m_aviLast(0)
{
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1440, 860);
    m_toolbar = buildToolbar();
    m_panel = buildPanel();
    m_status = buildStatus();
    m_floatBar = buildFloatBar();

    QHBoxLayout* middle = new QHBoxLayout();
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(0);
    middle->addWidget(m_view, 1);
    middle->addWidget(m_panel);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_toolbar);
    root->addLayout(middle, 1);
    root->addWidget(m_status);

    m_view->setFit(m_config->fitDefault);
    m_fitSeg->setCurrent(m_config->fitDefault ? 0 : 1);
    m_view->setSender([this](const InputMsg& msg) {
        if (!m_tile.peerId.empty()) {
            m_session->sendInput(m_tile.peerId, msg);
        }
    });
    m_view->setToggle([this]() { setFullScreenMode(!m_full); });
    syncControls();
}

QString SessionWindow::key() const
{
    return m_key;
}

void SessionWindow::setOnBack(const std::function<void()>& back)
{
    m_back = back;
}

void SessionWindow::setOnClosed(const std::function<void(const QString&)>& closed)
{
    m_closed = closed;
}

QString SessionWindow::rateText(double kbps)
{
    if (kbps >= 1000.0) {
        return QString("%1 Mbps").arg(kbps / 1000.0, 0, 'f', 1);
    }
    return QString("%1 kbps").arg(static_cast<int>(kbps + 0.5));
}

QString SessionWindow::shareText(const ControlSession::Tile& tile)
{
    if (tile.share == 1) {
        return tile.monitors > 1 ? QString::fromUtf8("桌面 · 显示器 %1").arg(tile.monitor + 1) : QString::fromUtf8("桌面");
    }
    const QString source = QString::fromUtf8(tile.source.c_str());
    return source.isEmpty() ? QString::fromUtf8("窗口") : QString::fromUtf8("窗口 · %1").arg(source);
}

QWidget* SessionWindow::buildToolbar()
{
    QWidget* bar = new QWidget(this);
    bar->setObjectName("toolbar");
    bar->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* row = new QHBoxLayout(bar);
    row->setContentsMargins(10, 8, 10, 8);
    row->setSpacing(5);

    // 建按钮；markLater 的是后续期次才实现的功能，只占位
    QPushButton* back = RemoteTheme::button(QString::fromUtf8("监控墙"), "left", 0, bar);
    m_operate = RemoteTheme::button(QString::fromUtf8("申请操作"), "mouse", "primary", bar);
    m_monitors = new SegmentBar(bar);
    m_quality = new QComboBox(bar);
    m_quality->addItem(QString::fromUtf8("流畅"));
    m_quality->addItem(QString::fromUtf8("均衡"));
    m_quality->addItem(QString::fromUtf8("清晰"));
    m_quality->setCurrentIndex(m_config && m_config->defaultQuality >= 0 && m_config->defaultQuality <= 2
        ? m_config->defaultQuality : 1);
    m_quality->setFixedWidth(96);
    m_quality->setToolTip(QString::fromUtf8("流畅：低画质高帧率；清晰：高画质低带宽压力"));
    m_codec = new QComboBox(bar);
    m_codec->addItem(QString::fromUtf8("沿用"), 0);
    m_codec->addItem(QString::fromUtf8("JPEG"), 1);
    m_codec->addItem(QString::fromUtf8("H.264"), 2);
    m_codec->addItem(QString::fromUtf8("原图"), 3);
    {
        const int want = m_config ? m_config->streamCodec : 0;
        const int idx = m_codec->findData(want);
        m_codec->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_codec->setFixedWidth(88);
    m_codec->setToolTip(QString::fromUtf8("发送方式：JPEG / H.264 / 原图 BGRA，方便对比带宽与画质"));
    m_maxFps = new QSpinBox(bar);
    m_maxFps->setRange(RemoteConst::kMaxFpsMin, RemoteConst::kMaxFpsMax);
    m_maxFps->setValue(m_config && m_config->defaultFps >= RemoteConst::kMaxFpsMin
        ? m_config->defaultFps : RemoteConst::kMaxFpsDefault);
    m_maxFps->setSuffix(QString::fromUtf8(" fps"));
    m_maxFps->setFixedWidth(100);
    m_maxFps->setToolTip(QString::fromUtf8("帧率上限；实际帧率受编码耗时与带宽限制"));
    m_fitSeg = new SegmentBar(bar);
    m_fitSeg->addButton(QString::fromUtf8("适应"), "fit");
    m_fitSeg->addButton("1:1");
    QPushButton* full = RemoteTheme::button(QString(), "full", "icon", bar);
    full->setToolTip(QString::fromUtf8("全屏"));
    QPushButton* cad = RemoteTheme::button("Ctrl+Alt+Del", "keyboard", 0, bar);
    QObject::connect(cad, &QPushButton::clicked, [this]() {
        if (m_online && m_tile.operating) {
            m_session->sendSpecial(m_tile.peerId, SpecialMsg::KindCad);
        }
    });
    m_win = RemoteTheme::button(QString(), "win", "icon", bar);
    m_win->setToolTip(QString::fromUtf8("发送 Win 键"));
    m_altTab = RemoteTheme::button(QString(), "tab", "icon", bar);
    m_altTab->setToolTip(QString::fromUtf8("发送 Alt+Tab"));
    QPushButton* clip = RemoteTheme::button(QString(), "clipboard", "icon", bar);
    clip->setToolTip(QString::fromUtf8("把本机剪贴板文本发到被控端。对方复制后会自动回到这里"));
    QPushButton* file = RemoteTheme::button(QString::fromUtf8("文件"), "folder", 0, bar);
    file->setToolTip(QString::fromUtf8("在本机和被控端之间选择路径并传文件"));
    m_chat = RemoteTheme::button(QString::fromUtf8("聊天"), "chat", 0, bar);
    m_chat->setCheckable(true);
    m_chat->setChecked(true);
    m_chat->setToolTip(QString::fromUtf8("显示或隐藏右侧面板"));
    m_shot = RemoteTheme::button(QString(), "camera", "icon", bar);
    m_shot->setToolTip(QString::fromUtf8("截图"));
    m_record = RemoteTheme::button(QString::fromUtf8("录屏"), "record", 0, bar);
    m_record->setToolTip(QString::fromUtf8("把主码流录成 MJPEG AVI，保存在截图目录"));
    m_privacy = RemoteTheme::button(QString(), "shield", "icon", bar);
    m_privacy->setCheckable(true);
    m_privacy->setToolTip(QString::fromUtf8("隐私屏：被控端全屏显示“维护中”"));
    QPushButton* power = RemoteTheme::button(QString(), "power", 0, bar);
    power->setIcon(RemoteTheme::icon("power"));
    power->setToolTip(QString::fromUtf8("锁屏、注销、重启、关机"));
    QPushButton* close = RemoteTheme::button(QString::fromUtf8("断开"), "x", "dangerFill", bar);
    close->setToolTip(QString::fromUtf8("结束会话并释放操作"));

    QLabel* monitorLabel = new QLabel(QString::fromUtf8("显示器"), bar);
    monitorLabel->setProperty("role", "key");
    QLabel* qualityLabel = new QLabel(QString::fromUtf8("画质"), bar);
    qualityLabel->setProperty("role", "key");
    QLabel* codecLabel = new QLabel(QString::fromUtf8("编码"), bar);
    codecLabel->setProperty("role", "key");
    QLabel* fpsLabel = new QLabel(QString::fromUtf8("上限"), bar);
    fpsLabel->setProperty("role", "key");

    // 排布：返回 | 操作 | 显示器、画质、编码、帧率上限 | 缩放、全屏 | 组合键 | 工具 …… 电源、断开
    row->addWidget(back);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(m_operate);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(monitorLabel);
    row->addWidget(m_monitors);
    row->addSpacing(6);
    row->addWidget(qualityLabel);
    row->addWidget(m_quality);
    row->addSpacing(6);
    row->addWidget(codecLabel);
    row->addWidget(m_codec);
    row->addSpacing(6);
    row->addWidget(fpsLabel);
    row->addWidget(m_maxFps);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(m_fitSeg);
    row->addWidget(full);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(cad);
    row->addWidget(m_win);
    row->addWidget(m_altTab);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(clip);
    row->addWidget(file);
    row->addWidget(m_chat);
    row->addWidget(m_shot);
    row->addWidget(m_record);
    row->addWidget(m_privacy);
    row->addStretch(1);
    row->addWidget(power);
    row->addWidget(close);

    // 组合键只在持有操作权时发出，见 sendCombo
    QObject::connect(back, &QPushButton::clicked, [this]() {
        if (m_back) {
            m_back();
        }
        this->close();
    });
    QObject::connect(m_operate, &QPushButton::clicked, [this]() { toggleOperate(); });
    m_monitors->setOnChanged([this](int index) { pickMonitor(index); });
    QObject::connect(m_quality, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        // 切档时顺带把帧率上限调到该档推荐值，用户仍可再改
        const int presets[] = { 60, 30, 20 };
        if (index >= 0 && index < 3) {
            m_maxFps->blockSignals(true);
            m_maxFps->setValue(presets[index]);
            m_maxFps->blockSignals(false);
        }
        pushPictureConfig(QString::fromUtf8("画质：%1").arg(m_quality->currentText()));
    });
    QObject::connect(m_maxFps, QOverload<int>::of(&QSpinBox::valueChanged), [this](int) {
        pushPictureConfig(QString::fromUtf8("帧率上限 %1 fps").arg(m_maxFps->value()));
    });
    QObject::connect(m_codec, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        pushPictureConfig(QString::fromUtf8("编码：%1").arg(m_codec->currentText()));
    });
    m_fitSeg->setOnChanged([this](int index) { m_view->setFit(index == 0); });
    QObject::connect(full, &QPushButton::clicked, [this]() { setFullScreenMode(true); });
    QObject::connect(m_win, &QPushButton::clicked, [this]() {
        const uint16_t keys[] = { VK_LWIN };
        sendCombo(keys, 1);
    });
    QObject::connect(m_altTab, &QPushButton::clicked, [this]() {
        const uint16_t keys[] = { VK_MENU, VK_TAB };
        sendCombo(keys, 2);
    });
    QObject::connect(m_chat, &QPushButton::toggled, [this](bool on) { m_panel->setVisible(on && !m_full); });
    QObject::connect(clip, &QPushButton::clicked, [this]() { pushClipboard(); });
    QObject::connect(file, &QPushButton::clicked, [this]() { openFiles(); });
    QObject::connect(m_shot, &QPushButton::clicked, [this]() { takeShot(); });
    QObject::connect(m_record, &QPushButton::clicked, [this]() { toggleRecord(); });
    QObject::connect(m_privacy, &QPushButton::clicked, [this]() { togglePrivacy(); });
    QObject::connect(power, &QPushButton::clicked, [this, power]() {
        QMenu menu(power);
        QObject::connect(menu.addAction(QString::fromUtf8("锁屏")), &QAction::triggered, [this]() { sendPower(1); });
        QObject::connect(menu.addAction(QString::fromUtf8("注销")), &QAction::triggered, [this]() { sendPower(2); });
        QObject::connect(menu.addAction(QString::fromUtf8("重启")), &QAction::triggered, [this]() { sendPower(3); });
        QObject::connect(menu.addAction(QString::fromUtf8("关机")), &QAction::triggered, [this]() { sendPower(4); });
        menu.exec(power->mapToGlobal(QPoint(0, power->height())));
    });
    QObject::connect(close, &QPushButton::clicked, [this]() { this->close(); });
    return bar;
}

QWidget* SessionWindow::buildPanel()
{
    QWidget* panel = new QWidget(this);
    panel->setObjectName("panel");
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setFixedWidth(300);

    // 页签行
    m_tabs = new QTabBar(panel);
    m_tabs->addTab(QString::fromUtf8("聊天"));
    m_tabs->addTab(QString::fromUtf8("会话信息"));
    m_tabs->addTab(QString::fromUtf8("快捷键"));
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    QWidget* tabLine = new QWidget(panel);
    tabLine->setStyleSheet("border-bottom:1px solid #F0F0F0;");
    QHBoxLayout* tabRow = new QHBoxLayout(tabLine);
    tabRow->setContentsMargins(16, 0, 0, 0);
    tabRow->addWidget(m_tabs);
    tabRow->addStretch(1);

    m_pages = new QStackedWidget(panel);

    // 聊天页：上面是会话事件列表，文字聊天未实现
    QWidget* chat = new QWidget(m_pages);
    QVBoxLayout* chatRoot = new QVBoxLayout(chat);
    chatRoot->setContentsMargins(0, 0, 0, 0);
    chatRoot->setSpacing(0);
    QScrollArea* scroll = new QScrollArea(chat);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget* list = new QWidget(scroll);
    list->setStyleSheet("background:#FFFFFF;");
    m_events = new QVBoxLayout(list);
    m_events->setContentsMargins(14, 14, 14, 14);
    m_events->setSpacing(12);
    QLabel* later = new QLabel(QString::fromUtf8("会话事件和聊天"), list);
    later->setProperty("role", "small");
    later->setAlignment(Qt::AlignCenter);
    later->setWordWrap(true);
    m_events->addWidget(later);
    m_events->addStretch(1);
    scroll->setWidget(list);
    chatRoot->addWidget(scroll, 1);

    // 在场人员：本人（观看/操作中）和现场
    QWidget* viewers = new QWidget(chat);
    viewers->setStyleSheet("QWidget#viewers{border-top:1px solid #F0F0F0;}");
    viewers->setObjectName("viewers");
    viewers->setAttribute(Qt::WA_StyledBackground);
    QVBoxLayout* viewersCol = new QVBoxLayout(viewers);
    viewersCol->setContentsMargins(14, 12, 14, 12);
    viewersCol->setSpacing(10);
    QHBoxLayout* head = new QHBoxLayout();
    m_viewerCount = new QLabel(QString::fromUtf8("在线 2 人"), viewers);
    m_viewerCount->setProperty("role", "small");
    QLabel* right = new QLabel(QString::fromUtf8("操作权"), viewers);
    right->setProperty("role", "small");
    head->addWidget(m_viewerCount);
    head->addStretch(1);
    head->addWidget(right);
    viewersCol->addLayout(head);
    QHBoxLayout* me = new QHBoxLayout();
    me->setSpacing(8);
    me->addWidget(new AvatarLabel(RemoteFiles::userName(), 0, 26, viewers));
    me->addWidget(new QLabel(QString::fromUtf8("%1（我）").arg(RemoteFiles::userName()), viewers), 1);
    m_meTag = new TagLabel(QString::fromUtf8("观看"), "gray", viewers);
    me->addWidget(m_meTag);
    viewersCol->addLayout(me);
    QHBoxLayout* site = new QHBoxLayout();
    site->setSpacing(8);
    site->addWidget(new AvatarLabel(QString::fromUtf8("现"), 2, 26, viewers));
    m_siteName = new QLabel(QString::fromUtf8("现场"), viewers);
    site->addWidget(m_siteName, 1);
    site->addWidget(new TagLabel(QString::fromUtf8("本机"), "gray", viewers));
    viewersCol->addLayout(site);
    m_extraViewers = new QVBoxLayout();
    viewersCol->addLayout(m_extraViewers);
    QPushButton* invite = RemoteTheme::button(QString::fromUtf8("邀请观看"), 0, 0, viewers);
    QObject::connect(invite, &QPushButton::clicked, [this]() { inviteViewer(); });
    viewersCol->addWidget(invite);
    chatRoot->addWidget(viewers);

    // 输入框与发送按钮，占位置灰
    QWidget* compose = new QWidget(chat);
    compose->setObjectName("compose");
    compose->setStyleSheet("QWidget#compose{border-top:1px solid #F0F0F0;}");
    compose->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* composeRow = new QHBoxLayout(compose);
    composeRow->setContentsMargins(10, 10, 10, 10);
    composeRow->setSpacing(8);
    m_chatKind = new QComboBox(compose);
    m_chatKind->addItem(QString::fromUtf8("聊天"), 0);
    m_chatKind->addItem(QString::fromUtf8("弹窗"), 1);
    m_chatKind->setToolTip(QString::fromUtf8("聊天记在被控端记录里；弹窗会在被控端桌面上弹出"));
    m_chatEdit = new QLineEdit(compose);
    m_chatEdit->setPlaceholderText(QString::fromUtf8("输入消息，Enter 发送"));
    QPushButton* send = RemoteTheme::button(QString(), "send", "primary", compose);
    send->setProperty("kind", "icon");
    send->setStyleSheet("QPushButton{background:#1890FF;border-color:#1890FF;} QPushButton:disabled{background:#F5F5F5;border-color:#D9D9D9;}");
    QObject::connect(send, &QPushButton::clicked, [this]() { sendChat(); });
    QObject::connect(m_chatEdit, &QLineEdit::returnPressed, [this]() { sendChat(); });
    composeRow->addWidget(m_chatKind);
    composeRow->addWidget(m_chatEdit, 1);
    composeRow->addWidget(send);
    chatRoot->addWidget(compose);
    m_pages->addWidget(chat);

    // 会话信息页：值标签由 syncControls 刷新
    QWidget* info = new QWidget(m_pages);
    info->setStyleSheet("background:#FFFFFF;");
    QGridLayout* grid = new QGridLayout(info);
    grid->setContentsMargins(16, 14, 16, 14);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    m_infoName = addInfo(grid, 0, QString::fromUtf8("设备"));
    m_infoId = addInfo(grid, 1, QString::fromUtf8("设备 ID"));
    m_infoAddress = addInfo(grid, 2, QString::fromUtf8("地址"));
    m_infoSystem = addInfo(grid, 3, QString::fromUtf8("系统"));
    m_infoShare = addInfo(grid, 4, QString::fromUtf8("共享"));
    m_infoImage = addInfo(grid, 5, QString::fromUtf8("画面"));
    m_infoFps = addInfo(grid, 6, QString::fromUtf8("帧率"));
    m_infoRate = addInfo(grid, 7, QString::fromUtf8("下行"));
    m_infoRtt = addInfo(grid, 8, QString::fromUtf8("延迟"));
    m_infoBytes = addInfo(grid, 9, QString::fromUtf8("已接收"));
    m_infoUptime = addInfo(grid, 10, QString::fromUtf8("在线时长"));
    QLabel* link = addInfo(grid, 11, QString::fromUtf8("连接"));
    link->setText(QString::fromUtf8("反连 · TCP %1").arg(m_config->port));
    QLabel* cryptoKey = new QLabel(QString::fromUtf8("加密"), info);
    cryptoKey->setProperty("role", "key");
    grid->addWidget(cryptoKey, 12, 0);
    QHBoxLayout* cryptoRow = new QHBoxLayout();
    m_infoCrypto = new TagLabel(QString::fromUtf8("TLS"), "green", info);
    cryptoRow->addWidget(m_infoCrypto);
    m_infoFingerprint = new QLabel(QString::fromUtf8("-"), info);
    m_infoFingerprint->setProperty("role", "mono");
    m_infoFingerprint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cryptoRow->addWidget(m_infoFingerprint, 1);
    grid->addLayout(cryptoRow, 12, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(13, 1);
    m_pages->addWidget(info);

    // 快捷键页：静态说明
    QWidget* keys = new QWidget(m_pages);
    keys->setStyleSheet("background:#FFFFFF;");
    QGridLayout* keyGrid = new QGridLayout(keys);
    keyGrid->setContentsMargins(16, 14, 16, 14);
    keyGrid->setHorizontalSpacing(12);
    keyGrid->setVerticalSpacing(10);
    const char* const shortcuts[][2] = {
        { "双击画面", "只看时进入或退出全屏" },
        { "键盘", "操作时全部转发到现场，含 Tab" },
        { "鼠标 / 滚轮", "操作时转发到现场" },
        { "Win 按钮", "向现场发送 Win 键" },
        { "Alt+Tab 按钮", "在现场切换窗口" },
        { "截图按钮", "保存当前画面到截图目录" },
        { "现场动鼠标", "操作自动暂停，需重新申请" }
    };
    for (int i = 0; i < 7; ++i) {
        QLabel* k = new QLabel(QString::fromUtf8(shortcuts[i][0]), keys);
        k->setProperty("role", "key");
        QLabel* v = new QLabel(QString::fromUtf8(shortcuts[i][1]), keys);
        v->setWordWrap(true);
        keyGrid->addWidget(k, i, 0, Qt::AlignTop);
        keyGrid->addWidget(v, i, 1);
    }
    keyGrid->setColumnStretch(1, 1);
    keyGrid->setRowStretch(7, 1);
    m_pages->addWidget(keys);

    QVBoxLayout* col = new QVBoxLayout(panel);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    col->addWidget(tabLine);
    col->addWidget(m_pages, 1);
    QObject::connect(m_tabs, &QTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
    return panel;
}

QLabel* SessionWindow::addInfo(QGridLayout* grid, int row, const QString& key)
{
    QLabel* k = new QLabel(key, grid->parentWidget());
    k->setProperty("role", "key");
    QLabel* v = new QLabel("-", grid->parentWidget());
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->setWordWrap(true);
    grid->addWidget(k, row, 0, Qt::AlignTop);
    grid->addWidget(v, row, 1);
    return v;
}

QLabel* SessionWindow::addStatus(QHBoxLayout* row, const char* iconName)
{
    QWidget* parent = row->parentWidget();
    if (iconName) {
        QLabel* icon = new QLabel(parent);
        icon->setPixmap(RemoteTheme::pixmap(iconName, QColor(RemoteTheme::kMuted), 13, devicePixelRatioF()));
        row->addWidget(icon);
    }
    QLabel* text = new QLabel(parent);
    text->setTextFormat(Qt::RichText);
    row->addWidget(text);
    row->addSpacing(14);
    return text;
}

QWidget* SessionWindow::buildStatus()
{
    QWidget* bar = new QWidget(this);
    bar->setObjectName("statusbar");
    bar->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* row = new QHBoxLayout(bar);
    row->setContentsMargins(12, 5, 12, 5);
    row->setSpacing(5);
    m_stDot = new StatusDot("gray", bar);
    row->addWidget(m_stDot);
    m_stState = addStatus(row, 0);
    QLabel* link = addStatus(row, "link");
    link->setText(QString::fromUtf8("反连 · 明文 TCP"));
    m_stImage = addStatus(row, "monitor");
    m_stRtt = addStatus(row, 0);
    m_stRate = addStatus(row, "download");
    row->addStretch(1);
    m_stPerm = addStatus(row, "shieldOk");
    return bar;
}

QWidget* SessionWindow::buildFloatBar()
{
    QWidget* bar = new QWidget(this);
    bar->setObjectName("floatbar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setStyleSheet("QWidget#floatbar{background:#FFFFFF;border:1px solid #E8E8E8;border-radius:6px;}");
    QHBoxLayout* row = new QHBoxLayout(bar);
    row->setContentsMargins(8, 6, 8, 6);
    row->setSpacing(6);
    QLabel* name = new QLabel(bar);
    name->setObjectName("floatName");
    name->setStyleSheet("font-weight:600;padding:0 6px;");
    m_floatOperate = RemoteTheme::button(QString::fromUtf8("申请操作"), "mouse", "primary", bar);
    QPushButton* exit = RemoteTheme::button(QString::fromUtf8("退出全屏"), "fit", 0, bar);
    QPushButton* close = RemoteTheme::button(QString::fromUtf8("断开"), "x", "dangerFill", bar);
    row->addWidget(name);
    row->addWidget(m_floatOperate);
    row->addWidget(exit);
    row->addWidget(close);
    QObject::connect(m_floatOperate, &QPushButton::clicked, [this]() { toggleOperate(); });
    QObject::connect(exit, &QPushButton::clicked, [this]() { setFullScreenMode(false); });
    QObject::connect(close, &QPushButton::clicked, [this]() { this->close(); });
    bar->hide();
    return bar;
}

void SessionWindow::addEvent(const QString& text)
{
    QLabel* label = new QLabel(QDateTime::currentDateTime().toString("HH:mm ") + text);
    label->setStyleSheet("background:#F5F5F5;color:#8C8C8C;font-size:11px;padding:2px 10px;border-radius:10px;");
    QHBoxLayout* row = new QHBoxLayout();
    row->addStretch(1);
    row->addWidget(label);
    row->addStretch(1);
    m_events->insertLayout(m_events->count() - 1, row);
}

void SessionWindow::rebuildMonitors()
{
    const int count = m_tile.monitors < 1 ? 1 : m_tile.monitors;
    const int share = m_tile.share;
    if (count == m_monitorCount && share == m_monitorShare) {
        m_monitors->setCurrent(m_tile.share == 1 ? m_tile.monitor : -1);
        return;
    }
    m_monitorCount = count;
    m_monitorShare = share;
    m_monitors->clear();
    for (int i = 0; i < count; ++i) {
        m_monitors->addButton(QString::number(i + 1));
    }
    const int all = m_monitors->addButton(QString::fromUtf8("全部"));
    m_monitors->setButtonEnabled(all, share == 1, share == 1 ? QString() : QString::fromUtf8("共享窗口时不能切显示器"));
    for (int i = 0; i < count; ++i) {
        m_monitors->setButtonEnabled(i, share == 1, share == 1 ? QString() : QString::fromUtf8("共享窗口时不能切显示器"));
    }
    m_monitors->setCurrent(share == 1 ? (m_tile.monitor == 255 ? all : m_tile.monitor) : -1);
}

void SessionWindow::fillQuality(ConfigMsg* cfg) const
{
    if (!cfg) {
        return;
    }
    // 0 流畅 / 1 均衡 / 2 清晰
    const int index = m_quality ? m_quality->currentIndex() : 1;
    if (index <= 0) {
        cfg->jpegQuality = 40;
        cfg->longEdge = 960;
    } else if (index >= 2) {
        cfg->jpegQuality = 80;
        cfg->longEdge = 1600;
    } else {
        cfg->jpegQuality = static_cast<uint8_t>(RemoteConst::kJpegQuality);
        cfg->longEdge = static_cast<uint16_t>(RemoteConst::kLongEdge);
    }
    cfg->maxFps = static_cast<uint16_t>(m_maxFps ? m_maxFps->value() : RemoteConst::kMaxFpsDefault);
    if (m_codec) {
        cfg->codec = static_cast<uint8_t>(m_codec->currentData().toInt());
    }
}

ConfigMsg SessionWindow::makeConfig() const
{
    ConfigMsg cfg;
    cfg.share = m_tile.share;
    cfg.monitor = static_cast<uint8_t>(m_tile.monitor < 0 ? 0 : m_tile.monitor);
    cfg.title = m_tile.title;
    cfg.process = m_tile.process;
    fillQuality(&cfg);
    return cfg;
}

void SessionWindow::usePictureDefaults()
{
    if (m_quality) {
        m_quality->blockSignals(true);
    }
    if (m_codec) {
        m_codec->blockSignals(true);
    }
    if (m_maxFps) {
        m_maxFps->blockSignals(true);
    }
    if (m_config) {
        if (m_quality && m_config->defaultQuality >= 0 && m_config->defaultQuality <= 2) {
            m_quality->setCurrentIndex(m_config->defaultQuality);
        }
        if (m_codec) {
            const int idx = m_codec->findData(m_config->streamCodec);
            m_codec->setCurrentIndex(idx >= 0 ? idx : 0);
        }
        if (m_maxFps) {
            m_maxFps->setValue(m_config->defaultFps);
        }
    }
    if (m_quality) {
        m_quality->blockSignals(false);
    }
    if (m_codec) {
        m_codec->blockSignals(false);
    }
    if (m_maxFps) {
        m_maxFps->blockSignals(false);
    }
    pushPictureConfig(QString::fromUtf8("已按设置里的默认画质、编码和帧率下发"));
}

void SessionWindow::pushPictureConfig(const QString& reason)
{
    if (!m_online || m_tile.peerId.empty()) {
        return;
    }
    const ConfigMsg cfg = makeConfig();
    m_session->pushConfig(m_tile.peerId, cfg);
    m_audit->add("config.push", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile), reason);
    addEvent(reason);
}

void SessionWindow::pickMonitor(int index)
{
    if (!m_online || m_tile.share != 1 || index < 0 || index > m_monitorCount) {
        return;
    }
    const int monitor = index == m_monitorCount ? 255 : index;
    if (monitor == m_tile.monitor) {
        return;
    }
    ConfigMsg cfg = makeConfig();
    cfg.share = 1;
    cfg.monitor = static_cast<uint8_t>(monitor);
    m_session->pushConfig(m_tile.peerId, cfg);
    const QString text = monitor == 255
        ? QString::fromUtf8("切到全部显示器")
        : QString::fromUtf8("切到显示器 %1").arg(index + 1);
    m_audit->add("config.push", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile), text);
    addEvent(text);
}

void SessionWindow::requestOperate()
{
    if (!m_online || m_tile.operating) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setWindowTitle(QString::fromUtf8("申请操作"));
    dialog.resize(360, 200);
    QRadioButton* byCode = new QRadioButton(QString::fromUtf8("一次性验证码"), &dialog);
    QRadioButton* byPass = new QRadioButton(QString::fromUtf8("无人值守密码"), &dialog);
    QRadioButton* byTrust = new QRadioButton(QString::fromUtf8("信任设备（免密）"), &dialog);
    byCode->setChecked(true);
    QLineEdit* token = new QLineEdit(&dialog);
    token->setPlaceholderText(QString::fromUtf8("输入验证码或密码"));
    QPushButton* ok = RemoteTheme::button(QString::fromUtf8("申请"), 0, "primary", &dialog);
    QPushButton* cancel = RemoteTheme::button(QString::fromUtf8("取消"), 0, 0, &dialog);
    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->addWidget(byCode);
    root->addWidget(byPass);
    root->addWidget(byTrust);
    root->addWidget(token);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    root->addLayout(buttons);
    QObject::connect(byTrust, &QRadioButton::toggled, token, &QLineEdit::setDisabled);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    uint8_t method = AuthMethodCode;
    if (byPass->isChecked()) {
        method = AuthMethodPassword;
    } else if (byTrust->isChecked()) {
        method = AuthMethodTrusted;
    }
    m_session->requestOperate(m_tile.peerId, method, token->text().trimmed().toUtf8().constData());
}

void SessionWindow::toggleOperate()
{
    if (!m_online) {
        return;
    }
    if (m_tile.operating) {
        m_view->setInputEnabled(false);
        m_session->releaseOperate(m_tile.peerId);
    } else {
        requestOperate();
    }
}

void SessionWindow::sendCombo(const uint16_t* keys, int count)
{
    if (!m_tile.operating) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        m_view->sendKey(keys[i], true);
    }
    for (int i = count; i > 0; --i) {
        m_view->sendKey(keys[i - 1], false);
    }
    m_view->setFocus();
}

void SessionWindow::takeShot()
{
    const QImage image = m_view->image();
    if (image.isNull()) {
        flash(QString::fromUtf8("还没有画面，无法截图"));
        return;
    }
    QDir().mkpath(m_config->shotDir);
    QString safe = m_name;
    safe.replace(QRegExp("[\\\\/:*?\"<>|\\s]"), "_");
    const QString path = QDir(m_config->shotDir).filePath(
        safe + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png");
    if (!image.save(path, "PNG")) {
        flash(QString::fromUtf8("截图保存失败：%1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    m_audit->add("screenshot", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile),
        QDir::toNativeSeparators(path));
    addEvent(QString::fromUtf8("截图已保存"));
    flash(QString::fromUtf8("截图已保存：%1").arg(QDir::toNativeSeparators(path)));
}

void SessionWindow::flash(const QString& text)
{
    m_flash = text;
    m_flashUntil = QDateTime::currentMSecsSinceEpoch() + 3000;
    syncControls();
}

void SessionWindow::setFullScreenMode(bool full)
{
    if (m_full == full) {
        return;
    }
    m_full = full;
    m_toolbar->setVisible(!full);
    m_status->setVisible(!full);
    m_panel->setVisible(!full && m_chat->isChecked());
    m_floatBar->setVisible(full);
    if (full) {
        showFullScreen();
        m_floatBar->raise();
    } else {
        showNormal();
    }
    resizeEvent(0);
}

void SessionWindow::resizeEvent(QResizeEvent* event)
{
    if (event) {
        QWidget::resizeEvent(event);
    }
    if (m_floatBar) {
        m_floatBar->adjustSize();
        m_floatBar->move((width() - m_floatBar->width()) / 2, 8);
    }
}

void SessionWindow::closeEvent(QCloseEvent* event)
{
    stopRecord();
    // 先补发未松开的键鼠：交回操作权后控制端就不再转发输入
    m_view->setInputEnabled(false);
    if (m_online && m_tile.operating) {
        m_session->releaseOperate(m_tile.peerId);
    }
    if (m_closed) {
        m_closed(m_key);
    }
    event->accept();
}

void SessionWindow::refresh(const ControlSession::Tile* tile, const QString& name, double fps, double kbps)
{
    m_name = name;
    m_fps = fps;
    m_kbps = kbps;
    m_online = tile && tile->online && !tile->peerId.empty();
    if (tile) {
        const uint64_t seen = m_seen;
        m_tile = *tile;
        m_tile.jpeg.clear();
        // 有新解出帧才刷新；H.264 已在收包线程解成 BGRA（pixels），这里不能再抽最新 Annex-B 包单独解
        if (m_online && tile->pixels && !tile->pixels->empty()
            && tile->pixelsW > 0 && tile->pixelsH > 0 && tile->pixelsSeq != seen) {
            m_view->setImage(*tile->pixels, tile->pixelsW, tile->pixelsH, 3);
            m_seen = tile->pixelsSeq;
        } else if (m_online && tile->codec == 1 && tile->pictureSeq != seen && !tile->jpeg.empty()) {
            m_view->setImage(tile->jpeg, tile->imageW, tile->imageH, 1);
            m_seen = tile->pictureSeq;
            appendRecord(tile->jpeg, tile->imageW, tile->imageH);
        } else if (m_online && tile->codec == 3 && tile->pictureSeq != seen && !tile->jpeg.empty()) {
            m_view->setImage(tile->jpeg, tile->imageW, tile->imageH, 3);
            m_seen = tile->pictureSeq;
        }
        // 主码流没打开或还没解出时，先用监控墙那张 JPEG，避免一直停在「等待画面」
        if (m_online && m_view->image().isNull() && !tile->thumb.empty() && tile->thumbSeq != m_thumbSeen) {
            m_view->setImage(tile->thumb, tile->imageW, tile->imageH, 1);
            m_thumbSeen = tile->thumbSeq;
        }
        if (m_online) {
            m_view->setRemoteCursor(tile->cursorX, tile->cursorY, tile->cursorOn != 0);
            if (tile->cursorShapeSeq != m_cursorSeen) {
                m_cursorSeen = tile->cursorShapeSeq;
                m_view->setCursorShape(tile->cursorShape, tile->cursorHotX, tile->cursorHotY);
            }
            if (tile->operating && !m_recordArmed && (m_config->autoRecord || tile->forceRecord)) {
                m_recordArmed = true;
                if (!m_recording) {
                    toggleRecord();
                }
            }
            if (!tile->operating) {
                m_recordArmed = false;
            }
            if (tile->clipSeq != m_clipSeen) {
                m_clipSeen = tile->clipSeq;
                if (!tile->clipText.empty()) {
                    QApplication::clipboard()->setText(QString::fromUtf8(tile->clipText.c_str()));
                    addEvent(QString::fromUtf8("收到剪贴板文本"));
                } else if (!tile->clipPng.empty()) {
                    QImage image;
                    image.loadFromData(tile->clipPng.data(), static_cast<int>(tile->clipPng.size()), "PNG");
                    if (!image.isNull()) {
                        // 本机两端共用剪贴板。内容没变就再写一次，序列号仍会变，被控端会把同一张图再发回来
                        const QImage have = QApplication::clipboard()->image();
                        const QImage left = have.convertToFormat(QImage::Format_RGB32);
                        const QImage right = image.convertToFormat(QImage::Format_RGB32);
                        if (have.isNull() || left.size() != right.size() || left != right) {
                            QApplication::clipboard()->setImage(image);
                        }
                        addEvent(QString::fromUtf8("收到剪贴板图片"));
                    }
                }
                if (!tile->clipFiles.empty()) {
                    addEvent(QString::fromUtf8("对方剪贴板文件：%1").arg(QString::fromUtf8(tile->clipFiles.c_str()).left(80)));
                    pullClipFiles(QString::fromUtf8(tile->clipFiles.c_str()));
                }
            }
            if (tile->chatSeq != m_chatSeen && !tile->chatLine.empty()) {
                m_chatSeen = tile->chatSeq;
                addEvent(QString::fromUtf8(tile->chatLine.c_str()));
            }
            if (!tile->sysInfo.empty() && m_infoSystem) {
                m_infoSystem->setText(QString::fromUtf8(tile->system.c_str()) + QString::fromUtf8(" · ")
                    + QString::fromUtf8(tile->sysInfo.c_str()));
                m_infoSystem->setWordWrap(true);
            }
            if (tile->viewerSeq != m_viewerSeen && m_extraViewers) {
                m_viewerSeen = tile->viewerSeq;
                while (m_extraViewers->count() > 0) {
                    QLayoutItem* item = m_extraViewers->takeAt(0);
                    delete item->widget();
                    delete item;
                }
                const QString listing = QString::fromUtf8(tile->viewers.c_str());
                const QStringList rows = listing.split('\n', QString::SkipEmptyParts);
                m_viewerCount->setText(QString::fromUtf8("在线 %1 人").arg(2 + rows.size()));
                for (int i = 0; i < rows.size(); ++i) {
                    const QStringList parts = rows.at(i).split('\t');
                    if (parts.size() < 2) {
                        continue;
                    }
                    QWidget* row = new QWidget();
                    QHBoxLayout* line = new QHBoxLayout(row);
                    line->setContentsMargins(0, 0, 0, 0);
                    const QString name = parts.at(0).isEmpty() ? QString::fromUtf8("中控台") : parts.at(0);
                    line->addWidget(new QLabel(name, row), 1);
                    QPushButton* give = new QPushButton(QString::fromUtf8("交接"), row);
                    const QString fp = parts.at(1);
                    QObject::connect(give, &QPushButton::clicked, [this, fp, name]() {
                        if (!m_online || !m_tile.operating) {
                            flash(QString::fromUtf8("先取得操作权，再交接"));
                            return;
                        }
                        OperateMsg op;
                        op.action = Frame::OperateHandoff;
                        op.who = fp.toUtf8().constData();
                        std::string err;
                        m_session->post(m_tile.peerId, Messages::packOperate(op, &err));
                        m_audit->add("operate.handoff", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), name, fp);
                        addEvent(QString::fromUtf8("操作权交给 %1").arg(name));
                    });
                    line->addWidget(give);
                    m_extraViewers->addWidget(row);
                }
            }
            advanceClipPull(*tile);
        }
    }
    if (!m_online) {
        m_tile.operating = false;
    }
    setWindowTitle(QString::fromUtf8("%1 — 中控台").arg(name));
    QLabel* floatName = m_floatBar->findChild<QLabel*>("floatName");
    if (floatName) {
        floatName->setText(name);
    }

    // 状态变化记入会话事件；「正在申请操作…」只显示不记
    if (m_online != m_wasOnline) {
        addEvent(m_online ? QString::fromUtf8("已连接 %1").arg(name) : QString::fromUtf8("设备离线"));
        m_wasOnline = m_online;
    }
    if (m_tile.operating != m_wasOperating) {
        addEvent(m_tile.operating ? QString::fromUtf8("你 取得操作权") : QString::fromUtf8("你 释放了操作权"));
        m_wasOperating = m_tile.operating;
        if (m_tile.operating) {
            m_view->setFocus();
        }
    }
    if (m_tile.hint != m_lastHint) {
        if (!m_tile.hint.empty() && m_tile.hint != "正在申请操作…") {
            addEvent(QString::fromUtf8(m_tile.hint.c_str()));
        }
        m_lastHint = m_tile.hint;
    }
    rebuildMonitors();
    syncControls();
}

void SessionWindow::syncControls()
{
    // 操作按钮（工具条与浮条两处）和依赖操作权的控件
    const bool operating = m_online && m_tile.operating;
    const QString opText = operating ? QString::fromUtf8("释放操作") : QString::fromUtf8("申请操作");
    QPushButton* ops[] = { m_operate, m_floatOperate };
    for (int i = 0; i < 2; ++i) {
        if (!ops[i]) {
            continue;
        }
        ops[i]->setText(opText);
        RemoteTheme::setProp(ops[i], "kind", operating ? "danger" : "primary");
        ops[i]->setIcon(RemoteTheme::icon("mouse", operating ? QColor(RemoteTheme::kDanger) : QColor("#FFFFFF")));
        ops[i]->setEnabled(m_online && (operating || m_tile.operable));
        ops[i]->setToolTip(m_online && !m_tile.operable && !operating ? QString::fromUtf8("被控端当前画面不可操作") : QString());
    }
    m_win->setEnabled(operating);
    m_altTab->setEnabled(operating);
    m_shot->setEnabled(m_online);
    m_quality->setEnabled(m_online);
    if (m_codec) {
        m_codec->setEnabled(m_online);
    }
    m_maxFps->setEnabled(m_online);
    m_view->setInputEnabled(operating);
    m_view->setOperating(operating);

    // 画面提示优先级：临时提示 > 离线 > 操作中 > 会话提示
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_flash.isEmpty() && now < m_flashUntil) {
        m_view->setNotice(m_flash, "blue");
    } else if (!m_online) {
        m_view->setNotice(QString::fromUtf8("设备离线，等待重连…"), "orange");
    } else if (operating) {
        m_view->setNotice(QString::fromUtf8("你正在操作 · 现场动鼠标时自动暂停"), "blue");
    } else if (!m_tile.hint.empty()) {
        m_view->setNotice(QString::fromUtf8(m_tile.hint.c_str()), "orange");
    } else {
        m_view->setNotice(QString(), "blue");
    }

    m_meTag->setText(operating ? QString::fromUtf8("操作中") : QString::fromUtf8("观看"));
    m_meTag->setTone(operating ? "blue" : "gray");
    m_siteName->setText(QString::fromUtf8("%1 · 现场").arg(m_name));

    // 状态栏；延迟超过 150 ms 黄、300 ms 红
    m_stDot->setTone(m_online ? "green" : "gray");
    m_stState->setText(m_online ? QString::fromUtf8("<b style='color:#262626'>已连接</b>") : QString::fromUtf8("<b>已断开</b>"));
    if (m_online && m_tile.imageW > 0) {
        const QString codec = m_tile.codec == 3 ? QString::fromUtf8("原图")
            : (m_tile.codec == 2 ? QString::fromUtf8("H.264") : QString::fromUtf8("JPEG"));
        m_stImage->setText(QString::fromUtf8("<b style='color:#262626'>%1×%2</b> · %3 · <b style='color:#262626'>%4 fps</b>")
                               .arg(m_tile.imageW).arg(m_tile.imageH).arg(codec).arg(m_fps, 0, 'f', 0));
    } else {
        m_stImage->setText("-");
    }
    if (m_online && m_tile.rttMs >= 0) {
        const char* color = m_tile.rttMs > 300 ? "#CF1322" : (m_tile.rttMs > 150 ? "#D48806" : "#389E0D");
        m_stRtt->setText(QString::fromUtf8("延迟 <b style='color:%1'>%2 ms</b>").arg(color).arg(m_tile.rttMs));
    } else {
        m_stRtt->setText(QString::fromUtf8("延迟 -"));
    }
    m_stRate->setText(QString("<b style='color:#262626'>%1</b>").arg(m_online ? rateText(m_kbps) : QString("0 kbps")));
    m_stPerm->setText(operating ? QString::fromUtf8("权限：观看 · 操作") : QString::fromUtf8("权限：观看"));

    // 会话信息页
    m_infoName->setText(m_name);
    m_infoId->setText(m_tile.deviceId.empty() ? QString("-") : QString::fromUtf8(m_tile.deviceId.c_str()));
    m_infoAddress->setText(m_tile.address.empty() ? QString("-") : QString::fromUtf8(m_tile.address.c_str()));
    m_infoSystem->setText(m_tile.system.empty() ? QString("-") : QString::fromUtf8(m_tile.system.c_str()));
    m_infoShare->setText(m_online ? shareText(m_tile) : QString("-"));
    m_infoImage->setText(m_online && m_tile.imageW > 0
        ? QString("%1×%2 · %3").arg(m_tile.imageW).arg(m_tile.imageH).arg(
            m_tile.codec == 3 ? QString::fromUtf8("原图")
                : (m_tile.codec == 2 ? QString::fromUtf8("H.264") : QString::fromUtf8("JPEG")))
        : QString("-"));
    m_infoFps->setText(m_online ? QString("%1 fps").arg(m_fps, 0, 'f', 1) : QString("-"));
    m_infoRate->setText(m_online ? rateText(m_kbps) : QString("-"));
    m_infoRtt->setText(m_online && m_tile.rttMs >= 0 ? QString("%1 ms").arg(m_tile.rttMs) : QString("-"));
    m_infoBytes->setText(QString("%1 MB").arg(m_tile.bytesRx / 1048576.0, 0, 'f', 1));
    if (m_online && m_tile.onlineSince > 0) {
        m_infoUptime->setText(AuditLog::duration(static_cast<qint64>((GetTickCount64() - m_tile.onlineSince) / 1000)));
    } else {
        m_infoUptime->setText("-");
    }
    if (m_infoCrypto && m_infoFingerprint) {
        const QString fp = m_online
            ? QString::fromUtf8(m_session->peerFingerprint(m_tile.peerId).c_str())
            : QString();
        if (!fp.isEmpty()) {
            m_infoCrypto->setText(QString::fromUtf8("TLS"));
            m_infoCrypto->setTone("green");
            m_infoFingerprint->setText(fp);
            m_infoFingerprint->setToolTip(fp);
        } else {
            m_infoCrypto->setText(m_online ? QString::fromUtf8("明文") : QString::fromUtf8("TLS"));
            m_infoCrypto->setTone(m_online ? "orange" : "gray");
            m_infoFingerprint->setText("-");
            m_infoFingerprint->setToolTip(QString());
        }
    }
}

}
