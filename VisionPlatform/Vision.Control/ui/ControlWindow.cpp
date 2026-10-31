/**
 * @file ControlWindow.cpp
 * @brief 控制端主窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ControlWindow.h"
#include "WakeOnLan.h"
#include <QInputDialog>
#include "AddDeviceDialog.h"
#include "AuditDialog.h"
#include "DeviceDialog.h"
#include "RemoteBeacon.h"
#include "RemoteCrash.h"
#include "RemoteLogWindow.h"
#include "Messages.h"
#include "RemoteFiles.h"
#include "RemoteTheme.h"
#include "RemoteWidgets.h"
#include "SessionWindow.h"
#include "SettingsDialog.h"
#include "WallView.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QListWidget>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

#include <windows.h>

namespace Vision {

ControlWindow::ControlWindow(QWidget* parent)
    : QWidget(parent)
    , m_tree(0)
    , m_wall(0)
    , m_timer(new QTimer(this))
    , m_tourTimer(new QTimer(this))
    , m_tray(0)
    , m_trayMenu(0)
    , m_scanRun(false)
    , m_quit(false)
    , m_wallFull(false)
    , m_pageIndex(0)
    , m_tick(0)
    , m_lastSave(0)
    , m_dirty(false)
    , m_relayStatus(0)
    , m_userStatus(0)
    , m_logWin(0)
{
    setWindowTitle(QString::fromUtf8("中控台"));
    resize(1600, 940);
    // 读配置与设备库，清理过期审计，设图标
    m_config.load();
    m_auth.load();
    if (m_auth.loginRequired()) {
        for (;;) {
            bool ok = false;
            const QString user = QInputDialog::getText(0, QString::fromUtf8("登录"), QString::fromUtf8("账号"), QLineEdit::Normal, QString(), &ok);
            if (!ok) {
                QTimer::singleShot(0, qApp, &QApplication::quit);
                break;
            }
            const QString password = QInputDialog::getText(0, QString::fromUtf8("登录"), QString::fromUtf8("密码"), QLineEdit::Password, QString(), &ok);
            if (ok && m_auth.login(user.toStdString(), password.toStdString())) {
                break;
            }
            QMessageBox::warning(0, QString::fromUtf8("登录"), QString::fromUtf8("账号或密码不对"));
        }
    }
    m_store.load();
    m_audit.prune(m_config.auditDays);
    loadIcon();
    if (m_icon.isNull()) {
        m_icon = RemoteTheme::icon("monitor", QColor(RemoteTheme::kPrimary), 32);
    }
    setWindowIcon(m_icon);
    QApplication::setWindowIcon(m_icon);

    // 布局：菜单、工具条在上，设备树与监控墙居中，状态栏在下
    m_tree = new DeviceTree(this);
    m_wall = new WallView(this);
    m_menu = buildMenus();
    m_toolbar = buildToolbar();
    m_status = buildStatus();

    QHBoxLayout* main = new QHBoxLayout();
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    main->addWidget(m_tree);
    main->addWidget(m_wall, 1);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->setMenuBar(m_menu);
    root->addWidget(m_toolbar);
    root->addLayout(main, 1);
    root->addWidget(m_status);

    // 设备树与监控墙的回调
    m_tree->setOnOpen([this](const QString& key) { openSession(key); });
    m_tree->setOnContext([this](const QString& key, const QString& group, const QPoint& pos) { showDeviceMenu(key, group, pos); });
    m_tree->setOnMove([this](const QString& key, const QString& group) { moveToGroup(key, group); });
    m_tree->setOnNewGroup([this]() { newGroup(); });
    m_tree->setOnWake([this]() { wakeSelected(); });
    m_tree->setOnAddFound([this](int index) { addFound(index); });
    m_wall->setOnAction([this](const QString& key, int action) {
        if (action == WallView::ActView) {
            openSession(key);
        } else if (action == WallView::ActOperate) {
            SessionWindow* window = openSession(key);
            if (window) {
                window->requestOperate();
            }
        } else if (action == WallView::ActFile) {
            SessionWindow* window = openSession(key);
            if (window) {
                window->openFiles();
            }
        } else if (action == WallView::ActFull) {
            SessionWindow* window = openSession(key);
            if (window) {
                window->setFullScreenMode(true);
            }
        }
    });
    // 拖到格子上：在设备库里挪到该格原设备之前，监控墙顺序随之改变
    m_wall->setOnDrop([this](const QString& key, int slot) {
        const std::vector<QString> keys = wallKeys();
        const int per = m_rows->value() * m_cols->value();
        const size_t at = static_cast<size_t>(m_pageIndex * per + slot);
        m_store.moveBefore(key, at < keys.size() ? keys[at] : QString());
        m_store.save();
        refreshTree(m_session.tiles());
    });
    m_wall->setOnEmpty([this]() { openAddDevice(); });
    m_wall->setOnContext([this](const QString& key, const QPoint& pos) { showDeviceMenu(key, QString(), pos); });

    // 监听放到事件循环里再开，避免构造未完成时被控端连入把进程打崩，随后互斥残留导致“启动不了”
    m_session.setSelfName(RemoteFiles::selfName().toUtf8().constData());
    setGrid(m_config.rows, m_config.cols);
    RemoteCrash::checkpoint("ControlWindow.ui_ready");
    setupTray();
    RemoteCrash::checkpoint("ControlWindow.tray_ok");

    QObject::connect(m_timer, &QTimer::timeout, [this]() {
        RemoteCrash::run("ControlWindow.refresh", [this]() { refresh(); });
    });
    QObject::connect(m_tourTimer, &QTimer::timeout, [this]() { turnPage(1); });
    QTimer::singleShot(0, this, [this]() {
        RemoteCrash::checkpoint("ControlWindow.deferred_listen");
        RemoteCrash::run("ControlWindow.startListen", [this]() { startListen(); });
        RemoteCrash::checkpoint("ControlWindow.after_listen");
        RemoteCrash::run("ControlWindow.startScan", [this]() { startScan(); });
        RemoteCrash::checkpoint("ControlWindow.after_scan");
        m_timer->start(100);
        RemoteCrash::run("ControlWindow.refreshTree", [this]() { refreshTree(m_session.tiles()); });
        RemoteCrash::checkpoint("ControlWindow.listen_started");
    });
}

ControlWindow::~ControlWindow()
{
    m_timer->stop();
    std::map<QString, SessionWindow*> open = m_sessions;
    m_sessions.clear();
    for (std::map<QString, SessionWindow*>::iterator it = open.begin(); it != open.end(); ++it) {
        it->second->setOnClosed(std::function<void(const QString&)>());
        delete it->second;
    }
    stopScan();
    m_session.stop();
    m_store.save();
}

bool ControlWindow::startHidden() const
{
    return m_config.startHidden;
}

QMenuBar* ControlWindow::buildMenus()
{
    QMenuBar* bar = new QMenuBar(this);

    // 文件：「导出审计日志」打开审计对话框，在那里导出
    QMenu* file = bar->addMenu(QString::fromUtf8("文件"));
    QAction* add = file->addAction(RemoteTheme::icon("plus"), QString::fromUtf8("添加设备…"));
    add->setShortcut(QKeySequence::New);
    QAction* exportAudit = file->addAction(RemoteTheme::icon("download"), QString::fromUtf8("导出审计日志…"));
    file->addSeparator();
    QAction* quit = file->addAction(QString::fromUtf8("退出"));
    QObject::connect(add, &QAction::triggered, [this]() { openAddDevice(); });
    QObject::connect(exportAudit, &QAction::triggered, [this]() { openAudit(); });
    QObject::connect(quit, &QAction::triggered, [this]() { quitApp(); });

    // 设备：作用于设备树当前选中项，弹出前按是否选中设备启用
    QMenu* device = bar->addMenu(QString::fromUtf8("设备"));
    QAction* open = device->addAction(RemoteTheme::icon("eye"), QString::fromUtf8("打开会话"));
    QAction* operate = device->addAction(RemoteTheme::icon("mouse"), QString::fromUtf8("申请操作"));
    QAction* props = device->addAction(RemoteTheme::icon("info"), QString::fromUtf8("属性…"));
    QAction* remove = device->addAction(RemoteTheme::icon("trash"), QString::fromUtf8("删除设备"));
    device->addSeparator();
    QAction* group = device->addAction(RemoteTheme::icon("plus"), QString::fromUtf8("新建分组…"));
    QAction* wake = device->addAction(RemoteTheme::icon("zap"), QString::fromUtf8("远程唤醒"));
    QAction* notice = device->addAction(RemoteTheme::icon("send"), QString::fromUtf8("发消息…"));
    device->addSeparator();
    QAction* scan = device->addAction(RemoteTheme::icon("refresh"), QString::fromUtf8("刷新局域网发现"));
    scan->setShortcut(QKeySequence::Refresh);
    QObject::connect(open, &QAction::triggered, [this]() { openSession(m_tree->selected()); });
    QObject::connect(operate, &QAction::triggered, [this]() {
        SessionWindow* window = openSession(m_tree->selected());
        if (window) {
            window->requestOperate();
        }
    });
    QObject::connect(props, &QAction::triggered, [this]() { openProperties(m_tree->selected()); });
    QObject::connect(remove, &QAction::triggered, [this]() { removeDevice(m_tree->selected()); });
    QObject::connect(group, &QAction::triggered, [this]() { newGroup(); });
    QObject::connect(wake, &QAction::triggered, [this]() { wakeSelected(); });
    QObject::connect(notice, &QAction::triggered, [this]() { sendNotice(); });
    QObject::connect(scan, &QAction::triggered, [this]() {
        foundNow();
        refreshTree(m_session.tiles());
    });
    QObject::connect(device, &QMenu::aboutToShow, [this, open, operate, props, remove]() {
        const bool has = m_store.find(m_tree->selected()) != 0;
        open->setEnabled(has);
        operate->setEnabled(has);
        props->setEnabled(has);
        remove->setEnabled(has);
    });

    // 视图：布局、翻页、轮巡、全屏墙、设备列表开关
    QMenu* view = bar->addMenu(QString::fromUtf8("视图"));
    QActionGroup* grids = new QActionGroup(this);
    for (int n = 1; n <= 4; ++n) {
        QAction* act = view->addAction(QString("%1×%1").arg(n));
        act->setCheckable(true);
        grids->addAction(act);
        m_actGrid.push_back(act);
        QObject::connect(act, &QAction::triggered, [this, n]() { setGrid(n, n); });
    }
    view->addSeparator();
    QAction* prev = view->addAction(RemoteTheme::icon("left"), QString::fromUtf8("上一页"));
    prev->setShortcut(QKeySequence(Qt::Key_PageUp));
    QAction* next = view->addAction(RemoteTheme::icon("right"), QString::fromUtf8("下一页"));
    next->setShortcut(QKeySequence(Qt::Key_PageDown));
    m_actTour = view->addAction(RemoteTheme::icon("play"), QString::fromUtf8("轮巡"));
    m_actTour->setCheckable(true);
    m_actFull = view->addAction(RemoteTheme::icon("full"), QString::fromUtf8("全屏墙"));
    m_actFull->setShortcut(QKeySequence(Qt::Key_F11));
    view->addSeparator();
    m_actTree = view->addAction(QString::fromUtf8("显示设备列表"));
    m_actTree->setCheckable(true);
    m_actTree->setChecked(true);
    QObject::connect(prev, &QAction::triggered, [this]() { turnPage(-1); });
    QObject::connect(next, &QAction::triggered, [this]() { turnPage(1); });
    QObject::connect(m_actTour, &QAction::triggered, [this](bool on) { setTour(on); });
    QObject::connect(m_actFull, &QAction::triggered, [this]() { setWallFull(!m_wallFull); });
    QObject::connect(m_actTree, &QAction::toggled, [this](bool on) { m_tree->setVisible(on && !m_wallFull); });
    // 全屏墙时菜单栏隐藏，快捷键要挂在窗口上才生效；Esc 只在全屏墙时启用
    QAction* esc = new QAction(this);
    esc->setObjectName("escWall");
    esc->setShortcut(QKeySequence(Qt::Key_Escape));
    esc->setEnabled(false);
    addAction(esc);
    addAction(prev);
    addAction(next);
    addAction(m_actFull);
    QObject::connect(esc, &QAction::triggered, [this]() { setWallFull(false); });

    // 工具与帮助；「局域网扫描」复用添加设备对话框
    QMenu* tools = bar->addMenu(QString::fromUtf8("工具"));
    QAction* audit = tools->addAction(RemoteTheme::icon("list"), QString::fromUtf8("审计日志…"));
    QAction* runLog = tools->addAction(RemoteTheme::icon("list"), QString::fromUtf8("运行日志…"));
    QAction* found = tools->addAction(RemoteTheme::icon("wifi"), QString::fromUtf8("局域网扫描…"));
    QObject::connect(audit, &QAction::triggered, [this]() { openAudit(); });
    QObject::connect(runLog, &QAction::triggered, [this]() { openRunLog(); });
    QObject::connect(found, &QAction::triggered, [this]() { openAddDevice(); });

    QMenu* settingsMenu = bar->addMenu(QString::fromUtf8("设置"));
    QAction* settings = settingsMenu->addAction(RemoteTheme::icon("sliders"), QString::fromUtf8("设置…"));
    settings->setShortcut(QKeySequence::Preferences);
    settingsMenu->addSeparator();
    QAction* setPicture = settingsMenu->addAction(RemoteTheme::icon("monitor"), QString::fromUtf8("画面…"));
    QAction* setNetwork = settingsMenu->addAction(RemoteTheme::icon("globe"), QString::fromUtf8("网络…"));
    QAction* setRecord = settingsMenu->addAction(RemoteTheme::icon("film"), QString::fromUtf8("录屏…"));
    QObject::connect(settings, &QAction::triggered, [this]() { openSettings(0); });
    QObject::connect(setPicture, &QAction::triggered, [this]() { openSettings(2); });
    QObject::connect(setNetwork, &QAction::triggered, [this]() { openSettings(1); });
    QObject::connect(setRecord, &QAction::triggered, [this]() { openSettings(4); });

    QMenu* help = bar->addMenu(QString::fromUtf8("帮助"));
    QAction* usage = help->addAction(RemoteTheme::icon("info"), QString::fromUtf8("使用说明"));
    QAction* about = help->addAction(QString::fromUtf8("关于"));
    QObject::connect(usage, &QAction::triggered, [this]() {
        QMessageBox::information(this, QString::fromUtf8("使用说明"), QString::fromUtf8(
            "1. 在被控端把中控台地址设成本机、端口 %1，被控端会主动连上来。\n"
            "2. 连上的设备自动出现在左侧列表和监控墙；也可以用“添加设备”先登记。\n"
            "3. 双击监控墙格子或设备，打开会话窗口；点“申请操作”取得键鼠控制。\n"
            "4. 现场动鼠标时操作自动暂停，需要重新申请。\n"
            "5. 所有连接、操作、截图都记在“工具 → 审计日志”。").arg(m_config.port));
    });
    QObject::connect(about, &QAction::triggered, [this]() {
        QMessageBox::about(this, QString::fromUtf8("关于"), QString::fromUtf8(
            "RemoteMaster · 中控台\n反连 + JPEG 子码流，TLS（Schannel）。\nH.264、文件传输、中继按方案分期提供。"));
    });
    return bar;
}

QWidget* ControlWindow::buildToolbar()
{
    QWidget* bar = new QWidget(this);
    bar->setObjectName("toolbar");
    bar->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* row = new QHBoxLayout(bar);
    row->setContentsMargins(10, 8, 10, 8);
    row->setSpacing(6);

    // 建控件；轮巡间隔不在预设里时追加一项
    QPushButton* add = RemoteTheme::button(QString::fromUtf8("添加设备"), "plus", "primary", bar);
    QPushButton* refresh = RemoteTheme::button(QString::fromUtf8("刷新"), "refresh", 0, bar);
    refresh->setToolTip(QString::fromUtf8("重新扫描局域网并刷新列表"));
    m_layoutSeg = new SegmentBar(bar);
    for (int n = 1; n <= 4; ++n) {
        m_layoutSeg->addButton(QString("%1×%1").arg(n));
    }
    m_rows = new QSpinBox(bar);
    m_cols = new QSpinBox(bar);
    QSpinBox* spins[] = { m_rows, m_cols };
    for (int i = 0; i < 2; ++i) {
        spins[i]->setRange(1, 4);
        spins[i]->setFixedWidth(46);
        spins[i]->setAlignment(Qt::AlignCenter);
    }
    m_tour = RemoteTheme::button(QString::fromUtf8("轮巡"), "play", 0, bar);
    m_tour->setCheckable(true);
    m_tourSeconds = new QComboBox(bar);
    const int seconds[] = { 5, 10, 20, 30, 60 };
    for (int i = 0; i < 5; ++i) {
        m_tourSeconds->addItem(QString::fromUtf8("%1 秒").arg(seconds[i]), seconds[i]);
    }
    const int tourAt = m_tourSeconds->findData(m_config.tourSeconds);
    if (tourAt >= 0) {
        m_tourSeconds->setCurrentIndex(tourAt);
    } else {
        m_tourSeconds->addItem(QString::fromUtf8("%1 秒").arg(m_config.tourSeconds), m_config.tourSeconds);
        m_tourSeconds->setCurrentIndex(m_tourSeconds->count() - 1);
    }
    m_tourSeconds->setFixedWidth(76);
    m_filter = new QComboBox(bar);
    m_filter->setFixedWidth(130);
    m_prev = RemoteTheme::button(QString(), "left", "icon", bar);
    m_next = RemoteTheme::button(QString(), "right", "icon", bar);
    m_page = new QLabel("1 / 1", bar);
    m_page->setStyleSheet("padding:0 6px;color:#595959;");
    QPushButton* full = RemoteTheme::button(QString::fromUtf8("全屏墙"), "full", 0, bar);
    QPushButton* logBtn = RemoteTheme::button(QString::fromUtf8("运行日志"), "list", 0, bar);
    logBtn->setToolTip(QString::fromUtf8("查看接收、解码等分步耗时"));

    QLabel* layoutLabel = new QLabel(QString::fromUtf8("布局"), bar);
    QLabel* rowsLabel = new QLabel(QString::fromUtf8("行"), bar);
    QLabel* colsLabel = new QLabel(QString::fromUtf8("列"), bar);
    QLabel* showLabel = new QLabel(QString::fromUtf8("显示"), bar);
    QLabel* labels[] = { layoutLabel, rowsLabel, colsLabel, showLabel };
    for (int i = 0; i < 4; ++i) {
        labels[i]->setProperty("role", "key");
    }

    // 排布：添加、刷新 | 布局、行列 | 轮巡 | 筛选 …… 翻页 | 全屏墙
    row->addWidget(add);
    row->addWidget(refresh);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(layoutLabel);
    row->addWidget(m_layoutSeg);
    row->addSpacing(6);
    row->addWidget(rowsLabel);
    row->addWidget(m_rows);
    row->addWidget(colsLabel);
    row->addWidget(m_cols);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(m_tour);
    row->addWidget(m_tourSeconds);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(showLabel);
    row->addWidget(m_filter);
    row->addStretch(1);
    row->addWidget(m_prev);
    row->addWidget(m_page);
    row->addWidget(m_next);
    row->addWidget(new ToolSeparator(bar));
    row->addWidget(full);
    row->addWidget(logBtn);

    // 信号连接；改轮巡间隔立即存配置，轮巡中则按新间隔重启
    QObject::connect(add, &QPushButton::clicked, [this]() { openAddDevice(); });
    QObject::connect(logBtn, &QPushButton::clicked, [this]() { openRunLog(); });
    QObject::connect(refresh, &QPushButton::clicked, [this]() {
        foundNow();
        refreshTree(m_session.tiles());
    });
    m_layoutSeg->setOnChanged([this](int index) { setGrid(index + 1, index + 1); });
    for (int i = 0; i < 2; ++i) {
        QObject::connect(spins[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int) {
            setGrid(m_rows->value(), m_cols->value());
        });
    }
    QObject::connect(m_tour, &QPushButton::clicked, [this](bool on) { setTour(on); });
    QObject::connect(m_tourSeconds, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this](int) {
        m_config.tourSeconds = m_tourSeconds->currentData().toInt();
        m_config.save();
        if (m_tourTimer->isActive()) {
            m_tourTimer->start(m_config.tourSeconds * 1000);
        }
    });
    QObject::connect(m_filter, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this](int) {
        m_pageIndex = 0;
    });
    QObject::connect(m_prev, &QPushButton::clicked, [this]() { turnPage(-1); });
    QObject::connect(m_next, &QPushButton::clicked, [this]() { turnPage(1); });
    QObject::connect(full, &QPushButton::clicked, [this]() { setWallFull(true); });
    return bar;
}

QWidget* ControlWindow::buildStatus()
{
    QWidget* bar = new QWidget(this);
    bar->setObjectName("statusbar");
    bar->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* row = new QHBoxLayout(bar);
    row->setContentsMargins(12, 5, 12, 5);
    row->setSpacing(5);
    const qreal dpr = devicePixelRatioF();
    auto icon = [bar, dpr](const char* name) {
        QLabel* label = new QLabel(bar);
        label->setPixmap(RemoteTheme::pixmap(name, QColor(RemoteTheme::kMuted), 13, dpr));
        return label;
    };
    m_listenDot = new StatusDot("gray", bar);
    m_listen = new QLabel(bar);
    m_online = new QLabel(bar);
    m_relayStatus = new QLabel(bar);
    m_relayStatus->setToolTip(QString::fromUtf8("在设置里填写中继地址和口令。反连和直连都没有时才走中继"));
    m_rate = new QLabel(bar);
    const QString who = m_auth.user().empty()
        ? RemoteFiles::userName()
        : QString::fromUtf8(m_auth.user().c_str());
    const QString role = m_auth.role().empty()
        ? RemoteFiles::computerName()
        : QString::fromUtf8(m_auth.role().c_str());
    m_userStatus = new QLabel(QString::fromUtf8("<b style='color:#595959'>%1</b>（%2）")
                                  .arg(who.toHtmlEscaped(), role.toHtmlEscaped()), bar);
    row->addWidget(m_listenDot);
    row->addWidget(m_listen);
    row->addSpacing(14);
    row->addWidget(m_online);
    row->addSpacing(14);
    row->addWidget(icon("server"));
    row->addWidget(m_relayStatus);
    row->addSpacing(14);
    row->addWidget(icon("download"));
    row->addWidget(m_rate);
    row->addStretch(1);
    row->addWidget(icon("user"));
    row->addWidget(m_userStatus);
    return bar;
}

void ControlWindow::startListen()
{
    if (!m_listenDot || !m_listen) {
        RemoteCrash::alert("ControlWindow.startListen", "状态栏未初始化", true);
        return;
    }
    std::string err;
    const int port = m_config.port;
    if (port <= 0 || port > 65535) {
        m_listenDot->setTone("red");
        m_listen->setText(QString::fromUtf8("<span style='color:#CF1322'>端口无效</span>"));
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("监听端口无效：%1").arg(port));
        return;
    }
    m_session.setAdaptive(m_config.adaptive);
    const bool ok = m_session.start(m_config.host.toUtf8().constData(), port, &err);
    m_listenDot->setTone(ok ? "green" : "red");
    if (ok) {
        m_listen->setText(QString::fromUtf8("监听 <b style='color:#595959'>%1:%2</b>").arg(m_config.host).arg(port));
    } else {
        m_listen->setText(QString::fromUtf8("<span style='color:#CF1322'>监听 %1:%2 失败</span>").arg(m_config.host).arg(port));
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("监听 %1:%2 失败：%3\n可在“工具 → 设置 → 网络”修改端口。")
                                                      .arg(m_config.host).arg(port).arg(QString::fromUtf8(err.c_str())));
    }
}

void ControlWindow::startScan()
{
    if (!m_config.discovery || m_scanRun) {
        return;
    }
    m_scanRun = true;
    m_scan = std::thread([this]() {
        try {
            while (m_scanRun) {
                const std::vector<HelloMsg> found = RemoteBeacon::scan(700);
                {
                    std::lock_guard<std::mutex> lock(m_scanMu);
                    m_found = found;
                }
                for (int i = 0; i < 50 && m_scanRun; ++i) {
                    Sleep(100);
                }
            }
        } catch (const std::exception& e) {
            RemoteCrash::alert("ControlWindow.scan", e.what(), false);
        } catch (...) {
            RemoteCrash::alert("ControlWindow.scan", "发现线程异常退出", false);
        }
        m_scanRun = false;
    });
}

void ControlWindow::stopScan()
{
    m_scanRun = false;
    if (m_scan.joinable()) {
        m_scan.join();
    }
    if (!m_config.discovery) {
        std::lock_guard<std::mutex> lock(m_scanMu);
        m_found.clear();
    }
}

std::vector<HelloMsg> ControlWindow::foundNow()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const std::vector<HelloMsg> found = RemoteBeacon::scan(700);
    QApplication::restoreOverrideCursor();
    std::lock_guard<std::mutex> lock(m_scanMu);
    m_found = found;
    return found;
}

QString ControlWindow::keyOf(const ControlSession::Tile& tile) const
{
    if (tile.bindKey.empty()) {
        return QString();
    }
    return DeviceStore::keyFor(QString::fromUtf8(tile.deviceId.c_str()), tile.share,
        QString::fromUtf8(tile.address.c_str()), QString::fromUtf8(tile.source.c_str()));
}

const ControlSession::Tile* ControlWindow::tileOf(const std::vector<ControlSession::Tile>& tiles, const QString& key) const
{
    const ControlSession::Tile* any = 0;
    for (size_t i = 0; i < tiles.size(); ++i) {
        if (keyOf(tiles[i]) != key) {
            continue;
        }
        if (tiles[i].online && !tiles[i].peerId.empty()) {
            return &tiles[i];
        }
        if (!any) {
            any = &tiles[i];
        }
    }
    return any;
}

QString ControlWindow::nameOf(const QString& key) const
{
    const DeviceStore::Device* device = m_store.find(key);
    return device ? m_store.displayName(*device) : key;
}

std::vector<QString> ControlWindow::wallKeys() const
{
    const QString filter = m_filter->currentData().toString();
    std::vector<QString> keys;
    const std::vector<DeviceStore::Device>& devices = m_store.devices();
    for (size_t i = 0; i < devices.size(); ++i) {
        if (filter == "\x01online") {
            const std::map<QString, Seen>::const_iterator it = m_seen.find(devices[i].key);
            if (it == m_seen.end() || !it->second.online) {
                continue;
            }
        } else if (!filter.isEmpty() && devices[i].group != filter) {
            continue;
        }
        keys.push_back(devices[i].key);
    }
    return keys;
}

void ControlWindow::syncStreams(const std::vector<ControlSession::Tile>& tiles)
{
    // 当前墙页上的设备也要订主码流，否则只靠 2 fps 缩略图，比会话窗慢很多
    const std::vector<QString> wall = wallKeys();
    const int per = m_rows->value() * m_cols->value();
    std::map<QString, bool> onPage;
    for (int i = 0; i < per; ++i) {
        const size_t index = static_cast<size_t>(m_pageIndex * per + i);
        if (index >= wall.size()) {
            break;
        }
        onPage[wall[index]] = true;
    }

    const uint16_t fps = static_cast<uint16_t>(
        m_config.defaultFps >= RemoteConst::kMaxFpsMin && m_config.defaultFps <= RemoteConst::kMaxFpsMax
            ? m_config.defaultFps : RemoteConst::kMaxFpsDefault);

    std::map<std::string, int> live;
    for (size_t i = 0; i < tiles.size(); ++i) {
        const ControlSession::Tile& tile = tiles[i];
        if (!tile.online || tile.peerId.empty() || tile.bindKey.empty()) {
            continue;
        }
        // 会话窗口按设备 ID 索引，不是连接时的 d:/w: 地址键
        const QString key = keyOf(tile);
        const bool session = !key.isEmpty() && m_sessions.find(key) != m_sessions.end();
        const bool wallVis = !key.isEmpty() && onPage.find(key) != onPage.end();
        const bool wantMain = session || wallVis;
        // 1=仅缩略图，2=主码流（墙上/会话）
        const int want = wantMain ? 2 : 1;
        live[tile.peerId] = want;
        if (m_stream[tile.peerId] == want) {
            continue;
        }
        SubscribeMsg thumb;
        thumb.stream = 2;
        thumb.enable = wantMain ? 0 : 1;
        thumb.maxFps = 2;
        thumb.jpegQuality = 60;
        thumb.longEdge = 480;
        m_session.subscribe(tile.peerId, thumb);
        SubscribeMsg full;
        full.stream = 1;
        full.enable = wantMain ? 1 : 0;
        full.maxFps = fps;
        full.jpegQuality = static_cast<uint8_t>(RemoteConst::kJpegQuality);
        full.longEdge = static_cast<uint16_t>(RemoteConst::kLongEdge);
        full.codec = static_cast<uint8_t>(m_config.streamCodec >= 0 && m_config.streamCodec <= 3
            ? m_config.streamCodec : 0);
        m_session.subscribe(tile.peerId, full);
        m_stream[tile.peerId] = want;
    }
    for (std::map<std::string, int>::iterator it = m_stream.begin(); it != m_stream.end();) {
        if (live.find(it->first) == live.end()) {
            it = m_stream.erase(it);
        } else {
            ++it;
        }
    }
}

void ControlWindow::refresh()
{
    const std::vector<ControlSession::Tile> tiles = m_session.tiles();
    m_tick += 1;
    trackTiles(tiles);
    syncStreams(tiles);
    updateRates(tiles);
    refreshWall(tiles);
    for (std::map<QString, SessionWindow*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        const Rate& rate = m_rates[it->first];
        it->second->refresh(tileOf(tiles, it->first), nameOf(it->first), rate.fps, rate.kbps);
    }
    if (m_tick % 5 == 0) {
        refreshTree(tiles);
        refreshStatus(tiles);
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_dirty && now - m_lastSave > 30000) {
        m_store.save();
        m_lastSave = now;
        m_dirty = false;
    }
}

void ControlWindow::trackTiles(const std::vector<ControlSession::Tile>& tiles)
{
    // 报了设备 ID 的在线连接：把按 IP 手动登记的 d:地址 条目归到 ID 下，旧 key 的离线会话窗口与画面缓存随之作废
    bool changed = false;
    for (size_t i = 0; i < tiles.size(); ++i) {
        if (!tiles[i].online || tiles[i].bindKey.empty() || tiles[i].deviceId.empty()) {
            continue;
        }
        const QString address = QString::fromUtf8(tiles[i].address.c_str());
        const QString manual = DeviceStore::keyFor(QString(), 1, address, QString());
        if (!m_store.adoptAddress(QString::fromUtf8(tiles[i].deviceId.c_str()), address)) {
            continue;
        }
        changed = true;
        std::map<QString, SessionWindow*>::iterator open = m_sessions.find(manual);
        if (open != m_sessions.end()) {
            open->second->close();
        }
        m_pictures.erase(manual);
        m_wallH264.erase(manual);
        m_rates.erase(manual);
        m_seen.erase(manual);
    }

    // 同一 key 可能有多条状态，只用 tileOf 选出的那条；在线的写入设备库
    std::map<QString, const ControlSession::Tile*> current;
    for (size_t i = 0; i < tiles.size(); ++i) {
        const QString key = keyOf(tiles[i]);
        if (key.isEmpty()) {
            continue;
        }
        const ControlSession::Tile* best = tileOf(tiles, key);
        current[key] = best;
        if (best == &tiles[i] && tiles[i].online) {
            if (m_store.touch(key, QString::fromUtf8(tiles[i].name.c_str()), QString::fromUtf8(tiles[i].address.c_str()),
                    QString::fromUtf8(tiles[i].system.c_str()))) {
                changed = true;
                m_audit.add("device.add", nameOf(key), QString::fromUtf8(tiles[i].deviceId.c_str()),
                    QString::fromUtf8(tiles[i].address.c_str()), QString::fromUtf8("被控端首次连入，自动加入列表"));
            }
            m_dirty = true;
        }
    }
    // 新设备或合并了条目立即存盘，仅刷新 lastSeen 的留给 refresh() 节流保存
    if (changed) {
        m_store.save();
        m_lastSave = QDateTime::currentMSecsSinceEpoch();
        m_dirty = false;
    }

    // 与上一轮对比，边沿变化写审计
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (std::map<QString, const ControlSession::Tile*>::const_iterator it = current.begin(); it != current.end(); ++it) {
        const ControlSession::Tile& tile = *it->second;
        Seen& seen = m_seen[it->first];
        const bool online = tile.online && !tile.peerId.empty();
        const QString name = nameOf(it->first);
        const QString id = QString::fromUtf8(tile.deviceId.c_str());
        const QString address = QString::fromUtf8(tile.address.c_str());
        if (online && !seen.online) {
            seen.since = now;
            m_audit.add("session.start", name, id, address, QString::fromUtf8("反连 · ") + SessionWindow::shareText(tile));
        } else if (!online && seen.online) {
            m_audit.add("session.end", name, id, address, QString::fromUtf8("连接断开"), (now - seen.since) / 1000);
        }
        const bool operating = online && tile.operating;
        if (operating && !seen.operating) {
            seen.operateSince = now;
            m_audit.add("operate.start", name, id, address, QString::fromUtf8("申请通过"));
        } else if (!operating && seen.operating) {
            m_audit.add("operate.end", name, id, address,
                tile.hint.empty() ? QString::fromUtf8("主动释放") : QString::fromUtf8(tile.hint.c_str()), (now - seen.operateSince) / 1000);
        }
        // 被拒按提示文字识别，文字须与 ControlSession 处理 AuthAck 时一致
        if (tile.hint != seen.hint) {
            const QString hint = QString::fromUtf8(tile.hint.c_str());
            if (hint.startsWith(QString::fromUtf8("现场拒绝")) || hint.startsWith(QString::fromUtf8("现场无人响应"))
                || hint.startsWith(QString::fromUtf8("已有人在操作"))) {
                m_audit.add("operate.reject", name, id, address, hint);
            }
        }
        seen.online = online;
        seen.operating = operating;
        seen.hint = tile.hint;
    }
}

void ControlWindow::updateRates(const std::vector<ControlSession::Tile>& tiles)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (size_t i = 0; i < tiles.size(); ++i) {
        const QString key = keyOf(tiles[i]);
        if (key.isEmpty() || tileOf(tiles, key) != &tiles[i]) {
            continue;
        }
        Rate& rate = m_rates[key];
        if (!tiles[i].online) {
            rate = Rate();
            continue;
        }
        if (rate.at == 0 || tiles[i].pictureSeq < rate.seq || tiles[i].thumbSeq < rate.thumbSeq
            || tiles[i].bytesRx < rate.bytes) {
            rate.at = now;
            rate.seq = tiles[i].pictureSeq;
            rate.thumbSeq = tiles[i].thumbSeq;
            rate.bytes = tiles[i].bytesRx;
            continue;
        }
        const qint64 dt = now - rate.at;
        if (dt < 1000) {
            continue;
        }
        // 监控墙默认只订缩略图；有主码流时优先用主码流帧差
        const uint64_t dMain = tiles[i].pictureSeq - rate.seq;
        const uint64_t dThumb = tiles[i].thumbSeq - rate.thumbSeq;
        const uint64_t dFrames = dMain > 0 ? dMain : dThumb;
        rate.fps = dFrames * 1000.0 / dt;
        rate.kbps = (tiles[i].bytesRx - rate.bytes) * 8.0 / dt;
        rate.seq = tiles[i].pictureSeq;
        rate.thumbSeq = tiles[i].thumbSeq;
        rate.bytes = tiles[i].bytesRx;
        rate.at = now;
    }
}

void ControlWindow::refreshWall(const std::vector<ControlSession::Tile>& tiles)
{
    // 页码收敛到有效范围，更新翻页控件
    const std::vector<QString> keys = wallKeys();
    const int per = m_rows->value() * m_cols->value();
    const int pages = keys.empty() || per <= 0 ? 1 : (static_cast<int>(keys.size()) + per - 1) / per;
    if (m_pageIndex >= pages) {
        m_pageIndex = pages - 1;
    }
    if (m_pageIndex < 0) {
        m_pageIndex = 0;
    }
    m_page->setText(QString("%1 / %2").arg(m_pageIndex + 1).arg(pages));
    m_prev->setEnabled(m_pageIndex > 0);
    m_next->setEnabled(m_pageIndex + 1 < pages);

    // 当前页每格的文字和状态
    std::vector<WallView::Cell> cells;
    for (int i = 0; i < per; ++i) {
        const size_t index = static_cast<size_t>(m_pageIndex * per + i);
        if (index >= keys.size()) {
            break;
        }
        const QString& key = keys[index];
        const ControlSession::Tile* tile = tileOf(tiles, key);
        const DeviceStore::Device* device = m_store.find(key);
        WallView::Cell cell;
        cell.key = key;
        cell.name = nameOf(key);
        cell.online = tile && tile->online && !tile->peerId.empty();
        cell.operating = cell.online && tile->operating;
        if (cell.online) {
            cell.share = tile->share == 1 ? QString::fromUtf8("桌面") : QString::fromUtf8("窗口");
            cell.fps = QString("%1 fps").arg(static_cast<int>(m_rates[key].fps + 0.5));
        } else {
            cell.fps = QString::fromUtf8("—");
        }
        if (cell.operating) {
            cell.badge = QString::fromUtf8("%1 操作中").arg(RemoteFiles::userName());
        }
        if (tile) {
            cell.station = QString::fromUtf8(tile->station.c_str());
            cell.alarm = tile->stationNg;
        }
        if (device && device->lastSeen.isValid()) {
            cell.lastSeen = device->lastSeen.date() == QDate::currentDate()
                ? device->lastSeen.toString("HH:mm:ss") : device->lastSeen.toString("yyyy-MM-dd HH:mm");
        }
        // 优先收包线程已解出的 BGRA（H.264 逐帧）；否则主码流 JPEG/原图；再不行用缩略图
        Picture& picture = m_pictures[key];
        const bool havePixels = tile && tile->pixels && !tile->pixels->empty()
            && tile->pixelsW > 0 && tile->pixelsH > 0;
        const CommBytes* wallJpeg = 0;
        uint64_t wallSeq = 0;
        uint8_t wallCodec = 1;
        if (havePixels) {
            wallSeq = tile->pixelsSeq;
            wallCodec = 3;
        } else if (tile && !tile->jpeg.empty() && tile->codec != 2) {
            wallJpeg = &tile->jpeg;
            wallSeq = tile->pictureSeq;
            wallCodec = tile->codec == 0 ? 1 : tile->codec;
        } else if (tile && !tile->thumb.empty()) {
            wallJpeg = &tile->thumb;
            wallSeq = tile->thumbSeq;
            wallCodec = 1;
        }
        if (tile && (havePixels || (wallJpeg && !wallJpeg->empty()))
            && (wallSeq != picture.seq || picture.gray == cell.online || picture.codec != wallCodec)) {
            if (picture.codec != 0 && picture.codec != wallCodec) {
                m_wallH264[key].closeDecoder();
                picture.image = QImage();
            }
            if (wallSeq != picture.seq || picture.image.isNull() || (picture.gray && cell.online)
                || picture.codec != wallCodec) {
                bool decoded = false;
                if (havePixels) {
                    picture.image = RemoteTheme::imageFromBgra(tile->pixels->data(),
                        static_cast<int>(tile->pixels->size()),
                        static_cast<int>(tile->pixelsW), static_cast<int>(tile->pixelsH));
                    decoded = !picture.image.isNull();
                } else if (wallCodec == 3) {
                    picture.image = RemoteTheme::imageFromBgra(wallJpeg->data(), static_cast<int>(wallJpeg->size()),
                        static_cast<int>(tile->imageW), static_cast<int>(tile->imageH));
                    decoded = !picture.image.isNull();
                } else {
                    picture.image = RemoteTheme::imageFromJpeg(wallJpeg->data(), static_cast<int>(wallJpeg->size()));
                    decoded = !picture.image.isNull();
                }
                if (decoded) {
                    picture.codec = wallCodec;
                }
            }
            if (!cell.online && !picture.image.isNull()) {
                picture.image = picture.image.convertToFormat(QImage::Format_Grayscale8);
            }
            picture.seq = wallSeq;
            picture.gray = !cell.online;
        }
        cell.image = picture.image;
        cells.push_back(cell);
    }
    m_wall->setCells(cells);
}

void ControlWindow::refreshTree(const std::vector<ControlSession::Tile>& tiles)
{
    // 设备行；状态色与右侧附注：离线灰、操作中绿锁、不可操作橙、在线绿
    std::vector<DeviceTree::Row> rows;
    const std::vector<DeviceStore::Device>& devices = m_store.devices();
    for (size_t i = 0; i < devices.size(); ++i) {
        const DeviceStore::Device& device = devices[i];
        const ControlSession::Tile* tile = tileOf(tiles, device.key);
        const bool online = tile && tile->online && !tile->peerId.empty();
        DeviceTree::Row row;
        row.key = device.key;
        row.name = m_store.displayName(device);
        row.group = device.group;
        row.address = device.address;
        row.deviceId = device.key.contains(':') ? QString() : device.key;
        row.lastOpened = device.lastOpened;
        if (!online) {
            row.tone = "gray";
            row.metaText = device.lastSeen.isValid()
                ? QString::fromUtf8("离线 %1").arg(device.lastSeen.date() == QDate::currentDate()
                        ? device.lastSeen.toString("HH:mm") : device.lastSeen.toString("MM-dd"))
                : QString::fromUtf8("未连接过");
        } else if (tile->operating) {
            row.tone = "green";
            row.metaIcon = "lock";
            row.metaText = RemoteFiles::userName();
        } else if (!tile->operable) {
            row.tone = "orange";
            row.metaIcon = tile->share == 1 ? "monitor" : "app";
            row.metaText = QString::fromUtf8("不可操作");
            row.metaColor = "#D46B08";
        } else {
            row.tone = "green";
            row.metaIcon = tile->share == 1 ? "monitor" : "app";
            row.metaText = tile->share == 1 ? QString::fromUtf8("桌面") : QString::fromUtf8("窗口");
        }
        rows.push_back(row);
    }
    m_tree->setRows(m_store.groups(), rows);

    // 局域网发现页：标出已在库里的，签名不变就不重建
    std::vector<HelloMsg> found;
    {
        std::lock_guard<std::mutex> lock(m_scanMu);
        found = m_found;
    }
    std::vector<DeviceTree::Found> list;
    QString sig;
    for (size_t i = 0; i < found.size(); ++i) {
        DeviceTree::Found item;
        item.name = QString::fromUtf8(found[i].name.c_str());
        item.address = QString::fromUtf8(found[i].address.c_str());
        item.deviceId = QString::fromUtf8(found[i].deviceId.c_str());
        item.added = m_store.find(DeviceStore::keyFor(item.deviceId, found[i].share, item.address,
                         QString::fromUtf8(found[i].source.c_str()))) != 0;
        sig += item.name + item.address + (item.added ? "1" : "0") + "|";
        list.push_back(item);
    }
    if (sig != m_foundSig) {
        m_foundSig = sig;
        m_tree->setFound(list);
    }

    // 筛选下拉框：分组表变化才重建，尽量保留原选择
    const QString keep = m_filter->currentData().toString();
    QStringList want;
    want << QString::fromUtf8("全部设备") << QString::fromUtf8("在线设备") << m_store.groups();
    QStringList have;
    for (int i = 0; i < m_filter->count(); ++i) {
        have << m_filter->itemText(i);
    }
    if (want != have) {
        m_filter->clear();
        m_filter->addItem(QString::fromUtf8("全部设备"), QString());
        m_filter->addItem(QString::fromUtf8("在线设备"), QString("\x01online"));
        const QStringList& groups = m_store.groups();
        for (int i = 0; i < groups.size(); ++i) {
            m_filter->addItem(groups.at(i), groups.at(i));
        }
        const int at = m_filter->findData(keep);
        m_filter->setCurrentIndex(at < 0 ? 0 : at);
    }
}

void ControlWindow::refreshStatus(const std::vector<ControlSession::Tile>& tiles)
{
    int online = 0;
    double kbps = 0;
    const std::vector<DeviceStore::Device>& devices = m_store.devices();
    for (size_t i = 0; i < devices.size(); ++i) {
        const ControlSession::Tile* tile = tileOf(tiles, devices[i].key);
        if (tile && tile->online && !tile->peerId.empty()) {
            online += 1;
            kbps += m_rates[devices[i].key].kbps;
        }
    }
    const int total = static_cast<int>(devices.size());
    m_online->setText(QString::fromUtf8("在线 <b style='color:#595959'>%1 / %2</b>").arg(online).arg(total));
    if (m_relayStatus) {
        m_relayStatus->setText(m_config.relayOn
            ? QString::fromUtf8("中继 <b style='color:#722ED1'>%1</b>").arg(m_config.relayHost.toHtmlEscaped())
            : QString::fromUtf8("中继 <b>未启用</b>"));
    }
    m_rate->setText(QString::fromUtf8("下行 <b style='color:#595959'>%1</b>").arg(SessionWindow::rateText(kbps)));
    if (m_tray) {
        m_tray->setToolTip(QString::fromUtf8("中控台 · 在线 %1/%2").arg(online).arg(total));
    }
}

void ControlWindow::setGrid(int rows, int cols)
{
    rows = rows < 1 ? 1 : (rows > 4 ? 4 : rows);
    cols = cols < 1 ? 1 : (cols > 4 ? 4 : cols);
    m_rows->blockSignals(true);
    m_cols->blockSignals(true);
    m_rows->setValue(rows);
    m_cols->setValue(cols);
    m_rows->blockSignals(false);
    m_cols->blockSignals(false);
    m_layoutSeg->setCurrent(rows == cols ? rows - 1 : -1);
    for (size_t i = 0; i < m_actGrid.size(); ++i) {
        m_actGrid[i]->setChecked(rows == cols && static_cast<int>(i) == rows - 1);
    }
    m_wall->setLayoutCount(rows, cols);
    m_pageIndex = 0;
    if (m_config.rows != rows || m_config.cols != cols) {
        m_config.rows = rows;
        m_config.cols = cols;
        m_config.save();
    }
}

void ControlWindow::turnPage(int delta)
{
    const int per = m_rows->value() * m_cols->value();
    const int count = static_cast<int>(wallKeys().size());
    const int pages = count == 0 || per <= 0 ? 1 : (count + per - 1) / per;
    m_pageIndex = (m_pageIndex + delta + pages) % pages;
}

void ControlWindow::setTour(bool on)
{
    m_tour->setChecked(on);
    m_actTour->setChecked(on);
    m_tour->setText(on ? QString::fromUtf8("停止轮巡") : QString::fromUtf8("轮巡"));
    m_tour->setIcon(RemoteTheme::icon(on ? "pause" : "play"));
    if (on) {
        m_tourTimer->start(m_config.tourSeconds * 1000);
    } else {
        m_tourTimer->stop();
    }
}

void ControlWindow::setWallFull(bool full)
{
    if (m_wallFull == full) {
        return;
    }
    m_wallFull = full;
    m_menu->setVisible(!full);
    m_toolbar->setVisible(!full);
    m_status->setVisible(!full);
    m_tree->setVisible(!full && m_actTree->isChecked());
    QAction* esc = findChild<QAction*>("escWall");
    if (esc) {
        esc->setEnabled(full);
    }
    if (full) {
        showFullScreen();
    } else {
        showNormal();
    }
}

void ControlWindow::wakeSelected()
{
    DeviceStore::Device* device = m_store.find(m_tree->selected());
    if (!device) {
        return;
    }
    QString mac = device->mac;
    if (mac.trimmed().isEmpty()) {
        mac = QInputDialog::getText(this, QString::fromUtf8("远程唤醒"), QString::fromUtf8("MAC 地址"), QLineEdit::Normal, mac);
        if (mac.trimmed().isEmpty()) {
        return;
    }
        device->mac = mac.trimmed();
        m_store.save();
    }
    WakeOnLan wol;
    QString err;
    if (!wol.send(device->mac, &err)) {
        QMessageBox::warning(this, windowTitle(), err);
        return;
    }
    m_audit.add("device.wake", m_store.displayName(*device), device->key, device->address, device->mac);
}

void ControlWindow::sendNotice()
{
    const std::vector<DeviceStore::Device>& devices = m_store.devices();
    if (devices.empty()) {
        QMessageBox::information(this, windowTitle(), QString::fromUtf8("还没有设备"));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("发消息"));
    dialog.setMinimumWidth(440);
    QVBoxLayout* col = new QVBoxLayout(&dialog);
    QComboBox* kind = new QComboBox(&dialog);
    kind->addItem(QString::fromUtf8("聊天消息"), 0);
    kind->addItem(QString::fromUtf8("弹窗提示"), 1);
    QComboBox* scope = new QComboBox(&dialog);
    scope->addItem(QString::fromUtf8("选中的一台"), 0);
    scope->addItem(QString::fromUtf8("勾选多台"), 1);
    scope->addItem(QString::fromUtf8("全部在线"), 2);
    const QString selected = m_tree->selected();
    if (selected.isEmpty()) {
        scope->setCurrentIndex(2);
    }
    QListWidget* list = new QListWidget(&dialog);
    list->setMinimumHeight(160);
    list->setVisible(false);
    for (size_t i = 0; i < devices.size(); ++i) {
        QListWidgetItem* row = new QListWidgetItem(m_store.displayName(devices[i]), list);
        row->setData(Qt::UserRole, devices[i].key);
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        row->setCheckState(devices[i].key == selected ? Qt::Checked : Qt::Unchecked);
    }
    QTextEdit* edit = new QTextEdit(&dialog);
    edit->setPlaceholderText(QString::fromUtf8("输入要发给被控端的内容"));
    edit->setMinimumHeight(88);
    QPushButton* cancel = new QPushButton(QString::fromUtf8("取消"), &dialog);
    QPushButton* send = new QPushButton(QString::fromUtf8("发送"), &dialog);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(send);
    col->addWidget(new QLabel(QString::fromUtf8("类型"), &dialog));
    col->addWidget(kind);
    col->addWidget(new QLabel(QString::fromUtf8("发给"), &dialog));
    col->addWidget(scope);
    col->addWidget(list);
    col->addWidget(edit);
    col->addLayout(buttons);
    QObject::connect(scope, QOverload<int>::of(&QComboBox::currentIndexChanged), [list](int index) {
        list->setVisible(index == 1);
    });
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(send, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString text = edit->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }
    QStringList keys;
    const int mode = scope->currentData().toInt();
    if (mode == 0) {
        if (selected.isEmpty() || !m_store.find(selected)) {
            QMessageBox::information(this, windowTitle(), QString::fromUtf8("先在左侧选中一台设备"));
            return;
        }
        keys << selected;
    } else if (mode == 1) {
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->checkState() == Qt::Checked) {
                keys << list->item(i)->data(Qt::UserRole).toString();
            }
        }
        if (keys.isEmpty()) {
            QMessageBox::information(this, windowTitle(), QString::fromUtf8("先勾选要发送的设备"));
            return;
        }
    } else {
        for (size_t i = 0; i < devices.size(); ++i) {
            keys << devices[i].key;
        }
    }
    const std::vector<ControlSession::Tile> tiles = m_session.tiles();
    const uint8_t chatKind = kind->currentData().toInt() == 1 ? 1 : 0;
    const QString name = RemoteFiles::userName();
    int sent = 0;
    int offline = 0;
    std::string err;
    for (int i = 0; i < keys.size(); ++i) {
        const ControlSession::Tile* tile = tileOf(tiles, keys[i]);
        if (!tile || !tile->online || tile->peerId.empty()) {
            offline += 1;
            continue;
        }
        m_session.post(tile->peerId, Messages::packChat(name.toUtf8().constData(), text.toUtf8().constData(), &err, chatKind));
        sent += 1;
    }
    m_audit.add("chat", name, QString(), chatKind == 1 ? QString::fromUtf8("弹窗") : QString::fromUtf8("聊天"),
        QString::fromUtf8("发给 %1 台，%2 台不在线").arg(sent).arg(offline));
    if (sent == 0) {
        QMessageBox::information(this, windowTitle(), QString::fromUtf8("这些被控端都不在线，消息没有发出去"));
    }
}

SessionWindow* ControlWindow::openSession(const QString& key)
{
    if (key.isEmpty() || !m_store.find(key)) {
        return 0;
    }
    {
        const std::vector<ControlSession::Tile> tiles = m_session.tiles();
        const ControlSession::Tile* tile = tileOf(tiles, key);
        const DeviceStore::Device* device = m_store.find(key);
        if (device && (!tile || !tile->online || tile->peerId.empty())) {
            std::string err;
            QString id = key.startsWith("d:") ? QString() : key;
            m_session.dialDevice(device->address.toStdString(), id.toStdString(),
                m_config.relayOn ? m_config.relayHost.toStdString() : std::string(),
                m_config.relayPort, m_config.relayKey.toStdString(), &err);
        }
    }
    std::map<QString, SessionWindow*>::iterator it = m_sessions.find(key);
    if (it != m_sessions.end()) {
        it->second->showNormal();
        it->second->raise();
        it->second->activateWindow();
        return it->second;
    }
    SessionWindow* window = new SessionWindow(&m_session, &m_config, &m_audit, key);
    window->setWindowIcon(m_icon);
    window->setOnClosed([this](const QString& closed) { m_sessions.erase(closed); });
    window->setOnBack([this]() { showFromTray(); });
    m_sessions[key] = window;
    const std::vector<ControlSession::Tile> tiles = m_session.tiles();
    const Rate& rate = m_rates[key];
    window->refresh(tileOf(tiles, key), nameOf(key), rate.fps, rate.kbps);
    window->show();
    DeviceStore::Device* device = m_store.find(key);
    if (device) {
        device->lastOpened = QDateTime::currentDateTime();
        m_store.save();
    }
    refreshTree(tiles);
    return window;
}

void ControlWindow::openAddDevice()
{
    AddDeviceDialog dialog(&m_store, [this]() { return foundNow(); }, QString(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const std::vector<AddDeviceDialog::Pick> picks = dialog.picks();
    for (size_t i = 0; i < picks.size(); ++i) {
        const AddDeviceDialog::Pick& pick = picks[i];
        m_store.touch(pick.key, QString(), pick.address, pick.system);
        DeviceStore::Device* device = m_store.find(pick.key);
        if (!device) {
            continue;
        }
        device->name = pick.name;
        if (!pick.alias.isEmpty()) {
            device->alias = pick.alias;
        }
        device->group = dialog.group();
        device->lastSeen = QDateTime();
        m_audit.add("device.add", m_store.displayName(*device), pick.key.contains(':') ? QString() : pick.key, pick.address,
            QString::fromUtf8("手动添加"));
    }
    m_store.save();
    refreshTree(m_session.tiles());
}

void ControlWindow::addFound(int index)
{
    std::vector<HelloMsg> found;
    {
        std::lock_guard<std::mutex> lock(m_scanMu);
        found = m_found;
    }
    if (index < 0 || index >= static_cast<int>(found.size())) {
        return;
    }
    const HelloMsg& hello = found[static_cast<size_t>(index)];
    const QString id = QString::fromUtf8(hello.deviceId.c_str());
    const QString address = QString::fromUtf8(hello.address.c_str());
    const QString key = DeviceStore::keyFor(id, hello.share, address, QString::fromUtf8(hello.source.c_str()));
    if (m_store.find(key)) {
        return;
    }
    m_store.touch(key, QString::fromUtf8(hello.name.c_str()), address, QString::fromUtf8(hello.system.c_str()));
    DeviceStore::Device* device = m_store.find(key);
    if (device) {
        device->lastSeen = QDateTime();
        m_audit.add("device.add", m_store.displayName(*device), id, address, QString::fromUtf8("从局域网发现添加"));
    }
    m_store.save();
    refreshTree(m_session.tiles());
}

void ControlWindow::openProperties(const QString& key)
{
    if (!m_store.find(key)) {
        return;
    }
    const std::vector<ControlSession::Tile> tiles = m_session.tiles();
    DeviceDialog dialog(&m_store, &m_audit, &m_session, key, tileOf(tiles, key), this);
    dialog.exec();
    refreshTree(m_session.tiles());
}

void ControlWindow::openSettings(int page)
{
    const int oldFps = m_config.defaultFps;
    const int oldQuality = m_config.defaultQuality;
    const int oldCodec = m_config.streamCodec;
    SettingsDialog dialog(&m_config, m_session.listening(), m_audit.dir(), [this](bool moved) {
        if (moved) {
            m_session.stop();
            startListen();
        }
        stopScan();
        startScan();
        setGrid(m_config.rows, m_config.cols);
        if (m_tourTimer->isActive()) {
            m_tourTimer->start(m_config.tourSeconds * 1000);
        }
        m_audit.prune(m_config.auditDays);
        m_session.setAdaptive(m_config.adaptive);
    }, this);
    dialog.setPage(page);
    dialog.exec();
    if (m_config.defaultFps != oldFps || m_config.defaultQuality != oldQuality
        || m_config.streamCodec != oldCodec) {
        m_stream.clear();
        for (std::map<QString, SessionWindow*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            it->second->usePictureDefaults();
        }
    }
}

void ControlWindow::openAudit()
{
    AuditDialog dialog(&m_audit, this);
    dialog.exec();
}

void ControlWindow::openRunLog()
{
    if (!m_logWin) {
        m_logWin = new RemoteLogWindow(QString::fromUtf8("中控台 · 运行日志"), this);
    }
    m_logWin->showAndRaise();
}

void ControlWindow::renameDevice(const QString& key)
{
    DeviceStore::Device* device = m_store.find(key);
    if (!device) {
        return;
    }
    bool ok = false;
    const QString text = QInputDialog::getText(this, QString::fromUtf8("重命名"), QString::fromUtf8("名称（留空则用被控端上报的名称）"),
        QLineEdit::Normal, m_store.displayName(*device), &ok).trimmed();
    if (!ok) {
        return;
    }
    device->alias = text == device->name ? QString() : text;
    m_store.save();
    refreshTree(m_session.tiles());
}

void ControlWindow::removeDevice(const QString& key)
{
    const DeviceStore::Device* device = m_store.find(key);
    if (!device) {
        return;
    }
    const QString name = m_store.displayName(*device);
    if (QMessageBox::question(this, QString::fromUtf8("删除设备"),
            QString::fromUtf8("从列表删除“%1”？\n被控端仍连着本机时，会再次自动出现。").arg(name)) != QMessageBox::Yes) {
        return;
    }
    std::map<QString, SessionWindow*>::iterator it = m_sessions.find(key);
    if (it != m_sessions.end()) {
        it->second->close();
    }
    m_audit.add("device.remove", name, key.contains(':') ? QString() : key, device->address, QString::fromUtf8("手动删除"));
    m_store.remove(key);
    m_store.save();
    m_pictures.erase(key);
    m_wallH264.erase(key);
    refreshTree(m_session.tiles());
}

void ControlWindow::moveToGroup(const QString& key, const QString& group)
{
    DeviceStore::Device* device = m_store.find(key);
    if (!device || device->group == group) {
        return;
    }
    device->group = group;
    m_store.save();
    refreshTree(m_session.tiles());
}

void ControlWindow::newGroup()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QString::fromUtf8("新建分组"), QString::fromUtf8("分组名称"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    if (!m_store.addGroup(name)) {
        QMessageBox::information(this, QString::fromUtf8("新建分组"), QString::fromUtf8("已有同名分组。"));
        return;
    }
    m_store.save();
    refreshTree(m_session.tiles());
}

void ControlWindow::renameGroup(const QString& group)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QString::fromUtf8("重命名分组"), QString::fromUtf8("分组名称"),
        QLineEdit::Normal, group, &ok).trimmed();
    if (!ok || name.isEmpty() || name == group) {
        return;
    }
    m_store.renameGroup(group, name);
    m_store.save();
    refreshTree(m_session.tiles());
}

void ControlWindow::removeGroup(const QString& group)
{
    if (QMessageBox::question(this, QString::fromUtf8("删除分组"),
            QString::fromUtf8("删除分组“%1”？组内设备移到“未分组”。").arg(group)) != QMessageBox::Yes) {
        return;
    }
    m_store.removeGroup(group);
    m_store.save();
    refreshTree(m_session.tiles());
}

void ControlWindow::showDeviceMenu(const QString& key, const QString& group, const QPoint& pos)
{
    QMenu menu(this);
    if (!key.isEmpty()) {
        QAction* open = menu.addAction(RemoteTheme::icon("eye"), QString::fromUtf8("打开会话"));
        QAction* operate = menu.addAction(RemoteTheme::icon("mouse"), QString::fromUtf8("申请操作"));
        QAction* file = menu.addAction(RemoteTheme::icon("folder"), QString::fromUtf8("文件传输"));
        menu.addSeparator();
        QAction* props = menu.addAction(RemoteTheme::icon("info"), QString::fromUtf8("属性…"));
        QAction* rename = menu.addAction(RemoteTheme::icon("edit"), QString::fromUtf8("重命名…"));
        QMenu* move = menu.addMenu(QString::fromUtf8("移到分组"));
        const DeviceStore::Device* device = m_store.find(key);
        QStringList targets;
        targets << QString() << m_store.groups();
        for (int i = 0; i < targets.size(); ++i) {
            const QString target = targets.at(i);
            QAction* act = move->addAction(target.isEmpty() ? QString::fromUtf8("未分组") : target);
            act->setCheckable(true);
            act->setChecked(device && device->group == target);
            QObject::connect(act, &QAction::triggered, [this, key, target]() { moveToGroup(key, target); });
        }
        menu.addSeparator();
        QAction* remove = menu.addAction(RemoteTheme::icon("trash", QColor(RemoteTheme::kDanger)), QString::fromUtf8("删除设备"));
        QObject::connect(open, &QAction::triggered, [this, key]() { openSession(key); });
        QObject::connect(operate, &QAction::triggered, [this, key]() {
            SessionWindow* window = openSession(key);
            if (window) {
                window->requestOperate();
            }
        });
        QObject::connect(file, &QAction::triggered, [this, key]() {
            SessionWindow* window = openSession(key);
            if (window) {
                window->openFiles();
            }
        });
        QObject::connect(props, &QAction::triggered, [this, key]() { openProperties(key); });
        QObject::connect(rename, &QAction::triggered, [this, key]() { renameDevice(key); });
        QObject::connect(remove, &QAction::triggered, [this, key]() { removeDevice(key); });
    } else if (!group.isEmpty()) {
        QAction* rename = menu.addAction(RemoteTheme::icon("edit"), QString::fromUtf8("重命名分组…"));
        QAction* remove = menu.addAction(RemoteTheme::icon("trash", QColor(RemoteTheme::kDanger)), QString::fromUtf8("删除分组"));
        menu.addSeparator();
        QAction* create = menu.addAction(RemoteTheme::icon("plus"), QString::fromUtf8("新建分组…"));
        QObject::connect(rename, &QAction::triggered, [this, group]() { renameGroup(group); });
        QObject::connect(remove, &QAction::triggered, [this, group]() { removeGroup(group); });
        QObject::connect(create, &QAction::triggered, [this]() { newGroup(); });
    } else {
        QAction* add = menu.addAction(RemoteTheme::icon("plus"), QString::fromUtf8("添加设备…"));
        QAction* create = menu.addAction(RemoteTheme::icon("plus"), QString::fromUtf8("新建分组…"));
        QObject::connect(add, &QAction::triggered, [this]() { openAddDevice(); });
        QObject::connect(create, &QAction::triggered, [this]() { newGroup(); });
    }
    menu.exec(pos);
}

void ControlWindow::closeEvent(QCloseEvent* event)
{
    if (m_quit || !m_config.trayOnClose || !m_tray) {
        m_quit = true;
        event->accept();
        QApplication::quit();
        return;
    }
    hide();
    event->ignore();
}

void ControlWindow::setupTray()
{
    m_trayMenu = new QMenu(this);
    QAction* open = m_trayMenu->addAction(QString::fromUtf8("打开主界面"));
    QAction* audit = m_trayMenu->addAction(QString::fromUtf8("审计日志"));
    m_trayMenu->addSeparator();
    QAction* quit = m_trayMenu->addAction(QString::fromUtf8("退出"));
    QObject::connect(open, &QAction::triggered, [this]() { showFromTray(); });
    QObject::connect(audit, &QAction::triggered, [this]() {
        showFromTray();
        openAudit();
    });
    QObject::connect(quit, &QAction::triggered, [this]() { quitApp(); });
    m_tray = new QSystemTrayIcon(m_icon, this);
    m_tray->setContextMenu(m_trayMenu);
    m_tray->setToolTip(QString::fromUtf8("中控台"));
    QObject::connect(m_tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            showFromTray();
        }
    });
    m_tray->show();
}

void ControlWindow::showFromTray()
{
    if (m_wallFull) {
        showFullScreen();
    } else {
    showNormal();
    }
    raise();
    activateWindow();
}

void ControlWindow::quitApp()
{
    m_quit = true;
    close();
}

void ControlWindow::addIconSize(struct HICON__* icon, int side)
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

void ControlWindow::loadIcon()
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
