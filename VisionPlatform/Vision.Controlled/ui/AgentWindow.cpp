/**
 * @file AgentWindow.cpp
 * @brief 被控端窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentWindow.h"
#include "AgentFileTransferDialog.h"
#include "AgentService.h"
#include "AgentSettingsDialog.h"
#include "ConfirmDialog.h"
#include "ControlBar.h"
#include "ProcessCombo.h"
#include "RemoteCrash.h"
#include "RemoteFiles.h"
#include "RemoteLogWindow.h"
#include "RemoteTheme.h"
#include "RemoteWidgets.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegExp>
#include <QSaveFile>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QSystemTrayIcon>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include <windows.h>

namespace Vision {

AgentWindow::AgentWindow(QWidget* parent)
    : QWidget(parent)
    , m_confirmDialog(new ConfirmDialog())
    , m_bar(new ControlBar())
    , m_actDesktop(0)
    , m_actWindow(0)
    , m_actAlways(0)
    , m_actTrusted(0)
    , m_actNever(0)
    , m_actLock(0)
    , m_timer(new QTimer(this))
    , m_shade(0)
    , m_tray(0)
    , m_trayMenu(0)
    , m_quit(false)
    , m_confirm(AgentSession::ConfirmNever)
    , m_shownSeq(0)
    , m_codeLabel(0)
    , m_passwordTag(0)
    , m_whitelist(0)
    , m_allowOp(0)
    , m_allowClip(0)
    , m_allowFile(0)
    , m_allowPower(0)
    , m_allowPrivacy(0)
    , m_forceRecord(0)
    , m_fileRoots(0)
    , m_httpOn(0)
    , m_httpPort(0)
    , m_pipelineWorkers(0)
    , m_preferCodec(0)
    , m_httpUrl(0)
    , m_directOn(0)
    , m_directPort(0)
    , m_relayOn(0)
    , m_relayHost(0)
    , m_relayPort(0)
    , m_relayKey(0)
    , m_production(0)
    , m_fpLabel(0)
    , m_settingsBody(0)
    , m_logWin(0)
{
    // 图标优先用 exe 资源，取不到时用内置线条图标
    setWindowTitle(QString::fromUtf8("被控端 — %1").arg(RemoteFiles::computerName()));
    resize(960, 680);
    loadIcon();
    if (m_icon.isNull()) {
        m_icon = RemoteTheme::icon("monitor", QColor(RemoteTheme::kSuccess), 32);
    }
    setWindowIcon(m_icon);
    QApplication::setWindowIcon(m_icon);

    // 主界面只留 ID / 会话 / 共享预览；连接与安全进设置
    QMenuBar* menu = buildMenus();
    QHBoxLayout* top = new QHBoxLayout();
    top->setSpacing(12);
    top->addWidget(buildIdCard());
    top->addWidget(buildSessions(), 1);
    m_settingsBody = buildSettingsBody();
    m_settingsBody->hide();

    QHBoxLayout* foot = new QHBoxLayout();
    foot->setSpacing(8);
    QPushButton* logBtn = RemoteTheme::button(QString::fromUtf8("运行日志"), "list", 0, this);
    logBtn->setToolTip(QString::fromUtf8("查看抓屏、编码、发送等分步耗时"));
    QPushButton* save = RemoteTheme::button(QString::fromUtf8("保存并重连"), 0, "primary", this);
    foot->addStretch(1);
    foot->addWidget(logBtn);
    foot->addWidget(save);
    QObject::connect(logBtn, &QPushButton::clicked, [this]() { openRunLog(); });

    QVBoxLayout* main = new QVBoxLayout();
    main->setContentsMargins(14, 14, 14, 14);
    main->setSpacing(12);
    main->addLayout(top);
    main->addWidget(buildShare(), 1);
    main->addLayout(foot);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->setMenuBar(menu);
    root->addLayout(main, 1);

    m_http.setSession(&m_session);
    loadConfig();
    syncShare();
    applyPolicy();
    if (m_httpOn) {
        QObject::connect(m_httpOn, &QCheckBox::clicked, [this]() { applyHttp(); });
    }
    if (m_httpPort) {
        QObject::connect(m_httpPort, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int) { applyHttp(); });
    }
    if (m_directOn) {
        QObject::connect(m_directOn, &QCheckBox::clicked, [this]() { applyLink(); });
    }
    if (m_directPort) {
        QObject::connect(m_directPort, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int) { applyLink(); });
    }
    if (m_relayOn) {
        QObject::connect(m_relayOn, &QCheckBox::clicked, [this]() { applyLink(); });
    }
    if (m_relayHost) {
        QObject::connect(m_relayHost, &QLineEdit::editingFinished, [this]() { applyLink(); });
    }
    if (m_relayKey) {
        QObject::connect(m_relayKey, &QLineEdit::editingFinished, [this]() { applyLink(); });
    }
    if (m_relayPort) {
        QObject::connect(m_relayPort, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int) { applyLink(); });
    }
    if (m_production) {
        QObject::connect(m_production, &QCheckBox::clicked, [this]() { applyLink(); });
    }

    // 确认框勾了“以后信任”时，优先存中控台证书指纹，没有指纹再退回显示名
    m_confirmDialog->setOnAnswer([this](uint64_t seq, bool allow, bool trust) {
        if (trust) {
            QString key = QString::fromUtf8(m_session.peerFingerprint().c_str());
            if (key.isEmpty()) {
                key = QString::fromUtf8(m_session.pending().controller.c_str());
            }
            if (!key.isEmpty() && !m_trusted.contains(key)) {
                m_trusted.append(key);
                applyPolicy();
            }
        }
        m_session.answerAuth(seq, allow);
    });
    m_bar->setOnPause([this]() { m_session.pauseOperate(); });
    m_bar->setOnDisconnect([this]() { m_session.disconnect(); });
    QObject::connect(save, &QPushButton::clicked, [this]() {
        RemoteCrash::run("AgentWindow.apply", [this]() { apply(); });
    });
    QObject::connect(m_timer, &QTimer::timeout, [this]() {
        RemoteCrash::run("AgentWindow.refresh", [this]() { refresh(); });
    });
    // 按读到的配置立即开始反连
    RemoteCrash::run("AgentWindow.apply_boot", [this]() { apply(); });
    setupTray();
    m_timer->start(200);
}

AgentWindow::~AgentWindow()
{
    m_timer->stop();
    m_http.stop();
    m_session.setHttpView(false);
    m_session.stop();
    delete m_shade;
    delete m_confirmDialog;
    delete m_bar;
}

QMenuBar* AgentWindow::buildMenus()
{
    // 文件：保存并重连、重新连接（解除手动断开）、退出
    QMenuBar* bar = new QMenuBar(this);
    QMenu* file = bar->addMenu(QString::fromUtf8("文件"));
    QAction* save = file->addAction(QString::fromUtf8("保存并重连"));
    QAction* again = file->addAction(RemoteTheme::icon("refresh"), QString::fromUtf8("重新连接"));
    QAction* transfer = file->addAction(RemoteTheme::icon("folder"), QString::fromUtf8("文件传输…"));
    file->addSeparator();
    QAction* quit = file->addAction(QString::fromUtf8("退出"));
    QObject::connect(save, &QAction::triggered, [this]() { apply(); });
    QObject::connect(again, &QAction::triggered, [this]() { m_session.resume(); });
    QObject::connect(transfer, &QAction::triggered, [this]() { openFileTransfer(); });
    QObject::connect(quit, &QAction::triggered, [this]() { quitApp(); });

    // 共享：切换后同步面板单选并立即重连生效
    QMenu* share = bar->addMenu(QString::fromUtf8("共享"));
    QActionGroup* shares = new QActionGroup(this);
    m_actDesktop = share->addAction(RemoteTheme::icon("monitor"), QString::fromUtf8("整个桌面"));
    m_actWindow = share->addAction(RemoteTheme::icon("app"), QString::fromUtf8("进程画面"));
    m_actDesktop->setCheckable(true);
    m_actWindow->setCheckable(true);
    shares->addAction(m_actDesktop);
    shares->addAction(m_actWindow);
    QObject::connect(m_actDesktop, &QAction::triggered, [this]() {
        m_desktop->setChecked(true);
        apply();
    });
    QObject::connect(m_actWindow, &QAction::triggered, [this]() {
        m_window->setChecked(true);
        apply();
    });

    // 安全：acts 下标与 AgentSession::Confirm 取值一致
    QMenu* safe = bar->addMenu(QString::fromUtf8("安全"));
    QActionGroup* confirms = new QActionGroup(this);
    m_actAlways = safe->addAction(QString::fromUtf8("每次都要现场确认"));
    m_actTrusted = safe->addAction(QString::fromUtf8("信任的中控台免确认"));
    m_actNever = safe->addAction(QString::fromUtf8("从不确认"));
    QAction* acts[] = { m_actNever, m_actTrusted, m_actAlways };
    for (int i = 0; i < 3; ++i) {
        acts[i]->setCheckable(true);
        confirms->addAction(acts[i]);
        QObject::connect(acts[i], &QAction::triggered, [this, i]() { setConfirm(i); });
    }
    safe->addSeparator();
    m_actLock = safe->addAction(RemoteTheme::icon("lock"), QString::fromUtf8("远程操作结束后锁屏"));
    m_actLock->setCheckable(true);
    QAction* trusted = safe->addAction(RemoteTheme::icon("shieldOk"), QString::fromUtf8("管理信任的中控台…"));
    QObject::connect(m_actLock, &QAction::triggered, [this](bool on) {
        m_lockOnEnd->setChecked(on);
        applyPolicy();
    });
    QObject::connect(trusted, &QAction::triggered, [this]() { manageTrusted(); });
    safe->addSeparator();
    QAction* install = safe->addAction(QString::fromUtf8("安装 Windows 服务…"));
    QAction* uninstall = safe->addAction(QString::fromUtf8("卸载 Windows 服务"));
    QObject::connect(install, &QAction::triggered, [this]() { installService(true); });
    QObject::connect(uninstall, &QAction::triggered, [this]() { installService(false); });

    QMenu* settings = bar->addMenu(QString::fromUtf8("设置"));
    QAction* openCfg = settings->addAction(RemoteTheme::icon("sliders"), QString::fromUtf8("全部设置…"));
    settings->addSeparator();
    QAction* openLink = settings->addAction(RemoteTheme::icon("link"), QString::fromUtf8("连接…"));
    QAction* openSafe = settings->addAction(RemoteTheme::icon("shield"), QString::fromUtf8("安全…"));
    QAction* openPic = settings->addAction(RemoteTheme::icon("monitor"), QString::fromUtf8("画面…"));
    QObject::connect(openCfg, &QAction::triggered, [this]() { openSettings(0); });
    QObject::connect(openLink, &QAction::triggered, [this]() { openSettings(0); });
    QObject::connect(openSafe, &QAction::triggered, [this]() { openSettings(1); });
    QObject::connect(openPic, &QAction::triggered, [this]() { openSettings(2); });

    QMenu* view = bar->addMenu(QString::fromUtf8("查看"));
    QAction* runLog = view->addAction(RemoteTheme::icon("list"), QString::fromUtf8("运行日志…"));
    QObject::connect(runLog, &QAction::triggered, [this]() { openRunLog(); });

    QMenu* help = bar->addMenu(QString::fromUtf8("帮助"));
    QAction* about = help->addAction(RemoteTheme::icon("info"), QString::fromUtf8("关于"));
    QObject::connect(about, &QAction::triggered, [this]() {
        QMessageBox::about(this, QString::fromUtf8("关于"), QString::fromUtf8(
            "RemoteClient · 被控端\n本机主动连到中控台（反连），画面 JPEG，TLS（Schannel）。\n"
            "现场动鼠标键盘时远程输入自动暂停。可安装服务以支持登录界面与 Ctrl+Alt+Del。"));
    });
    return bar;
}

QFrame* AgentWindow::card(const QString& title, const char* iconName, QVBoxLayout** col)
{
    QFrame* frame = new QFrame(this);
    frame->setObjectName("card");
    QVBoxLayout* c = new QVBoxLayout(frame);
    c->setContentsMargins(16, 14, 16, 14);
    c->setSpacing(12);
    if (!title.isEmpty()) {
        QHBoxLayout* head = new QHBoxLayout();
        head->setSpacing(8);
        if (iconName) {
            QLabel* icon = new QLabel(frame);
            icon->setPixmap(RemoteTheme::pixmap(iconName, QColor(RemoteTheme::kText), 15, devicePixelRatioF()));
            head->addWidget(icon);
        }
        QLabel* text = new QLabel(title, frame);
        text->setProperty("role", "section");
        head->addWidget(text);
        head->addStretch(1);
        c->addLayout(head);
    }
    *col = c;
    return frame;
}

QWidget* AgentWindow::line(QWidget* a, QWidget* b, QWidget* c, QWidget* d, QWidget* e)
{
    QWidget* box = new QWidget(a->parentWidget());
    QHBoxLayout* row = new QHBoxLayout(box);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    QWidget* items[] = { a, b, c, d, e };
    for (int i = 0; i < 5; ++i) {
        if (items[i]) {
            row->addWidget(items[i]);
        }
    }
    row->addStretch(1);
    return box;
}

QFrame* AgentWindow::buildIdCard()
{
    // 蓝色渐变底，卡片内文字白色、按钮半透明
    QFrame* frame = new QFrame(this);
    frame->setObjectName("idCard");
    frame->setFixedWidth(380);
    frame->setStyleSheet(
        "QFrame#idCard{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #1677FF,stop:1 #4096FF);border-radius:6px;}"
        "QFrame#idCard QLabel{color:#FFFFFF;background:transparent;}"
        "QFrame#idCard QPushButton{background:rgba(255,255,255,46);border:none;}"
        "QFrame#idCard QPushButton:hover{background:rgba(255,255,255,80);}"
        "QFrame#idCard QPushButton:disabled{background:rgba(255,255,255,20);}");
    QVBoxLayout* col = new QVBoxLayout(frame);
    col->setContentsMargins(20, 18, 20, 18);
    col->setSpacing(14);

    // 设备 ID：可选中，旁边按钮复制到剪贴板
    QLabel* idKey = new QLabel(QString::fromUtf8("本机设备 ID"), frame);
    idKey->setStyleSheet("font-size:12px;color:rgba(255,255,255,217);");
    const QString id = QString::fromUtf8(m_session.deviceId().c_str());
    QLabel* idText = new QLabel(id, frame);
    idText->setStyleSheet("font-size:30px;font-weight:600;letter-spacing:3px;font-family:'Segoe UI','Microsoft YaHei UI';");
    idText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* copy = RemoteTheme::button(QString(), 0, "icon", frame);
    copy->setIcon(RemoteTheme::icon("copy", QColor("#FFFFFF"), 14));
    copy->setProperty("size", "sm");
    copy->setToolTip(QString::fromUtf8("复制设备 ID"));
    QObject::connect(copy, &QPushButton::clicked, [id]() { QApplication::clipboard()->setText(id); });
    QVBoxLayout* idCol = new QVBoxLayout();
    idCol->setSpacing(2);
    idCol->addWidget(idKey);
    idCol->addWidget(line(idText, copy));
    col->addLayout(idCol);

    QFrame* hr = new QFrame(frame);
    hr->setFixedHeight(1);
    hr->setStyleSheet("background:rgba(255,255,255,64);");
    col->addWidget(hr);

    QLabel* codeKey = new QLabel(QString::fromUtf8("一次性验证码"), frame);
    codeKey->setStyleSheet("font-size:12px;color:rgba(255,255,255,217);");
    m_codeLabel = new QLabel(QString::fromUtf8("000000"), frame);
    m_codeLabel->setStyleSheet("font-size:24px;font-weight:600;letter-spacing:4px;color:#FFFFFF;");
    QPushButton* again = RemoteTheme::button(QString(), 0, "icon", frame);
    again->setIcon(RemoteTheme::icon("refresh", QColor("#FFFFFF"), 14));
    again->setProperty("size", "sm");
    QObject::connect(again, &QPushButton::clicked, [this]() { refreshCodeUi(); });
    QLabel* clockIcon = new QLabel(frame);
    clockIcon->setPixmap(RemoteTheme::pixmap("clock", QColor(255, 255, 255, 217), 12, devicePixelRatioF()));
    QLabel* note = new QLabel(QString::fromUtf8("10 分钟后或认证成功后自动刷新"), frame);
    note->setStyleSheet("font-size:12px;color:rgba(255,255,255,217);");
    QVBoxLayout* codeCol = new QVBoxLayout();
    codeCol->setSpacing(4);
    codeCol->addWidget(codeKey);
    codeCol->addWidget(line(m_codeLabel, again));
    codeCol->addWidget(line(clockIcon, note));
    col->addLayout(codeCol);

    QFrame* hr2 = new QFrame(frame);
    hr2->setFixedHeight(1);
    hr2->setStyleSheet("background:rgba(255,255,255,64);");
    col->addWidget(hr2);
    QLabel* fpKey = new QLabel(QString::fromUtf8("本机证书指纹"), frame);
    fpKey->setStyleSheet("font-size:12px;color:rgba(255,255,255,217);");
    const QString fp = QString::fromUtf8(m_session.fingerprint().c_str());
    m_fpLabel = new QLabel(fp.isEmpty() ? QString::fromUtf8("-") : (fp.left(16) + QString("…")), frame);
    m_fpLabel->setStyleSheet("font-size:12px;font-family:Consolas,'Segoe UI';");
    m_fpLabel->setToolTip(fp);
    m_fpLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* copyFp = RemoteTheme::button(QString(), 0, "icon", frame);
    copyFp->setIcon(RemoteTheme::icon("copy", QColor("#FFFFFF"), 14));
    copyFp->setProperty("size", "sm");
    copyFp->setToolTip(QString::fromUtf8("复制证书指纹"));
    QObject::connect(copyFp, &QPushButton::clicked, [fp]() {
        if (!fp.isEmpty()) {
            QApplication::clipboard()->setText(fp);
        }
    });
    QVBoxLayout* fpCol = new QVBoxLayout();
    fpCol->setSpacing(2);
    fpCol->addWidget(fpKey);
    fpCol->addWidget(line(m_fpLabel, copyFp));
    col->addLayout(fpCol);
    col->addStretch(1);
    return frame;
}

QFrame* AgentWindow::buildSessions()
{
    QVBoxLayout* col = 0;
    QFrame* frame = card(QString(), 0, &col);
    col->setSpacing(10);

    // 标题行：会话数、重新连接（仅手动断开后显示）、断开全部
    QLabel* title = new QLabel(QString::fromUtf8("当前会话"), frame);
    title->setProperty("role", "section");
    m_sessionCount = new TagLabel("0", "gray", frame);
    m_reconnect = RemoteTheme::button(QString::fromUtf8("重新连接"), "refresh", 0, frame);
    m_reconnect->setProperty("size", "sm");
    m_disconnectAll = RemoteTheme::button(QString::fromUtf8("断开全部"), 0, "danger", frame);
    m_disconnectAll->setProperty("size", "sm");
    QHBoxLayout* head = new QHBoxLayout();
    head->setSpacing(8);
    head->addWidget(title);
    head->addWidget(m_sessionCount);
    head->addStretch(1);
    head->addWidget(m_reconnect);
    head->addWidget(m_disconnectAll);
    col->addLayout(head);

    // 服务行：状态点与文字、反连地址、中继（未启用）
    m_svcDot = new StatusDot("gray", frame);
    m_svcState = new QLabel(frame);
    m_svcState->setStyleSheet("font-size:12px;color:#595959;");
    QLabel* linkIcon = new QLabel(frame);
    linkIcon->setPixmap(RemoteTheme::pixmap("link", QColor("#595959"), 12, devicePixelRatioF()));
    m_svcLink = new QLabel(frame);
    m_svcLink->setStyleSheet("font-size:12px;color:#595959;");
    QLabel* relayIcon = new QLabel(frame);
    relayIcon->setPixmap(RemoteTheme::pixmap("server", QColor("#595959"), 12, devicePixelRatioF()));
    QLabel* relay = new QLabel(QString::fromUtf8("中继 未启用"), frame);
    relay->setStyleSheet("font-size:12px;color:#595959;");
    relay->setToolTip(QString::fromUtf8("中继在第五期"));
    QHBoxLayout* svc = new QHBoxLayout();
    svc->setSpacing(6);
    svc->addWidget(m_svcDot);
    svc->addWidget(m_svcState);
    svc->addSpacing(12);
    svc->addWidget(linkIcon);
    svc->addWidget(m_svcLink);
    svc->addSpacing(12);
    svc->addWidget(relayIcon);
    svc->addWidget(relay);
    svc->addStretch(1);
    col->addLayout(svc);

    // 会话行（这一期最多一条）与无会话提示，二者由 refresh 切换显示
    m_sessionRow = new QFrame(frame);
    m_sessionRow->setObjectName("sessionRow");
    m_sessionRow->setStyleSheet("QFrame#sessionRow{border:1px solid #F0F0F0;border-radius:6px;}");
    QHBoxLayout* row = new QHBoxLayout(m_sessionRow);
    row->setContentsMargins(10, 8, 10, 8);
    row->setSpacing(10);
    m_sessionAvatar = new AvatarLabel(QString::fromUtf8("控"), 0, 30, m_sessionRow);
    m_sessionName = new QLabel(m_sessionRow);
    m_sessionMeta = new QLabel(m_sessionRow);
    m_sessionMeta->setProperty("role", "small");
    QVBoxLayout* info = new QVBoxLayout();
    info->setSpacing(2);
    info->addWidget(m_sessionName);
    info->addWidget(m_sessionMeta);
    m_sessionTag = new TagLabel(QString::fromUtf8("观看"), "gray", m_sessionRow);
    m_sessionTime = new QLabel(m_sessionRow);
    m_sessionTime->setProperty("role", "muted");
    QPushButton* cut = RemoteTheme::button(QString::fromUtf8("断开"), 0, 0, m_sessionRow);
    cut->setProperty("size", "sm");
    row->addWidget(m_sessionAvatar);
    row->addLayout(info, 1);
    row->addWidget(m_sessionTag);
    row->addWidget(m_sessionTime);
    row->addWidget(cut);
    col->addWidget(m_sessionRow);

    m_noSession = new QLabel(frame);
    m_noSession->setProperty("role", "muted");
    m_noSession->setAlignment(Qt::AlignCenter);
    m_noSession->setMinimumHeight(48);
    col->addWidget(m_noSession);
    QLabel* noteTitle = new QLabel(QString::fromUtf8("中控台消息"), frame);
    noteTitle->setProperty("role", "small");
    col->addWidget(noteTitle);
    m_notes = new QPlainTextEdit(frame);
    m_notes->setReadOnly(true);
    m_notes->setMaximumHeight(96);
    m_notes->setPlaceholderText(QString::fromUtf8("中控台发来的聊天会记在这里"));
    col->addWidget(m_notes);
    col->addStretch(1);

    // 只有一条连接，“断开”与“断开全部”效果相同
    QObject::connect(cut, &QPushButton::clicked, [this]() { m_session.disconnect(); });
    QObject::connect(m_disconnectAll, &QPushButton::clicked, [this]() { m_session.disconnect(); });
    QObject::connect(m_reconnect, &QPushButton::clicked, [this]() { m_session.resume(); });
    return frame;
}

QFrame* AgentWindow::buildShare()
{
    QVBoxLayout* col = 0;
    QFrame* frame = card(QString::fromUtf8("共享范围"), "monitor", &col);

    // 共享方式与显示器：每块显示器一项，末尾“全部显示器”禁用（后续分期）
    m_desktop = new QRadioButton(QString::fromUtf8("整个桌面"), frame);
    m_window = new QRadioButton(QString::fromUtf8("进程画面"), frame);
    QButtonGroup* kinds = new QButtonGroup(frame);
    kinds->addButton(m_desktop);
    kinds->addButton(m_window);
    m_monitor = new QComboBox(frame);
    m_monitor->setFixedWidth(150);
    const int monitors = WindowCapture::monitorCount();
    for (int i = 0; i < monitors; ++i) {
        m_monitor->addItem(QString::fromUtf8("显示器 %1").arg(i + 1));
    }
    m_monitor->addItem(QString::fromUtf8("全部显示器"));
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_monitor->model());
    if (model) {
        QStandardItem* all = model->item(m_monitor->count() - 1);
        all->setEnabled(false);
        all->setToolTip(QString::fromUtf8("多屏合并在第三期"));
    }
    // 进程画面的条件与注入方式；“前台注入”尚未提供，固定为窗口消息
    m_process = new ProcessCombo(frame);
    m_title = new QLineEdit(frame);
    m_title->setPlaceholderText(QString::fromUtf8("留空则匹配该进程的主窗口"));
    QRadioButton* post = new QRadioButton(QString::fromUtf8("窗口消息"), frame);
    QRadioButton* front = new QRadioButton(QString::fromUtf8("前台注入"), frame);
    QButtonGroup* inject = new QButtonGroup(frame);
    inject->addButton(post);
    inject->addButton(front);
    post->setChecked(true);
    post->setToolTip(QString::fromUtf8("进程画面用 PostMessage 投递，不抢前台；整个桌面用 SendInput"));
    RemoteTheme::markLater(front, QString::fromUtf8("前台注入在第三期"));

    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(12);
    grid->addWidget(line(m_desktop, m_monitor), 0, 0, 1, 2);
    grid->addWidget(m_window, 1, 0, 1, 2);
    const char* const keys[] = { "进程", "标题", "注入" };
    QWidget* values[] = { m_process, m_title, line(post, front) };
    for (int i = 0; i < 3; ++i) {
        QLabel* key = new QLabel(QString::fromUtf8(keys[i]), frame);
        key->setProperty("role", "key");
        key->setFixedWidth(52);
        QHBoxLayout* row = new QHBoxLayout();
        row->setContentsMargins(22, 0, 0, 0);
        row->setSpacing(10);
        row->addWidget(key);
        row->addWidget(values[i], 1);
        grid->addLayout(row, 2 + i, 0, 1, 2);
    }

    // 右侧预览：显示正在送出的画面，由 refresh 更新
    m_previewInfo = new QLabel(frame);
    m_previewInfo->setProperty("role", "small");
    m_preview = new QLabel(frame);
    m_preview->setFixedSize(300, 169);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet("background:#15181D;border:1px solid #E8E8E8;border-radius:4px;color:#8C8C8C;");
    QVBoxLayout* prev = new QVBoxLayout();
    prev->setSpacing(8);
    prev->addWidget(m_previewInfo);
    prev->addWidget(m_preview);
    prev->addStretch(1);

    QHBoxLayout* shareRow = new QHBoxLayout();
    shareRow->setSpacing(16);
    shareRow->addLayout(grid, 1);
    shareRow->addLayout(prev);
    col->addLayout(shareRow);
    col->addStretch(1);

    QObject::connect(m_desktop, &QRadioButton::toggled, [this](bool) { syncShare(); });
    return frame;
}

QWidget* AgentWindow::buildSettingsBody()
{
    QWidget* body = new QWidget(this);
    QVBoxLayout* wrap = new QVBoxLayout(body);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->setSpacing(0);
    QStackedWidget* pages = new QStackedWidget(body);
    pages->setObjectName("settingsPages");
    pages->addWidget(buildConnectionPage(pages));
    pages->addWidget(buildSecurityPage(pages));
    pages->addWidget(buildPicturePage(pages));
    pages->addWidget(buildServicePage(pages));
    wrap->addWidget(pages, 1);
    return body;
}

QWidget* AgentWindow::buildConnectionPage(QWidget* parent)
{
    QWidget* page = new QWidget(parent);
    QVBoxLayout* col = new QVBoxLayout(page);
    col->setContentsMargins(24, 20, 24, 20);
    col->setSpacing(12);

    QLabel* title = new QLabel(QString::fromUtf8("连接"), page);
    title->setProperty("role", "section");
    col->addWidget(title);

    QCheckBox* reverse = new QCheckBox(QString::fromUtf8("反连中控台"), page);
    reverse->setChecked(true);
    reverse->setEnabled(false);
    reverse->setToolTip(QString::fromUtf8("当前唯一的连接方式"));
    reverse->setFixedWidth(120);
    m_host = new QLineEdit(page);
    m_host->setFixedWidth(200);
    m_host->setPlaceholderText("192.168.1.10");
    m_port = new QSpinBox(page);
    m_port->setRange(1, 65535);
    m_port->setFixedWidth(90);
    QPushButton* more = RemoteTheme::button(QString::fromUtf8("再加一台"), "plus", 0, page);
    more->setProperty("size", "sm");
    RemoteTheme::markLater(more, QString::fromUtf8("同时反连多台中控台在第五期"));
    col->addWidget(line(reverse, m_host, m_port, more));

    m_directOn = new QCheckBox(QString::fromUtf8("允许直连"), page);
    m_directOn->setFixedWidth(120);
    m_directOn->setToolTip(QString::fromUtf8("中控台连不上反连时，可以按这个端口连进来。默认关闭"));
    QLabel* directPortLabel = new QLabel(QString::fromUtf8("端口"), page);
    m_directPort = new QSpinBox(page);
    m_directPort->setRange(1, 65535);
    m_directPort->setValue(19102);
    m_directPort->setFixedWidth(90);
    col->addWidget(line(m_directOn, directPortLabel, m_directPort));

    m_httpOn = new QCheckBox(QString::fromUtf8("局域网查看"), page);
    m_httpOn->setFixedWidth(120);
    m_httpOn->setToolTip(QString::fromUtf8("打开后，同一局域网里用浏览器输入下面的地址就能看画面。只看，不能操作。默认关闭"));
    QLabel* httpPortLabel = new QLabel(QString::fromUtf8("端口"), page);
    m_httpPort = new QSpinBox(page);
    m_httpPort->setRange(1, 65535);
    m_httpPort->setValue(19180);
    m_httpPort->setFixedWidth(90);
    col->addWidget(line(m_httpOn, httpPortLabel, m_httpPort));
    m_httpUrl = new QLabel(QString::fromUtf8("关闭"), page);
    m_httpUrl->setProperty("role", "small");
    m_httpUrl->setWordWrap(true);
    m_httpUrl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    col->addWidget(m_httpUrl);

    m_relayOn = new QCheckBox(QString::fromUtf8("中继"), page);
    m_relayOn->setFixedWidth(120);
    m_relayHost = new QLineEdit(page);
    m_relayHost->setFixedWidth(200);
    m_relayHost->setPlaceholderText("relay.company.local");
    m_relayPort = new QSpinBox(page);
    m_relayPort->setRange(1, 65535);
    m_relayPort->setValue(19200);
    m_relayPort->setFixedWidth(90);
    col->addWidget(line(m_relayOn, m_relayHost, m_relayPort));
    m_relayKey = new QLineEdit(page);
    m_relayKey->setEchoMode(QLineEdit::Password);
    m_relayKey->setPlaceholderText(QString::fromUtf8("中继口令"));
    m_relayKey->setFixedWidth(200);
    col->addWidget(line(new QLabel(QString::fromUtf8("口令"), page), m_relayKey));
    col->addStretch(1);
    return page;
}

QWidget* AgentWindow::buildSecurityPage(QWidget* parent)
{
    QWidget* page = new QWidget(parent);
    QVBoxLayout* col = new QVBoxLayout(page);
    col->setContentsMargins(24, 20, 24, 20);
    col->setSpacing(12);

    QLabel* title = new QLabel(QString::fromUtf8("安全"), page);
    title->setProperty("role", "section");
    col->addWidget(title);

    QLabel* confirmKey = new QLabel(QString::fromUtf8("来访确认"), page);
    confirmKey->setProperty("role", "key");
    col->addWidget(confirmKey);
    m_confirmAlways = new QRadioButton(QString::fromUtf8("每次都要现场确认"), page);
    m_confirmTrusted = new QRadioButton(QString::fromUtf8("信任的中控台免确认，其他要现场确认"), page);
    m_confirmNever = new QRadioButton(QString::fromUtf8("从不确认"), page);
    QButtonGroup* confirms = new QButtonGroup(page);
    QRadioButton* radios[] = { m_confirmNever, m_confirmTrusted, m_confirmAlways };
    QVBoxLayout* radioCol = new QVBoxLayout();
    radioCol->setContentsMargins(4, 0, 0, 0);
    radioCol->setSpacing(8);
    for (int i = 2; i >= 0; --i) {
        confirms->addButton(radios[i]);
        radioCol->addWidget(radios[i]);
        QObject::connect(radios[i], &QRadioButton::clicked, [this, i]() { setConfirm(i); });
    }
    col->addLayout(radioCol);

    auto keyLabel = [page](const char* text) {
        QLabel* label = new QLabel(QString::fromUtf8(text), page);
        label->setProperty("role", "key");
        label->setFixedWidth(96);
        return label;
    };
    m_passwordTag = new TagLabel(QString::fromUtf8("未设置"), "gray", page);
    QPushButton* change = RemoteTheme::button(QString::fromUtf8("修改"), 0, 0, page);
    change->setProperty("size", "sm");
    QObject::connect(change, &QPushButton::clicked, [this]() { changePassword(); });
    col->addWidget(line(keyLabel("无人值守密码"), m_passwordTag, change));

    m_trustedCount = new QLabel(page);
    QPushButton* manage = RemoteTheme::button(QString::fromUtf8("管理"), 0, 0, page);
    manage->setProperty("size", "sm");
    col->addWidget(line(keyLabel("信任的中控台"), m_trustedCount, manage));

    m_whitelist = new QLineEdit(page);
    m_whitelist->setPlaceholderText(QString::fromUtf8("空=不限制；逗号分隔，如 192.168.1.10,control-pc"));
    m_whitelist->setProperty("size", "sm");
    m_whitelist->setToolTip(QString::fromUtf8("反连时校验中控台 host；支持 * 放行全部"));
    QWidget* whiteRow = new QWidget(page);
    QHBoxLayout* whiteLine = new QHBoxLayout(whiteRow);
    whiteLine->setContentsMargins(0, 0, 0, 0);
    whiteLine->setSpacing(10);
    whiteLine->addWidget(keyLabel("IP 白名单"));
    whiteLine->addWidget(m_whitelist, 1);
    col->addWidget(whiteRow);
    QObject::connect(m_whitelist, &QLineEdit::editingFinished, [this]() { applyWhitelist(); });

    col->addWidget(keyLabel("允许远端"));
    QGridLayout* allow = new QGridLayout();
    allow->setContentsMargins(4, 0, 0, 0);
    allow->setHorizontalSpacing(8);
    allow->setVerticalSpacing(8);
    const char* const names[] = { "操作", "剪贴板", "文件", "电源", "隐私屏" };
    const char* const tips[] = {
        "关掉后对方不能键鼠，仍可观看",
        "关掉后不同步剪贴板",
        "关掉后不允许列目录和传文件",
        "关掉后不接受远程锁屏、注销、重启、关机",
        "关掉后对方不能开隐私屏"
    };
    QCheckBox** boxes[] = { &m_allowOp, &m_allowClip, &m_allowFile, &m_allowPower, &m_allowPrivacy };
    for (int i = 0; i < 5; ++i) {
        QCheckBox* check = new QCheckBox(QString::fromUtf8(names[i]), page);
        check->setChecked(true);
        check->setToolTip(QString::fromUtf8(tips[i]));
        *boxes[i] = check;
        allow->addWidget(check, i / 2, i % 2);
        QObject::connect(check, &QCheckBox::clicked, [this]() { applyPolicy(); });
    }
    m_lockOnEnd = new QCheckBox(QString::fromUtf8("断开后锁屏"), page);
    m_lockOnEnd->setToolTip(QString::fromUtf8("远程操作过的会话断开时锁定本机"));
    allow->addWidget(m_lockOnEnd, 2, 1);
    m_production = new QCheckBox(QString::fromUtf8("生产保护"), page);
    m_production->setToolTip(QString::fromUtf8("打开后，远程操作必须现场点允许。旁边放 station.json 且 running 为 true 时也会打开"));
    col->addWidget(m_production);
    m_forceRecord = new QCheckBox(QString::fromUtf8("取得操作时强制录屏"), page);
    m_forceRecord->setToolTip(QString::fromUtf8("中控台拿到操作权后自动开始录屏"));
    allow->addWidget(m_forceRecord, 3, 1);
    m_fileRoots = new QLineEdit(page);
    m_fileRoots->setPlaceholderText(QString::fromUtf8("文件根目录，分号分隔，默认 D:\\"));
    allow->addWidget(m_fileRoots, 4, 0, 1, 2);
    col->addLayout(allow);
    col->addStretch(1);

    QObject::connect(manage, &QPushButton::clicked, [this]() { manageTrusted(); });
    QObject::connect(m_lockOnEnd, &QCheckBox::clicked, [this]() { applyPolicy(); });
    QObject::connect(m_forceRecord, &QCheckBox::clicked, [this]() { applyPolicy(); });
    QObject::connect(m_fileRoots, &QLineEdit::editingFinished, [this]() { applyPolicy(); });
    return page;
}

QWidget* AgentWindow::buildPicturePage(QWidget* parent)
{
    QWidget* page = new QWidget(parent);
    QVBoxLayout* col = new QVBoxLayout(page);
    col->setContentsMargins(24, 20, 24, 20);
    col->setSpacing(12);

    QLabel* title = new QLabel(QString::fromUtf8("画面"), page);
    title->setProperty("role", "section");
    col->addWidget(title);

    QLabel* pipeLabel = new QLabel(QString::fromUtf8("抓/编并行"), page);
    pipeLabel->setProperty("role", "key");
    m_pipelineWorkers = new QSpinBox(page);
    m_pipelineWorkers->setRange(0, 1);
    m_pipelineWorkers->setValue(1);
    m_pipelineWorkers->setFixedWidth(90);
    m_pipelineWorkers->setToolTip(QString::fromUtf8("0=抓屏线程内联编码（最省 CPU）；1=独立编码线程，抓/编/发重叠。JPEG 编码不可重入，不要开多条。改完后保存并重连生效"));
    col->addWidget(line(pipeLabel, m_pipelineWorkers,
        new QLabel(QString::fromUtf8("改完后需保存并重连"), page)));

    QLabel* codecLabel = new QLabel(QString::fromUtf8("发送编码"), page);
    codecLabel->setProperty("role", "key");
    m_preferCodec = new QComboBox(page);
    m_preferCodec->addItem(QString::fromUtf8("自动(优先 JPEG)"), 0);
    m_preferCodec->addItem(QString::fromUtf8("JPEG"), 1);
    m_preferCodec->addItem(QString::fromUtf8("H.264"), 2);
    m_preferCodec->addItem(QString::fromUtf8("原图 BGRA"), 3);
    m_preferCodec->setFixedWidth(180);
    m_preferCodec->setToolTip(QString::fromUtf8("本机默认发送方式。中控台会话里指定编码时会覆盖"));
    col->addWidget(line(codecLabel, m_preferCodec));
    QObject::connect(m_preferCodec, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        if (!m_preferCodec) {
            return;
        }
        m_session.setPreferCodec(m_preferCodec->currentData().toInt());
        saveConfig();
    });
    col->addStretch(1);
    return page;
}

QWidget* AgentWindow::buildServicePage(QWidget* parent)
{
    QWidget* page = new QWidget(parent);
    QVBoxLayout* col = new QVBoxLayout(page);
    col->setContentsMargins(24, 20, 24, 20);
    col->setSpacing(12);

    QLabel* title = new QLabel(QString::fromUtf8("服务"), page);
    title->setProperty("role", "section");
    col->addWidget(title);
    QLabel* tip = new QLabel(QString::fromUtf8(
        "安装 Windows 服务后，开机可守护会话代理，并支持登录界面与 Ctrl+Alt+Del。"), page);
    tip->setWordWrap(true);
    tip->setProperty("role", "small");
    col->addWidget(tip);

    QPushButton* install = RemoteTheme::button(QString::fromUtf8("安装服务"), 0, "primary", page);
    QPushButton* uninstall = RemoteTheme::button(QString::fromUtf8("卸载服务"), 0, 0, page);
    col->addWidget(line(install, uninstall));
    QObject::connect(install, &QPushButton::clicked, [this]() { installService(true); });
    QObject::connect(uninstall, &QPushButton::clicked, [this]() { installService(false); });
    col->addStretch(1);
    return page;
}

void AgentWindow::openSettings(int page)
{
    if (!m_settingsBody) {
        return;
    }
    AgentSettingsDialog dialog(m_settingsBody, [this]() {
        RemoteCrash::run("AgentWindow.settings_apply", [this]() { apply(); });
    }, this);
    dialog.setPage(page);
    dialog.exec();
}

void AgentWindow::syncShare()
{
    const bool desktop = m_desktop->isChecked();
    m_monitor->setEnabled(desktop);
    m_process->setEnabled(!desktop);
    m_title->setEnabled(!desktop);
    if (m_actDesktop) {
        m_actDesktop->setChecked(desktop);
        m_actWindow->setChecked(!desktop);
    }
}

void AgentWindow::setConfirm(int confirm)
{
    m_confirm = confirm;
    applyPolicy();
}

void AgentWindow::applyPolicy()
{
    QRadioButton* radios[] = { m_confirmNever, m_confirmTrusted, m_confirmAlways };
    QAction* acts[] = { m_actNever, m_actTrusted, m_actAlways };
    for (int i = 0; i < 3; ++i) {
        radios[i]->setChecked(i == m_confirm);
        acts[i]->setChecked(i == m_confirm);
    }
    m_actLock->setChecked(m_lockOnEnd->isChecked());
    m_trustedCount->setText(QString::fromUtf8("%1 台").arg(m_trusted.size()));
    std::vector<std::string> trusted;
    for (int i = 0; i < m_trusted.size(); ++i) {
        trusted.push_back(m_trusted.at(i).toUtf8().constData());
    }
    m_session.setPolicy(m_confirm, trusted, m_lockOnEnd->isChecked());
    uint32_t allowMask = PermView;
    if (!m_allowOp || m_allowOp->isChecked()) {
        allowMask |= PermOperate;
    }
    if (!m_allowClip || m_allowClip->isChecked()) {
        allowMask |= PermClipboard;
    }
    if (!m_allowFile || m_allowFile->isChecked()) {
        allowMask |= PermFile;
    }
    if (!m_allowPower || m_allowPower->isChecked()) {
        allowMask |= PermPower;
    }
    if (!m_allowPrivacy || m_allowPrivacy->isChecked()) {
        allowMask |= PermPrivacy;
    }
    m_session.setAllow(allowMask);
    if (m_forceRecord) {
        m_session.setForceRecord(m_forceRecord->isChecked());
    }
    if (m_fileRoots) {
        std::vector<std::wstring> roots;
        const QStringList parts = m_fileRoots->text().split(QRegExp("[;\\n]+"), Qt::SkipEmptyParts);
        for (int i = 0; i < parts.size(); ++i) {
            const QString item = parts.at(i).trimmed();
            if (!item.isEmpty()) {
                roots.push_back(item.toStdWString());
            }
        }
        m_session.setFileRoots(roots);
    }
    saveConfig();
}

void AgentWindow::manageTrusted()
{
    QDialog dialog(this);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setWindowTitle(QString::fromUtf8("信任的中控台"));
    dialog.resize(420, 320);
    QListWidget* list = new QListWidget(&dialog);
    list->addItems(m_trusted);
    list->setStyleSheet("QListWidget{border:1px solid #F0F0F0;border-radius:6px;} QListWidget::item{height:32px;padding-left:8px;}");
    QLabel* note = new QLabel(QString::fromUtf8("在确认框勾选“以后信任”会加到这里。优先按中控台证书指纹识别，旧配置里的显示名仍可用。"), &dialog);
    note->setProperty("role", "small");
    note->setWordWrap(true);
    QPushButton* remove = RemoteTheme::button(QString::fromUtf8("移除"), "trash", "danger", &dialog);
    QPushButton* close = RemoteTheme::button(QString::fromUtf8("关闭"), 0, "primary", &dialog);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(remove);
    buttons->addStretch(1);
    buttons->addWidget(close);
    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);
    root->addWidget(note);
    root->addWidget(list, 1);
    root->addLayout(buttons);
    QObject::connect(remove, &QPushButton::clicked, [this, list]() {
        QListWidgetItem* item = list->currentItem();
        if (!item) {
            return;
        }
        m_trusted.removeAll(item->text());
        delete item;
        applyPolicy();
    });
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void AgentWindow::refreshCodeUi()
{
    if (!m_codeLabel) {
        return;
    }
    const QString code = QString::fromUtf8(m_session.refreshCode().c_str());
    QString spaced;
    for (int i = 0; i < code.size(); ++i) {
        if (i > 0 && i % 3 == 0) {
            spaced += ' ';
        }
        spaced += code.at(i);
    }
    m_codeLabel->setText(spaced.isEmpty() ? QString::fromUtf8("— — —") : spaced);
}

void AgentWindow::changePassword()
{
    QDialog dialog(this);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setWindowTitle(QString::fromUtf8("无人值守密码"));
    dialog.resize(360, 180);
    QLineEdit* edit = new QLineEdit(&dialog);
    edit->setEchoMode(QLineEdit::Password);
    edit->setPlaceholderText(QString::fromUtf8("留空则清除密码"));
    QLabel* tip = new QLabel(QString::fromUtf8("密码只存 PBKDF2 哈希，不会明文写入配置。"), &dialog);
    tip->setProperty("role", "small");
    tip->setWordWrap(true);
    QPushButton* ok = RemoteTheme::button(QString::fromUtf8("确定"), 0, "primary", &dialog);
    QPushButton* cancel = RemoteTheme::button(QString::fromUtf8("取消"), 0, 0, &dialog);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);
    root->addWidget(tip);
    root->addWidget(edit);
    root->addLayout(buttons);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_passwordHash = QString::fromUtf8(m_session.setPassword(edit->text().toUtf8().constData()).c_str());
    if (m_passwordTag) {
        m_passwordTag->setText(m_passwordHash.isEmpty() ? QString::fromUtf8("未设置") : QString::fromUtf8("已设置"));
        m_passwordTag->setTone(m_passwordHash.isEmpty() ? "gray" : "green");
    }
    saveConfig();
}

void AgentWindow::loadConfig()
{
    // 先填默认值，文件不存在或缺项时就用它们
    m_host->setText("127.0.0.1");
    m_port->setValue(RemoteConst::kPort);
    m_desktop->setChecked(true);
    QFile file(RemoteFiles::exeFile("controlled.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    m_host->setText(obj.value("host").toString(m_host->text()));
    m_port->setValue(obj.value("port").toInt(RemoteConst::kPort));
    m_title->setText(obj.value("title").toString());
    m_process->setProcess(obj.value("process").toString());
    const int monitor = obj.value("monitor").toInt(0);
    m_monitor->setCurrentIndex(monitor >= 0 && monitor < m_monitor->count() - 1 ? monitor : 0);
    (obj.value("share").toString("desktop") == "window" ? m_window : m_desktop)->setChecked(true);
    const QString confirm = obj.value("confirm").toString("never");
    m_confirm = confirm == "always" ? AgentSession::ConfirmAlways
        : (confirm == "trusted" ? AgentSession::ConfirmUntrusted : AgentSession::ConfirmNever);
    const QJsonArray trusted = obj.value("trusted").toArray();
    for (int i = 0; i < trusted.size(); ++i) {
        const QString name = trusted.at(i).toString();
        if (!name.isEmpty() && !m_trusted.contains(name)) {
            m_trusted.append(name);
        }
    }
    m_lockOnEnd->setChecked(obj.value("lockOnEnd").toBool(false));
    if (m_allowOp) {
        m_allowOp->setChecked(obj.value("allowOperate").toBool(true));
    }
    if (m_allowClip) {
        m_allowClip->setChecked(obj.value("allowClip").toBool(true));
    }
    if (m_allowFile) {
        m_allowFile->setChecked(obj.value("allowFile").toBool(true));
    }
    if (m_allowPower) {
        m_allowPower->setChecked(obj.value("allowPower").toBool(true));
    }
    if (m_allowPrivacy) {
        m_allowPrivacy->setChecked(obj.value("allowPrivacy").toBool(true));
    }
    if (m_forceRecord) {
        m_forceRecord->setChecked(obj.value("forceRecord").toBool(false));
    }
    if (m_fileRoots) {
        m_fileRoots->setText(obj.value("fileRoots").toString());
    }
    if (m_httpOn) {
        m_httpOn->setChecked(obj.value("httpView").toBool(false));
    }
    if (m_httpPort) {
        const int httpPort = obj.value("httpPort").toInt(19180);
        m_httpPort->setValue(httpPort >= 1 && httpPort <= 65535 ? httpPort : 19180);
    }
    if (m_directOn) {
        m_directOn->setChecked(obj.value("direct").toBool(false));
    }
    if (m_directPort) {
        const int directPort = obj.value("directPort").toInt(19102);
        m_directPort->setValue(directPort >= 1 && directPort <= 65535 ? directPort : 19102);
    }
    if (m_relayOn) {
        m_relayOn->setChecked(obj.value("relayOn").toBool(false));
    }
    if (m_relayHost) {
        m_relayHost->setText(obj.value("relayHost").toString());
    }
    if (m_relayPort) {
        m_relayPort->setValue(obj.value("relayPort").toInt(19200));
    }
    if (m_relayKey) {
        m_relayKey->setText(obj.value("relayKey").toString());
    }
    if (m_production) {
        m_production->setChecked(obj.value("production").toBool(false));
    }
    m_passwordHash = obj.value("passwordHash").toString();
    m_session.loadPasswordHash(m_passwordHash.toUtf8().constData());
    if (m_passwordTag) {
        m_passwordTag->setText(m_passwordHash.isEmpty() ? QString::fromUtf8("未设置") : QString::fromUtf8("已设置"));
        m_passwordTag->setTone(m_passwordHash.isEmpty() ? "gray" : "green");
    }
    if (m_whitelist) {
        m_whitelist->setText(obj.value("whitelist").toString());
    }
    if (m_pipelineWorkers) {
        const int workers = obj.value("pipelineWorkers").toInt(1);
        m_pipelineWorkers->setValue(workers <= 0 ? 0 : 1);
    }
    if (m_preferCodec) {
        const int codec = obj.value("preferCodec").toInt(0);
        const int idx = m_preferCodec->findData(codec >= 0 && codec <= 3 ? codec : 0);
        m_preferCodec->blockSignals(true);
        m_preferCodec->setCurrentIndex(idx >= 0 ? idx : 0);
        m_preferCodec->blockSignals(false);
        m_session.setPreferCodec(m_preferCodec->currentData().toInt());
    }
    applyWhitelist();
    refreshCodeUi();
    if (m_fpLabel) {
        const QString fp = QString::fromUtf8(m_session.fingerprint().c_str());
        m_fpLabel->setText(fp.isEmpty() ? QString::fromUtf8("-") : (fp.left(16) + QString("…")));
        m_fpLabel->setToolTip(fp);
    }
}

void AgentWindow::saveConfig() const
{
    QJsonObject obj;
    obj.insert("host", m_host->text().trimmed());
    obj.insert("port", m_port->value());
    obj.insert("title", m_title->text().trimmed());
    obj.insert("process", m_process->process());
    obj.insert("share", m_desktop->isChecked() ? "desktop" : "window");
    obj.insert("monitor", m_monitor->currentIndex() < 0 ? 0 : m_monitor->currentIndex());
    const char* const confirms[] = { "never", "trusted", "always" };
    obj.insert("confirm", confirms[m_confirm < 0 || m_confirm > 2 ? 0 : m_confirm]);
    obj.insert("trusted", QJsonArray::fromStringList(m_trusted));
    obj.insert("lockOnEnd", m_lockOnEnd->isChecked());
    obj.insert("allowOperate", !m_allowOp || m_allowOp->isChecked());
    obj.insert("allowClip", !m_allowClip || m_allowClip->isChecked());
    obj.insert("allowFile", !m_allowFile || m_allowFile->isChecked());
    obj.insert("allowPower", !m_allowPower || m_allowPower->isChecked());
    obj.insert("allowPrivacy", !m_allowPrivacy || m_allowPrivacy->isChecked());
    obj.insert("forceRecord", m_forceRecord && m_forceRecord->isChecked());
    obj.insert("fileRoots", m_fileRoots ? m_fileRoots->text().trimmed() : QString());
    obj.insert("httpView", m_httpOn && m_httpOn->isChecked());
    obj.insert("direct", m_directOn && m_directOn->isChecked());
    obj.insert("directPort", m_directPort ? m_directPort->value() : 19102);
    obj.insert("relayOn", m_relayOn && m_relayOn->isChecked());
    obj.insert("relayHost", m_relayHost ? m_relayHost->text().trimmed() : QString());
    obj.insert("relayPort", m_relayPort ? m_relayPort->value() : 19200);
    obj.insert("relayKey", m_relayKey ? m_relayKey->text() : QString());
    obj.insert("production", m_production && m_production->isChecked());
    obj.insert("httpPort", m_httpPort ? m_httpPort->value() : 19180);
    obj.insert("passwordHash", m_passwordHash);
    obj.insert("whitelist", m_whitelist ? m_whitelist->text().trimmed() : QString());
    obj.insert("pipelineWorkers", m_pipelineWorkers ? m_pipelineWorkers->value() : 0);
    obj.insert("preferCodec", m_preferCodec ? m_preferCodec->currentData().toInt() : 0);
    QSaveFile file(RemoteFiles::exeFile("controlled.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.commit();
    }
}

void AgentWindow::applyWhitelist()
{
    std::vector<std::string> allow;
    if (m_whitelist) {
        const QStringList parts = m_whitelist->text().split(QRegExp("[,;\\s]+"), Qt::SkipEmptyParts);
        for (int i = 0; i < parts.size(); ++i) {
            allow.push_back(parts.at(i).trimmed().toUtf8().constData());
        }
    }
    m_session.setWhitelist(allow);
    saveConfig();
}

void AgentWindow::installService(bool install)
{
    std::string err;
    if (install) {
        if (!AgentService::install(&err)) {
            QMessageBox::warning(this, QString::fromUtf8("安装服务"), QString::fromUtf8(err.c_str()));
            return;
        }
        QMessageBox::information(this, QString::fromUtf8("安装服务"),
            QString::fromUtf8("服务已安装并尝试启动。登录后会拉起会话代理，支持 Ctrl+Alt+Del。"));
    } else {
        AgentService::uninstall(&err);
        QMessageBox::information(this, QString::fromUtf8("卸载服务"),
            err.empty() ? QString::fromUtf8("服务已卸载。") : QString::fromUtf8(err.c_str()));
    }
}

void AgentWindow::apply()
{
    if (!m_host || !m_port || !m_title || !m_process || !m_desktop || !m_monitor || !m_lockOnEnd) {
        RemoteCrash::alert("AgentWindow.apply", "界面控件未就绪", true);
        return;
    }
    const QString host = m_host->text().trimmed();
    const int port = m_port->value();
    if (host.isEmpty() || port <= 0 || port > 65535) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("请填写有效的中控台地址和端口。"));
        return;
    }
    syncShare();
    applyWhitelist();
    saveConfig();
    std::vector<std::string> trusted;
    for (int i = 0; i < m_trusted.size(); ++i) {
        trusted.push_back(m_trusted.at(i).toUtf8().constData());
    }
    m_session.setPolicy(m_confirm, trusted, m_lockOnEnd->isChecked());
    uint32_t allowMask = PermView;
    if (!m_allowOp || m_allowOp->isChecked()) {
        allowMask |= PermOperate;
    }
    if (!m_allowClip || m_allowClip->isChecked()) {
        allowMask |= PermClipboard;
    }
    if (!m_allowFile || m_allowFile->isChecked()) {
        allowMask |= PermFile;
    }
    if (!m_allowPower || m_allowPower->isChecked()) {
        allowMask |= PermPower;
    }
    if (!m_allowPrivacy || m_allowPrivacy->isChecked()) {
        allowMask |= PermPrivacy;
    }
    m_session.setAllow(allowMask);
    if (m_forceRecord) {
        m_session.setForceRecord(m_forceRecord->isChecked());
    }
    if (m_fileRoots) {
        std::vector<std::wstring> roots;
        const QStringList parts = m_fileRoots->text().split(QRegExp("[;\\n]+"), Qt::SkipEmptyParts);
        for (int i = 0; i < parts.size(); ++i) {
            const QString item = parts.at(i).trimmed();
            if (!item.isEmpty()) {
                roots.push_back(item.toStdWString());
            }
        }
        m_session.setFileRoots(roots);
    }
    if (m_pipelineWorkers) {
        m_session.setPipelineWorkers(m_pipelineWorkers->value());
    }
    if (m_preferCodec) {
        m_session.setPreferCodec(m_preferCodec->currentData().toInt());
    }
    std::string err;
    if (!m_session.start(host.toUtf8().constData(), port, m_title->text().trimmed().toStdWString(),
            m_process->process().toStdWString(), m_desktop->isChecked(),
            m_monitor->currentIndex() < 0 ? 0 : m_monitor->currentIndex(), &err)) {
        QMessageBox::warning(this, windowTitle(),
            QString::fromUtf8("启动会话失败：%1").arg(QString::fromUtf8(err.empty() ? "未知错误" : err.c_str())));
    }
    applyHttp();
    applyLink();
}

void AgentWindow::applyLink()
{
    if (m_production) {
        m_session.setProduction(m_production->isChecked());
    }
    if (m_relayOn && m_relayHost && m_relayPort && m_relayKey) {
        m_session.setRelay(m_relayOn->isChecked(), m_relayHost->text().trimmed().toStdString(),
            m_relayPort->value(), m_relayKey->text().toStdString());
    }
    if (m_directOn && m_directPort) {
        std::string err;
        if (!m_session.setDirect(m_directOn->isChecked(), m_directPort->value(), &err) && m_directOn->isChecked()) {
            QMessageBox::warning(this, windowTitle(), QString::fromUtf8("直连监听失败：%1").arg(QString::fromUtf8(err.c_str())));
        }
    }
    saveConfig();
}

void AgentWindow::openRunLog()
{
    if (!m_logWin) {
        m_logWin = new RemoteLogWindow(QString::fromUtf8("被控端 · 运行日志"), this);
    }
    m_logWin->showAndRaise();
}

void AgentWindow::openFileTransfer()
{
    AgentFileTransferDialog dialog(&m_session, this);
    dialog.exec();
}

void AgentWindow::applyHttp()
{
    if (!m_httpOn || !m_httpPort || !m_httpUrl) {
        return;
    }
    const bool on = m_httpOn->isChecked();
    const int port = m_httpPort->value();
    m_session.setHttpView(on);
    if (!on) {
        m_http.stop();
        m_httpUrl->setText(QString::fromUtf8("关闭"));
        saveConfig();
        return;
    }
    if (!(m_http.running() && m_http.port() == port)) {
        std::string err;
        if (!m_http.start(port, &err)) {
            m_httpUrl->setText(QString::fromUtf8(err.c_str()));
            saveConfig();
            return;
        }
    }
    const std::vector<std::string> urls = m_http.urls();
    QStringList lines;
    for (size_t i = 0; i < urls.size(); ++i) {
        lines.append(QString::fromUtf8(urls[i].c_str()));
    }
    m_httpUrl->setText(lines.join(QString::fromUtf8("\n")));
    saveConfig();
}

QString AgentWindow::clock(uint64_t ms)
{
    const uint64_t s = ms / 1000;
    return QString("%1:%2:%3").arg(s / 3600, 2, 10, QChar('0')).arg((s / 60) % 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
}

void AgentWindow::showNotice(const QString& name, const QString& text)
{
    QDialog* box = new QDialog(0, Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowTitle(QString::fromUtf8("中控台提示"));
    box->setMinimumWidth(360);
    QVBoxLayout* col = new QVBoxLayout(box);
    QLabel* who = new QLabel(name.isEmpty() ? QString::fromUtf8("中控台") : name, box);
    who->setStyleSheet("font-weight:600;");
    QLabel* body = new QLabel(text, box);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* ok = new QPushButton(QString::fromUtf8("知道了"), box);
    QObject::connect(ok, &QPushButton::clicked, box, &QDialog::close);
    col->addWidget(who);
    col->addWidget(body);
    col->addWidget(ok, 0, Qt::AlignRight);
    box->show();
    box->raise();
    box->activateWindow();
}

void AgentWindow::refresh()
{
    bool desktop = false;
    int monitor = 0;
    std::wstring title;
    std::wstring process;
    // 中控台改了共享范围：同步到界面并存盘（会话已自行切换，不需要重启）
    if (m_session.takePushed(&desktop, &monitor, &title, &process)) {
        (desktop ? m_desktop : m_window)->setChecked(true);
        if (monitor >= 0 && monitor < m_monitor->count() - 1) {
            m_monitor->setCurrentIndex(monitor);
        }
        m_title->setText(QString::fromStdWString(title));
        m_process->setProcess(QString::fromStdWString(process));
        syncShare();
        saveConfig();
    }

    // 服务状态与会话行
    const AgentSession::Info info = m_session.info();
    const QString controller = QString::fromUtf8(info.controller.c_str());
    const QString address = QString("%1:%2").arg(QString::fromUtf8(info.host.c_str())).arg(info.port);
    m_sessionCount->setText(info.connected ? "1" : "0");
    m_sessionCount->setTone(info.connected ? "blue" : "gray");
    m_reconnect->setVisible(info.suspended);
    m_disconnectAll->setEnabled(info.connected);
    if (info.suspended) {
        m_svcDot->setTone("orange");
        m_svcState->setText(QString::fromUtf8("已手动断开"));
    } else if (info.connected) {
        m_svcDot->setTone("green");
        m_svcState->setText(QString::fromUtf8("服务运行中"));
    } else {
        m_svcDot->setTone("gray");
        m_svcState->setText(QString::fromUtf8("正在连接…"));
    }
    const QString crypto = m_session.tlsReady() ? QString::fromUtf8("TLS") : QString::fromUtf8("TCP");
    m_svcLink->setText(QString::fromUtf8("反连 %1 %2 · %3")
        .arg(controller.isEmpty() ? QString() : controller, address, crypto).simplified());

    m_sessionRow->setVisible(info.connected);
    m_noSession->setVisible(!info.connected);
    if (info.connected) {
        const QString who = controller.isEmpty() ? QString::fromUtf8("中控台") : controller;
        m_sessionAvatar->setText(who.left(1));
        m_sessionName->setText(who);
        m_sessionMeta->setText(QString::fromUtf8("%1 · %2").arg(QString::fromUtf8(info.host.c_str()),
            info.operating ? QString::fromUtf8("观看 操作") : QString::fromUtf8("观看")));
        if (info.operating && info.localPause) {
            m_sessionTag->setText(QString::fromUtf8("本地暂停"));
            m_sessionTag->setTone("orange");
        } else if (info.operating) {
            m_sessionTag->setText(QString::fromUtf8("操作中"));
            m_sessionTag->setTone("blue");
        } else {
            m_sessionTag->setText(QString::fromUtf8("观看"));
            m_sessionTag->setTone("gray");
        }
        m_sessionTime->setText(clock(info.operating ? info.operateMs : info.connectedMs));
    } else {
        m_noSession->setText(info.suspended
            ? QString::fromUtf8("已断开中控台。点“重新连接”恢复")
            : QString::fromUtf8("暂无会话，正在连接 %1 …").arg(address));
    }

    std::vector<AgentSession::Notice> notes;
    m_session.takeNotices(&notes);
    for (size_t i = 0; i < notes.size(); ++i) {
        const QString name = QString::fromUtf8(notes[i].name.c_str());
        const QString text = QString::fromUtf8(notes[i].text.c_str());
        const QString who = name.isEmpty() ? QString::fromUtf8("中控台") : name;
        m_notes->appendPlainText(QTime::currentTime().toString("HH:mm")
            + (notes[i].kind == 1 ? QString::fromUtf8("  弹窗  ") : QString::fromUtf8("  "))
            + who + QString::fromUtf8("：") + text);
        if (notes[i].kind == 1) {
            showNotice(who, text);
        }
    }

    // 新申请弹确认框（同一序号只弹一次）；申请已结束（答复或超时）则收起
    const AgentSession::Pending pending = m_session.pending();
    if (pending.active && pending.seq != m_shownSeq) {
        m_shownSeq = pending.seq;
        m_confirmDialog->showRequest(pending.seq, QString::fromUtf8(pending.controller.c_str()),
            QString::fromUtf8(pending.host.c_str()), pending.secondsLeft, AgentSession::kConfirmMs / 1000);
    } else if (!pending.active && m_confirmDialog->isVisible()) {
        m_confirmDialog->hide();
    }

    // 被操作期间显示顶部提示条
    if (info.privacy) {
        if (!m_shade) {
            m_shade = new QWidget(0, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
            m_shade->setStyleSheet("background:#111111;color:#FFFFFF;");
            QLabel* label = new QLabel(QString::fromUtf8("维护中"), m_shade);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet("font-size:36px;color:#FFFFFF;");
            QVBoxLayout* lay = new QVBoxLayout(m_shade);
            lay->addWidget(label);
        }
        m_shade->setGeometry(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
            GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
        m_shade->show();
        m_shade->raise();
    } else if (m_shade && m_shade->isVisible()) {
        m_shade->hide();
    }
    m_guard.setEnabled(info.privacy);

    if (info.operating) {
        m_bar->setState(controller, info.localPause);
        m_bar->showAtTop();
    } else if (m_bar->isVisible()) {
        m_bar->hide();
    }

    if (m_tray) {
        m_tray->setToolTip(QString::fromUtf8("被控端 · %1").arg(QString::fromUtf8(info.status.c_str())));
    }

    // 预览：没有画面时显示状态文字
    uint32_t imageW = 0;
    uint32_t imageH = 0;
    const CommBytes jpeg = m_session.preview(&imageW, &imageH);
    if (jpeg.empty()) {
        m_previewInfo->setText(QString::fromUtf8("送出预览"));
        m_preview->setPixmap(QPixmap());
        m_preview->setText(QString::fromUtf8(info.status.c_str()));
        return;
    }
    m_previewInfo->setText(QString::fromUtf8("送出预览 · %1×%2 · JPEG").arg(imageW).arg(imageH));
    QPixmap pix;
    if (!pix.loadFromData(jpeg.data(), static_cast<uint>(jpeg.size()), "JPEG")) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(QString::fromUtf8(info.status.c_str()));
        return;
    }
    m_preview->setText(QString());
    m_preview->setPixmap(pix.scaled(m_preview->size() - QSize(2, 2), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void AgentWindow::closeEvent(QCloseEvent* event)
{
    if (m_quit || !m_tray) {
        event->accept();
        QApplication::quit();
        return;
    }
    hide();
    event->ignore();
}

void AgentWindow::setupTray()
{
    m_trayMenu = new QMenu(this);
    QAction* open = m_trayMenu->addAction(QString::fromUtf8("打开界面"));
    QAction* again = m_trayMenu->addAction(QString::fromUtf8("重新连接"));
    m_trayMenu->addSeparator();
    QAction* quit = m_trayMenu->addAction(QString::fromUtf8("退出"));
    QObject::connect(open, &QAction::triggered, [this]() { showFromTray(); });
    QObject::connect(again, &QAction::triggered, [this]() { m_session.resume(); });
    QObject::connect(quit, &QAction::triggered, [this]() { quitApp(); });
    m_tray = new QSystemTrayIcon(m_icon, this);
    m_tray->setContextMenu(m_trayMenu);
    m_tray->setToolTip(QString::fromUtf8("被控端"));
    QObject::connect(m_tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            showFromTray();
        }
    });
    m_tray->show();
}

void AgentWindow::showFromTray()
{
    showNormal();
    raise();
    activateWindow();
}

void AgentWindow::quitApp()
{
    m_quit = true;
    close();
}

void AgentWindow::addIconSize(struct HICON__* icon, int side)
{
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = side;
    info.bmiHeader.biHeight = -side;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = 0;
    HDC screen = GetDC(0);
    HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, 0, 0);
    HDC mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, dib);
    DrawIconEx(mem, 0, 0, reinterpret_cast<HICON>(icon), side, side, 0, 0, DI_NORMAL);
    const QImage image(static_cast<const uchar*>(bits), side, side, side * 4, QImage::Format_ARGB32_Premultiplied);
    m_icon.addPixmap(QPixmap::fromImage(image.copy()));
    SelectObject(mem, old);
    DeleteDC(mem);
    DeleteObject(dib);
    ReleaseDC(0, screen);
}

void AgentWindow::loadIcon()
{
    const int sizes[] = { 16, 24, 32, 48, 64, 256 };
    for (int i = 0; i < 6; ++i) {
        const int side = sizes[i];
        HICON handle = reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(0), MAKEINTRESOURCEW(1), IMAGE_ICON, side, side, LR_DEFAULTCOLOR));
        if (!handle) {
            continue;
        }
        addIconSize(reinterpret_cast<struct HICON__*>(handle), side);
        DestroyIcon(handle);
    }
}

}
