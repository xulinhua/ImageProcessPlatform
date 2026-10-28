/**
 * @file MainWindow.cpp
 * @brief MainWindow实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "MainWindow.h"
#include "ParaAdaptDlg.h"
#include "CompositeMgrDlg.h"
#include "CompositeMgr.h"
#include "DraftRegistry.h"
#include "AppPaths.h"
#include "SolutionDataPaths.h"
#include "QtConv.h"
#include <HsCommData/Graphic.h>
#include "ThreadPool.h"
#include "VisionCfgMgr.h"
#include "CalibMgr.h"
#include "CalibMgrDlg.h"
#include "ToolLibDlg.h"
#include "PluginHost.h"
#include "LogMgr.h"
#include "SettingsDlg.h"
#include "AuditLog.h"
#include "ChangePwdDlg.h"
#include "DesignWorkspace.h"
#include "DeviceDlg.h"
#include "DeviceMgrDlg.h"
#include "CommMgrDlg.h"
#include "JobDlg.h"
#include "LogDlg.h"
#include "UiIcons.h"
#include "PermDlg.h"
#include "RecheckWorkspace.h"
#include "RuntimeViewDlg.h"
#include "RuntimeWorkspace.h"
#include "SharedFrames.h"
#include "StationRuntimePanel.h"
#include "StationDlg.h"
#include "StationMgrDlg.h"
#include "Theme.h"
#include "Translate.h"
#include "UiText.h"
#include "UserMgrDlg.h"
#include "AboutDlg.h"
#include "VersionDlg.h"
#include "WidgetPermBinder.h"
#include <HsCommData/HsImage.h>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QImage>
#include <QVector>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <mutex>
#include <QPushButton>
#include <QStatusBar>
#include <QStackedWidget>
#include <QTimer>
#include <QToolBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdio>

namespace Vision {

namespace {

// 取当前登录名，未登录为空
std::string sessionUser(const Session* s)
{
    return (s && s->isLoggedIn()) ? s->user().loginName : std::string();
}

// 取当前角色整型键，未登录为空
std::string sessionRole(const Session* s)
{
    if (!s || !s->isLoggedIn()) {
        return std::string();
    }
    char buf[16];
    sprintf_s(buf, "%d", static_cast<int>(s->role()));
    return std::string(buf);
}

/** @brief 周期刷屏包：线程池预转换缩略图后 Queued 到 UI */
struct CycleUiPayload {
    ObjectId stationId;                 ///< 来源工站
    StationRunRst result;            ///< 最近一拍汇总
    int dTotal;                         ///< 产量增量
    int dPass;                          ///< OK 增量
    int dFail;                          ///< NG 增量
    bool addHist;                       ///< 是否追加历史
    QImage histThumb;                   ///< 预生成缩略图（异步）
    QVector<HsCommData::Graphic> overlay;  ///< 预转换叠层
    CycleUiPayload()
        : dTotal(0)
        , dPass(0)
        , dFail(0)
        , addHist(false)
    {}
};

} // namespace

// 主窗口：菜单/工具条、设计/运行切换、方案与调度
MainWindow::MainWindow(Session* session, QWidget* parent)
    : QMainWindow(parent)
    , m_session(session)
    , m_mode(ModeDesign)
    , m_station(nullptr)
    , m_boundStationId()
    , m_running(false)
    , m_residentTray(false)
    , m_serviceRun(false)
    , m_forceQuit(false)
    , m_shuttingDown(false)
    , m_previewTimer(0)
    , m_designBusy(0)
    , m_mainTb(nullptr)
{
    setMinimumSize(1280, 800);
    if (!qApp->windowIcon().isNull()) {
        setWindowIcon(qApp->windowIcon());
    }
    m_sol = Solution::makeEmpty();
    CompositeMgr::instance().load(0);
    setCompositeFinder(&CompositeMgr::findRaw);
    CompositeMgr::instance().setOpenSolution(&m_sol);

    // 顶部设计/运行页签
    QWidget* tabBar = new QWidget(this);
    QHBoxLayout* tabLay = new QHBoxLayout(tabBar);
    tabLay->setContentsMargins(8, 0, 8, 0);
    tabLay->setSpacing(0);
    m_tabDesign = new QPushButton(T("设计"), tabBar);
    m_tabRuntime = new QPushButton(T("运行"), tabBar);
    m_tabRecheck = new QPushButton(T("复检"), tabBar);
    m_tabDesign->setObjectName(QLatin1String("modeTab"));
    m_tabRuntime->setObjectName(QLatin1String("modeTab"));
    m_tabRecheck->setObjectName(QLatin1String("modeTab"));
    m_tabDesign->setIcon(uiIcon(UiIconDesign, 18));
    m_tabRuntime->setIcon(uiIcon(UiIconRuntime, 18));
    m_tabRecheck->setIcon(uiIcon(UiIconRecheck, 18));
    m_tabDesign->setIconSize(QSize(18, 18));
    m_tabRuntime->setIconSize(QSize(18, 18));
    m_tabRecheck->setIconSize(QSize(18, 18));
    m_tabDesign->setCheckable(true);
    m_tabRuntime->setCheckable(true);
    m_tabRecheck->setCheckable(true);
    m_tabDesign->setChecked(true);
    connect(m_tabDesign, SIGNAL(clicked()), this, SLOT(switchDesign()));
    connect(m_tabRuntime, SIGNAL(clicked()), this, SLOT(switchRuntime()));
    connect(m_tabRecheck, SIGNAL(clicked()), this, SLOT(switchRecheck()));
    tabLay->addWidget(m_tabDesign);
    tabLay->addWidget(m_tabRuntime);
    tabLay->addWidget(m_tabRecheck);
    tabLay->addStretch();

    m_design = new DesignWorkspace(session, &m_sol, m_station, this);
    m_runtime = new RuntimeWorkspace(this);
    m_recheck = new RecheckWorkspace(session, &m_sol, this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_design);
    m_stack->addWidget(m_runtime);
    m_stack->addWidget(m_recheck);

    QWidget* central = new QWidget(this);
    QVBoxLayout* cLay = new QVBoxLayout(central);
    cLay->setContentsMargins(0, 0, 0, 0);
    cLay->setSpacing(0);
    cLay->addWidget(tabBar);
    cLay->addWidget(m_stack, 1);
    setCentralWidget(central);

    m_clock = new QTimer(this);
    m_clock->start(1000);
    connect(m_clock, SIGNAL(timeout()), this, SLOT(onClock()));
    m_runLogHub.addSink(this);
    m_runLogHub.addSink(&m_runLogFile);
    m_runLogFile.setIdentity(sessionUser(m_session), sessionRole(m_session));
    m_session->setListener(this);
    connect(m_design, SIGNAL(stationsRequested()), this, SLOT(openStations()));
    connect(m_design, SIGNAL(stationActivated(QString)), this, SLOT(onStation(QString)));
    connect(m_design, SIGNAL(jobActivated(QString)), this, SLOT(onJob(QString)));
    connect(m_design, SIGNAL(toolActivated(QString)), this, SLOT(onTool(QString)));
    connect(m_design, SIGNAL(runThisTool()), this, SLOT(runThisTool()));
    connect(m_design, SIGNAL(runThisJob()), this, SLOT(runThisJob()));
    connect(m_design, SIGNAL(namesChanged()), this, SLOT(updateTitle()));
    connect(m_runtime, SIGNAL(startRequested()), this, SLOT(startStation()));
    connect(m_runtime, SIGNAL(softTriggerRequested()), this, SLOT(pulseSoftTrigger()));
    connect(m_runtime, SIGNAL(stopRequested()), this, SLOT(stopStation()));
    connect(m_runtime, SIGNAL(resetRequested()), this, SLOT(resetCount()));
    connect(m_runtime, SIGNAL(switchSolutionRequested()), this, SLOT(openSolution()));
    connect(m_runtime, SIGNAL(configureViewRequested()), this, SLOT(openRuntimeView()));
    connect(m_recheck, SIGNAL(prepareStation(QString)), this, SLOT(onRecheckPrepareStation(QString)));
    connect(m_recheck, SIGNAL(openDesignDebug(QString,QString,QString)),
            this, SLOT(onRecheckOpenDebug(QString,QString,QString)));

    buildMenus();

    m_mainTb = addToolBar(T("主工具条"));
    m_mainTb->setMovable(false);
    m_mainTb->setIconSize(QSize(20, 20));
    m_mainTb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_actNew = m_mainTb->addAction(uiIcon(UiIconNew), T("新建"), this, SLOT(newSolution()));
    m_tbNew = m_actNew;
    m_tbOpen = m_mainTb->addAction(uiIcon(UiIconOpen), T("打开"), this, SLOT(openSolution()));
    m_actSave = m_mainTb->addAction(uiIcon(UiIconSave), T("保存"), this, SLOT(saveSolutionFile()));
    m_tbSave = m_actSave;
    m_tbSep1 = m_mainTb->addSeparator();
    m_tbNewStation = m_mainTb->addAction(uiIcon(UiIconNewStation), T("新建工站"), this, SLOT(newStation()));
    m_tbNewJob = m_mainTb->addAction(uiIcon(UiIconNewJob), T("新建流程"), this, SLOT(newJob()));
    m_tbStart = m_mainTb->addAction(uiIcon(UiIconStart), T("开始"), this, SLOT(startStation()));
    m_tbStop = m_mainTb->addAction(uiIcon(UiIconStop), T("停止"), this, SLOT(stopStation()));
    m_tbReset = m_mainTb->addAction(uiIcon(UiIconReset), T("清零"), this, SLOT(resetCount()));

    m_solLabel = new QLabel(this);
    m_stLabel = new QLabel(this);
    m_clockLabel = new QLabel(this);
    m_userLabel = new QLabel(this);
    statusBar()->addWidget(m_solLabel);
    statusBar()->addWidget(m_stLabel);
    statusBar()->addPermanentWidget(m_clockLabel);
    statusBar()->addPermanentWidget(m_userLabel);
    onClock();

    if (!m_sol.stations.empty()) {
        m_runtime->setStationName(qs(m_sol.stations[0].name));
    }
    // 按本机设置的启动默认模式进入（无设计权限则回落运行态）
    {
        AppMode boot = ModeDesign;
        if (m_session->settings().startupMode == StartupModeRuntime) {
            boot = ModeRuntime;
        }
        if (boot == ModeDesign && !m_session->has(PermEnterDesign)) {
            boot = ModeRuntime;
        }
        applyMode(boot);
    }
    m_binder.bindAction(m_tbStart, PermRuntimeStartStop);
    m_binder.bindAction(m_tbStop, PermRuntimeStartStop);
    m_binder.bindAction(m_tbReset, PermRuntimeResetCount);
    m_binder.bindAction(m_tbNewStation, PermEditSolution);
    m_binder.bindAction(m_tbNewJob, PermEditSolution);
    m_binder.bindAction(m_actNew, PermEditSolution);
    m_binder.bindAction(m_actSave, PermEditSolution);
    m_binder.bindAction(m_actRuntimeView, PermEditSolution);
    onUserChanged();
    m_design->appendLog(T("就绪。默认文件源指向程序目录 samples；也可在设备里改文件夹。"));
    rebuildStation();
    tryLastSolution();
}

// 卸掉会话/工站监听并释放工站
MainWindow::~MainWindow()
{
    shutdownRuntime();
    if (m_session) {
        m_session->setListener(0);
    }
    if (m_station) {
        delete m_station;
        m_station = nullptr;
    }
}

// 退出前：停线程 → 排空池 → 卸监听（先于销毁方案/工站）
void MainWindow::shutdownRuntime()
{
    if (m_shuttingDown) {
        clearLiveStations();
        ThreadPool::shared().drainForShutdown();
        return;
    }
    m_shuttingDown = true;
    m_running = false;
    if (m_runtime) {
        m_runtime->setLoopRunning(false);
    }
    m_designBusy = 0;
    if (m_recheck) {
        m_recheck->abortBatch();
    }
    clearLiveStations();
    if (m_station) {
        m_station->stop();
        m_station->setListener(0);
    }
    ThreadPool::shared().drainForShutdown();
    // 吃掉已排队的 Queued 槽，避免关窗后仍回调
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    m_runLogHub.removeSink(this);
    m_runLogHub.removeSink(&m_runLogFile);
    if (m_runtime) {
        m_runtime->setLive(false);
    }
}

// 运行日志：排队到 UI；界面只显示正文，落盘仍用完整行
void MainWindow::onRunLog(const RunLogEvent& ev)
{
    if (m_shuttingDown) {
        return;
    }
    QString level = QLatin1String(RunLogEvent::levelTag(ev.level));
    if (ev.message.find("[OK]") != std::string::npos) {
        level = QLatin1String("OK");
    } else if (ev.message.find("[NG]") != std::string::npos) {
        level = QLatin1String("NG");
    } else if (ev.level == RunLogWarn) {
        level = QLatin1String("NG");
    } else if (ev.level == RunLogError) {
        level = QLatin1String("ERR");
    } else if (ev.level == RunLogInfo) {
        level = QLatin1String("OK");
    }
    const QString message = qs(ev.formatMessage());
    QMetaObject::invokeMethod(this, "onEngineRunLog", Qt::QueuedConnection,
                              Q_ARG(QString, level), Q_ARG(QString, message));
}

// 用户变化后刷新动作；无设计权限则切到运行态
// rebuild menus (ctor + locale hot-switch)
void MainWindow::buildMenus()
{
    menuBar()->clear();
    // 方案 / 工站 / 设备 / 通信 / 视图 / 运行 / 设置 / 系统 / 帮助
    QMenu* mSol = menuBar()->addMenu(T("方案"));
    mSol->addAction(T("新建"), this, SLOT(newSolution()), QKeySequence::New);
    mSol->addAction(T("打开…"), this, SLOT(openSolution()), QKeySequence::Open);
    mSol->addAction(T("保存"), this, SLOT(saveSolutionFile()), QKeySequence::Save);
    mSol->addAction(T("另存为…"), this, SLOT(saveSolutionAs()));
    mSol->addSeparator();
    mSol->addAction(T("打开图像…"), this, SLOT(openImage()));
    mSol->addSeparator();
    mSol->addAction(T("退出"), qApp, SLOT(quit()));

    QMenu* mSt = menuBar()->addMenu(T("工站"));
    mSt->addAction(T("工站管理…"), this, SLOT(openStations()));

    QMenu* mDev = menuBar()->addMenu(T("设备"));
    mDev->addAction(T("设备管理…"), this, SLOT(openDevices()));
    mDev->addAction(T("标定管理..."), this, SLOT(openCalib()));
    mDev->addAction(T("工具库管理..."), this, SLOT(openToolLib()));

    QMenu* mComm = menuBar()->addMenu(T("通信"));
    mComm->addAction(T("通信管理…"), this, SLOT(openComm()));

    QMenu* mView = menuBar()->addMenu(T("视图"));
    m_actRuntimeView = mView->addAction(T("画面配置…"), this, SLOT(openRuntimeView()));

    QMenu* mRun = menuBar()->addMenu(T("运行"));
    mRun->addAction(T("设计"), this, SLOT(switchDesign()));
    mRun->addAction(T("运行"), this, SLOT(switchRuntime()));
    mRun->addAction(T("复检"), this, SLOT(switchRecheck()));
    mRun->addSeparator();
    mRun->addAction(T("开始"), this, SLOT(startStation()));
    mRun->addAction(T("停止"), this, SLOT(stopStation()));
    mRun->addAction(T("清零"), this, SLOT(resetCount()));

    QMenu* mSet = menuBar()->addMenu(T("设置"));
    m_actSettings = mSet->addAction(T("设置…"), this, SLOT(openAppSettings()));

    QMenu* mSys = menuBar()->addMenu(T("系统"));
    m_actUsers = mSys->addAction(T("用户管理…"), this, SLOT(openUsers()));
    m_actPerms = mSys->addAction(T("控件权限…"), this, SLOT(openPerms()));
    m_actLogs = mSys->addAction(T("日志管控…"), this, SLOT(openLogs()));
    mSys->addAction(T("参数版本…"), this, SLOT(openParaAdapt()));
    mSys->addAction(T("组合工具…"), this, SLOT(openComposites()));
    mSys->addSeparator();
    mSys->addAction(T("修改口令…"), this, SLOT(changePwd()));
    mSys->addAction(T("注销"), this, SLOT(logout()));

    QMenu* mHelp = menuBar()->addMenu(T("帮助"));
    mHelp->addAction(T("版本管理…"), this, SLOT(openVersionManager()));
    mHelp->addAction(T("关于"), this, SLOT(about()));

    // 主工具条：方案、工站设备、运行控制（图标 + 文字）
}

// 刷新静态界面文案
void MainWindow::retranslateUi()
{
    m_tabDesign->setText(T("设计"));
    m_tabRuntime->setText(T("运行"));
    m_tabRecheck->setText(T("复检"));
    if (m_mainTb) {
        m_mainTb->setWindowTitle(T("主工具条"));
    }
    if (m_actNew) { m_actNew->setText(T("新建")); }
    if (m_tbOpen) { m_tbOpen->setText(T("打开")); }
    if (m_actSave) { m_actSave->setText(T("保存")); }
    if (m_tbNewStation) { m_tbNewStation->setText(T("新建工站")); }
    if (m_tbNewJob) { m_tbNewJob->setText(T("新建流程")); }
    if (m_tbStart) { m_tbStart->setText(T("开始")); }
    if (m_tbStop) { m_tbStop->setText(T("停止")); }
    if (m_tbReset) { m_tbReset->setText(T("清零")); }
    buildMenus();
    m_binder.bindAction(m_actRuntimeView, PermEditSolution);
    updateTitle();
    updateActions();
    if (m_design) { m_design->retranslateUi(); }
    if (m_runtime) { m_runtime->retranslateUi(); }
    if (m_recheck) { m_recheck->retranslateUi(); }
}

// session: language/theme applied
void MainWindow::onUiLocaleChanged()
{
    retranslateUi();
}

// 用户变化后刷新动作；无设计权限则切到运行态
void MainWindow::onUserChanged()
{
    m_runLogFile.setIdentity(sessionUser(m_session), sessionRole(m_session));
    updateActions();
    updateTitle();
    if (m_session->isLoggedIn() && !m_session->has(PermEnterDesign) && m_mode == ModeDesign) {
        applyMode(ModeRuntime);
    }
}

// 开发阶段：缺权限时自动提权为管理员并恢复出厂权限
bool MainWindow::ensurePerm(Perm perm, const QString& deniedText)
{
    if (m_session->has(perm)) {
        return true;
    }
    m_session->enterAdminForDev();
    m_session->resetDevPerms();
    VisionCfgMgr::saveSettings(m_session->settings());
    if (m_session->has(perm)) {
        updateActions();
        updateTitle();
        return true;
    }
    Q_UNUSED(deniedText);
    return true;
}

// 进入设计态所需权限
bool MainWindow::ensureDesignAccess()
{
    return ensurePerm(PermEnterDesign, T("当前用户不能进入设计态。"));
}

// 切换中央页并按模式显隐工具条按钮
void MainWindow::applyMode(AppMode mode)
{
    m_mode = mode;
    if (mode == ModeRuntime) {
        m_stack->setCurrentWidget(m_runtime);
    } else if (mode == ModeRecheck) {
        m_stack->setCurrentWidget(m_recheck);
    } else {
        m_stack->setCurrentWidget(m_design);
    }
    m_tabDesign->blockSignals(true);
    m_tabRuntime->blockSignals(true);
    m_tabRecheck->blockSignals(true);
    m_tabDesign->setChecked(mode == ModeDesign);
    m_tabRuntime->setChecked(mode == ModeRuntime);
    m_tabRecheck->setChecked(mode == ModeRecheck);
    m_tabDesign->blockSignals(false);
    m_tabRuntime->blockSignals(false);
    m_tabRecheck->blockSignals(false);
    const bool design = (mode == ModeDesign);
    m_tbNew->setVisible(design);
    m_tbOpen->setVisible(design);
    m_tbSave->setVisible(design);
    m_tbSep1->setVisible(design);
    m_tbNewStation->setVisible(design);
    m_tbNewJob->setVisible(design);
    m_tbStart->setVisible(!design);
    m_tbStop->setVisible(!design);
    m_tbReset->setVisible(!design);
    if (mode == ModeRecheck && m_recheck) {
        m_recheck->reloadCombos();
    }
    updateTitle();
}

// 切到设计态（需权限）
void MainWindow::switchDesign()
{
    if (!ensureDesignAccess()) {
        return;
    }
    applyMode(ModeDesign);
}

// 切到运行态
void MainWindow::switchRuntime()
{
    applyMode(ModeRuntime);
}

// 切到复检态（需调试运行权限）
void MainWindow::switchRecheck()
{
    if (!m_session->has(PermRunJobDebug)) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能进入复检。"));
        applyMode(m_mode);
        return;
    }
    applyMode(ModeRecheck);
}

// 复检批跑前绑定工站实例
void MainWindow::onRecheckPrepareStation(const QString& stationId)
{
    ensureStationRuntime(m_sol.findStationConfig(oid(stationId)));
    if (m_recheck) {
        m_recheck->setStation(m_station);
    }
}

// 复检跳转设计调试
void MainWindow::onRecheckOpenDebug(const QString& jobId, const QString& toolId,
                                    const QString& imagePath)
{
    ensureStationRuntime(m_sol.findStationCfgOfJob(oid(jobId)));
    if (!ensureDesignAccess()) {
        applyMode(m_mode);
        return;
    }
    applyMode(ModeDesign);
    if (m_design) {
        m_design->setStation(m_station);
        m_design->focusRecheckDebug(jobId, toolId, imagePath);
    }
}

// 刷新标题栏、状态栏与运行区工站/方案名
void MainWindow::updateTitle()
{
    QString mode = T("设计");
    if (m_mode == ModeRuntime) {
        mode = T("运行");
    } else if (m_mode == ModeRecheck) {
        mode = T("复检");
    }
    QString st = m_sol.stations.empty() ? QLatin1String("-") : qs(m_sol.stations[0].name);
    QString user = m_session->isLoggedIn()
                   ? (qs(m_session->user().displayName) + QLatin1String(" / ") + QString::fromUtf8(roleTitle(m_session->role()).c_str()))
                   : T("未登录");
    setWindowTitle(T("通用视觉平台  -  ") + mode + QLatin1String("  -  ") + qs(m_sol.name) + QLatin1String("  -  ") + st);
    m_userLabel->setText(T("用户: ") + user);
    m_solLabel->setText(T("方案: ") + qs(m_sol.name));
    m_stLabel->setText(T("工站: ") + st);
    m_runtime->setStationName(st);
    m_runtime->setSolutionName(qs(m_sol.name));
    // 只保证宿主存在，禁止每次改标题都重建面板（否则停止后画面/产量被清）
    m_runtime->ensureHost(&m_sol);
}

// 按会话权限启用菜单与工具条动作
void MainWindow::updateActions()
{
    m_tabDesign->setEnabled(true);
    const bool debug = m_session->has(PermRunJobDebug);
    m_tabRecheck->setEnabled(debug);
    m_binder.apply(*m_session);
    m_design->setDebugPerms(debug);
    if (m_recheck) {
        m_recheck->setRunPerms(debug);
    }
    m_actUsers->setEnabled(true);
    m_actPerms->setEnabled(true);
    m_actSettings->setEnabled(true);
    m_actLogs->setEnabled(true);
    m_runtime->setControlPerms(m_session->has(PermRuntimeStartStop),
                               m_session->has(PermRuntimeResetCount),
                               m_session->has(PermRuntimeSwitchSolution));
    m_runtime->setConfigureEnabled(m_session->has(PermEditSolution));
}

// 状态栏时钟
void MainWindow::onClock()
{
    m_clockLabel->setText(QDateTime::currentDateTime().toString(QLatin1String("yyyy-MM-dd hh:mm:ss")));
}

// 确认保存后换成空方案
void MainWindow::newSolution()
{
    if (!m_session->has(PermEditSolution)) {
        QMessageBox::warning(this, T("权限"), T("没有改配方权限。"));
        return;
    }
    if (!confirmSave()) {
        return;
    }
    m_sol = Solution::makeEmpty();
    applyLoadedSolution();
}

// 方案读入后刷树与运行窗
void MainWindow::applyLoadedSolution()
{
    CalibMgr::instance().load(pathDirName(m_sol.filePath), 0);
    CompositeMgr::instance().setOpenSolution(&m_sol);
    clearProdAccum();
    rebuildStation();
    m_design->reloadTree();
    if (m_recheck) {
        m_recheck->reloadCombos();
    }
    if (!m_sol.stations.empty()) {
        m_runtime->setStationName(qs(m_sol.stations[0].name));
    }
    updateTitle();
    capturePersistBase();
}

// 按主工站配置重建 Station，并下发给设计/运行区
void MainWindow::rebuildStation()
{
    ensureStationRuntime(m_sol.primaryStation());
}

// 确保设计/复检运行实例绑定到指定工站（切换工站或刷新其 Job 列表）
void MainWindow::ensureStationRuntime(StationCfg* config)
{
    // 先停运行态多工站循环并排空线程池，禁止 tick/试跑与 rebuild 并发
    const bool wasLive = isRuntimeLive();
    clearLiveStations();
    m_running = false;
    if (wasLive && m_runtime) {
        m_runtime->setLive(false);
        m_runtime->setLoopRunning(false);
    }
    ThreadPool::shared().waitIdle();

    if (!config) {
        if (m_station) {
            m_station->setListener(0);
            delete m_station;
            m_station = nullptr;
        }
        m_boundStationId.clear();
        if (m_design) {
            m_design->setStation(0);
        }
        if (m_recheck) {
            m_recheck->setStation(0);
        }
        if (m_runtime) {
            m_runtime->bindContext(&m_sol, 0, 0);
        }
        return;
    }

    const QString id = qs(config->id);
    if (m_station && m_boundStationId == id) {
        // 已停循环且池空闲：重绑指针并重建 Job（覆盖 stations 扩容与流程增删）
        m_station->rebindConfig(config);
    } else {
        if (m_station) {
            m_station->setListener(0);
            delete m_station;
            m_station = nullptr;
        }
        m_station = new Station(config);
        m_station->setListener(&m_runLogHub);
        m_boundStationId = id;
    }

    if (m_design) {
        m_design->setStation(m_station);
    }
    if (m_recheck) {
        m_recheck->setStation(m_station);
    }
    if (m_runtime) {
        m_runtime->bindContext(&m_sol, config, m_station);
    }
}

// 是否有运行态循环在跑
bool MainWindow::isRuntimeLive() const
{
    if (m_running) {
        return true;
    }
    for (size_t i = 0; i < m_live.size(); ++i) {
        if (m_live[i] && m_live[i]->loop.isRunning()) {
            return true;
        }
    }
    return false;
}

// 停止并释放全部运行态工站槽（先把产量写入累计表）
void MainWindow::clearLiveStations()
{
    // 先全部置停，避免 join 时检测线程还在往共享池塞 UI 预转换
    for (size_t i = 0; i < m_live.size(); ++i) {
        if (m_live[i]) {
            storeProdFromSlot(m_live[i]);
            m_live[i]->uiQueued.store(false);
            m_live[i]->loop.requestStop();
        }
    }
    ThreadPool::shared().clearPending();
    for (size_t i = 0; i < m_live.size(); ++i) {
        LiveStationSlot* slot = m_live[i];
        if (!slot) {
            continue;
        }
        slot->loop.stop();
        if (slot->station) {
            slot->station->stop();
            slot->station->setListener(0);
            delete slot->station;
            slot->station = nullptr;
        }
        if (slot) {
            delete slot;
            slot = nullptr;
        }
    }
    m_live.clear();
}

// 从累计表恢复槽内产量
void MainWindow::restoreProdToSlot(LiveStationSlot* slot)
{
    if (!slot) {
        return;
    }
    const QHash<QString, StationProdAccum>::const_iterator it =
        m_prodAccum.constFind(qs(slot->stationId));
    if (it == m_prodAccum.constEnd()) {
        return;
    }
    slot->total = it.value().total;
    slot->pass = it.value().pass;
    slot->fail = it.value().fail;
}

// 把槽内产量写回累计表
void MainWindow::storeProdFromSlot(const LiveStationSlot* slot)
{
    if (!slot || slot->stationId.isNull()) {
        return;
    }
    StationProdAccum& a = m_prodAccum[qs(slot->stationId)];
    a.total = slot->total;
    a.pass = slot->pass;
    a.fail = slot->fail;
}

// 清零累计产量
void MainWindow::clearProdAccum()
{
    m_prodAccum.clear();
}

// 把 live 实例重新挂到运行壳面板
void MainWindow::rebindLivePanels()
{
    if (!m_runtime) {
        return;
    }
    for (size_t i = 0; i < m_live.size(); ++i) {
        LiveStationSlot* slot = m_live[i];
        if (slot && slot->station) {
            m_runtime->setStationInstance(slot->stationId, slot->station);
        }
    }
}

// 成功打开或保存后记下路径
void MainWindow::rememberSolutionPath()
{
    if (!m_session || m_sol.filePath.empty()) {
        return;
    }
    m_session->settings().lastSolutionPath = m_sol.filePath;
    VisionCfgMgr::saveSettings(m_session->settings());
}

// 启动时尝试上次方案；失败提示，主窗照常打开
void MainWindow::tryLastSolution()
{
    const std::string p = m_session->settings().lastSolutionPath;
    if (p.empty()) {
        capturePersistBase();
        return;
    }
    std::string err;
    if (VisionCfgMgr::loadSolution(p, &m_sol, &err)) {
        applyLoadedSolution();
        return;
    }
    QMessageBox box(this);
    box.setWindowTitle(T("上次方案"));
    box.setText(T("无法打开上次方案：") + qs(p) + QLatin1String("\n") + qs(err)
                + QLatin1String("\n") + T("软件将继续打开。"));
    QPushButton* blank = box.addButton(T("空白方案"), QMessageBox::AcceptRole);
    QPushButton* pick = box.addButton(T("另选文件"), QMessageBox::ActionRole);
    box.addButton(T("忽略"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == pick) {
        const QString np = QFileDialog::getOpenFileName(this, T("打开方案"), qs(AppPaths::solutionsDir()),
                                                       T("方案 (*.vpsol);;所有文件 (*.*)"));
        if (!np.isEmpty()) {
            std::string e2;
            if (VisionCfgMgr::loadSolution(ss(np), &m_sol, &e2)) {
                rememberSolutionPath();
                applyLoadedSolution();
                return;
            }
            QMessageBox::warning(this, T("打开失败"), qs(e2));
        }
    }
    m_sol = Solution::makeEmpty();
    if (box.clickedButton() == blank) {
        m_session->settings().lastSolutionPath.clear();
        VisionCfgMgr::saveSettings(m_session->settings());
    }
    applyLoadedSolution();
}

// 打开方案文件并刷新设计树
void MainWindow::openSolution()
{
    if (!m_session->has(PermRuntimeSwitchSolution) && m_mode == ModeRuntime) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能切换方案。"));
        return;
    }
    if (!confirmSave()) {
        return;
    }
    const QString p = QFileDialog::getOpenFileName(this, T("打开方案"), qs(AppPaths::solutionsDir()),
                                                   T("方案 (*.vpsol);;所有文件 (*.*)"));
    if (p.isEmpty()) {
        return;
    }
    std::string err;
    if (!VisionCfgMgr::loadSolution(ss(p), &m_sol, &err)) {
        QMessageBox::warning(this, T("打开失败"), qs(err));
        return;
    }
    auditLog("load_solution", m_session->user().loginName, ss(p));
    rememberSolutionPath();
    applyLoadedSolution();
}

// 记下当前可存盘内容，作为未改动基线
void MainWindow::capturePersistBase()
{
    m_persistBase = VisionCfgMgr::persistFingerprint(m_sol);
    m_sol.dirty = false;
}

// 用可存盘内容对照基线，刷新 dirty
void MainWindow::syncDirty()
{
    m_sol.dirty = VisionCfgMgr::persistFingerprint(m_sol) != m_persistBase;
}

// 记录保存者与时间，清除脏标记并写审计
void MainWindow::markSaved()
{
    m_sol.lastSavedById = m_session->user().id;
    m_sol.lastSavedByName = m_session->user().displayName;
    m_sol.lastSavedAt = ss(QDateTime::currentDateTime().toString(Qt::ISODate));
    m_sol.dirty = false;
    auditLog("save_solution", m_session->user().loginName, m_sol.filePath);
}

// 保存到已有路径，无路径则另存
void MainWindow::saveSolutionFile()
{
    if (!m_session->has(PermEditSolution)) {
        QMessageBox::warning(this, T("权限"), T("没有保存配方权限。"));
        return;
    }
    if (m_sol.filePath.empty()) {
        saveSolutionAs();
        return;
    }
    std::string err;
    markSaved();
    if (!VisionCfgMgr::saveSolution(m_sol, &err)) {
        QMessageBox::warning(this, T("保存失败"), qs(err));
        syncDirty();
        return;
    }
    CalibMgr::instance().setSolutionDir(pathDirName(m_sol.filePath));
    std::string calibErr;
    if (!CalibMgr::instance().save(&calibErr)) {
        m_design->appendLog(qs(calibErr));
    }
    rememberSolutionPath();
    capturePersistBase();
    m_design->appendLog(T("已保存 ") + qs(m_sol.filePath));
}

// 另选路径后保存，并尽量拷贝旧 vpdata
void MainWindow::saveSolutionAs()
{
    if (!m_session->has(PermEditSolution)) {
        return;
    }
    const QString p = QFileDialog::getSaveFileName(this, T("另存方案"), qs(AppPaths::solutionsDir()) + QLatin1String("/solution.vpsol"),
                                                   T("方案 (*.vpsol)"));
    if (p.isEmpty()) {
        return;
    }
    const std::string oldPath = m_sol.filePath;
    const std::string oldData = solutionDataRootFromFile(oldPath);
    m_sol.filePath = ss(p);
    saveSolutionFile();
    if (m_sol.dirty) {
        return;
    }
    const std::string newData = solutionDataRootFromFile(m_sol.filePath);
    if (!oldData.empty() && !newData.empty() && oldData != newData) {
        if (!copyDirectoryRecursive(oldData, newData)) {
            m_design->appendLog(T("警告：工具数据目录拷贝未完全成功"));
        }
    }
}

// 脏方案询问是否保存，取消则中止后续操作
bool MainWindow::confirmSave()
{
    syncDirty();
    if (!m_sol.dirty) {
        return true;
    }
    const int r = QMessageBox::question(this, T("未保存"), T("方案已修改，是否保存？"),
                                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (r == QMessageBox::Cancel) {
        return false;
    }
    if (r == QMessageBox::Yes) {
        saveSolutionFile();
        return !m_sol.dirty || !m_sol.filePath.empty();
    }
    return true;
}

// 打开图像到当前模式的图像窗
void MainWindow::openImage()
{
    const QString p = QFileDialog::getOpenFileName(this, T("打开图像"), QString(),
                                                   T("图像 (*.png *.bmp *.jpg *.jpeg *.tif);;所有文件 (*.*)"));
    if (p.isEmpty()) {
        return;
    }
    QImage img(p);
    if (img.isNull()) {
        QMessageBox::warning(this, T("图像"), T("无法打开。"));
        return;
    }
    HsCtrl::HsImageWindow* w = (m_mode == ModeRuntime) ? m_runtime->imageWindow() : m_design->imageWindow();
    applyFrame(w, fromQImage(img, 1));
    w->fitToWindow();
    m_design->appendLog(T("打开图像 ") + p);
}

// 设计态快捷新建工站
void MainWindow::newStation()
{
    if (!ensureDesignAccess()) {
        return;
    }
    if (!m_session->has(PermEditSolution)) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能编辑方案。"));
        return;
    }
    StationCfg st;
    st.id = ObjectId::create();
    st.name = std::string(Translate::tr("新工站"));
    char codeBuf[32];
    sprintf_s(codeBuf, "ST-%02d", static_cast<int>(m_sol.stations.size()) + 1);
    st.stationCode = std::string(codeBuf);
    m_sol.stations.push_back(st);
    m_sol.dirty = true;
    ensureStationRuntime(&m_sol.stations.back());
    if (m_design) {
        m_design->reloadTree();
    }
    updateTitle();
    if (m_design) {
        m_design->appendLog(T("已新建工站 ") + qs(st.name));
    }
}

// 设计态快捷新建流程（挂到当前工站）
void MainWindow::newJob()
{
    if (!ensureDesignAccess()) {
        return;
    }
    if (!m_session->has(PermEditSolution)) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能编辑方案。"));
        return;
    }
    QString sid;
    if (m_design) {
        sid = m_design->currentStationId();
    }
    StationCfg* st = m_sol.findStationConfig(oid(sid));
    if (!st) {
        st = m_sol.primaryStation();
    }
    if (!st) {
        QMessageBox::information(this, T("新建流程"), T("请先新建工站。"));
        return;
    }
    JobCfg j;
    j.id = ObjectId::create();
    j.order = static_cast<int>(st->jobs.size()) + 1;
    char jobBuf[32];
    sprintf_s(jobBuf, "JobCfg%d", j.order);
    j.name = std::string(jobBuf);
    j.root.id = ObjectId::create();
    j.execMode = ExecActive;
    j.root.kind = GroupSequence;
    st->jobs.push_back(j);
    m_sol.dirty = true;
    ensureStationRuntime(st);
    if (m_design) {
        m_design->reloadTree();
    }
    updateTitle();
    if (m_design) {
        m_design->appendLog(T("已新建流程 ") + qs(j.name));
    }
}

// 打开工站管理对话框
void MainWindow::openStations()
{
    if (m_mode == ModeDesign && !ensureDesignAccess()) {
        return;
    }
    StationMgrDlg dlg(m_session, &m_sol, this);
    connect(&dlg, SIGNAL(openStationRequested(QString)), this, SLOT(onStation(QString)));
    connect(&dlg, SIGNAL(openJobRequested(QString)), this, SLOT(onJob(QString)));
    dlg.exec();
    // 优先保持设计区当前流程所属工站，否则回落主工站
    StationCfg* prefer = nullptr;
    if (m_design && !m_design->currentJobId().isEmpty()) {
        prefer = m_sol.findStationCfgOfJob(oid(m_design->currentJobId()));
    }
    if (!prefer) {
        prefer = m_sol.primaryStation();
    }
    ensureStationRuntime(prefer);
    m_design->reloadTree();
    updateTitle();
}

// 打开设备管理对话框

// 打开标定管理

// 打开工具库管理
void MainWindow::openToolLib()
{
    ToolLibDlg dlg(this);
    dlg.exec();
    if (m_design) {
        m_design->reloadToolLib();
    }
}
void MainWindow::openCalib()
{
    if (!ensurePerm(PermEditSolution, T("当前用户不能编辑标定。"))) {
        return;
    }
    CalibMgr::instance().setSolutionDir(pathDirName(m_sol.filePath));
    CalibMgrDlg dlg(&m_sol, this);
    dlg.exec();
}
void MainWindow::openDevices()
{
    if (m_mode == ModeDesign && !ensureDesignAccess()) {
        return;
    }
    DeviceMgrDlg dlg(m_session, &m_sol, this);
    connect(&dlg, SIGNAL(openDeviceRequested(QString)), this, SLOT(onDevice(QString)));
    dlg.exec();
    m_design->reloadTree();
}

// 打开通信管理对话框
void MainWindow::openComm()
{
    if (m_mode == ModeDesign && !ensureDesignAccess()) {
        return;
    }
    CommMgrDlg dlg(m_session, &m_sol, this);
    dlg.exec();
}

// 打开工站配置
void MainWindow::onStation(const QString& id)
{
    openStationDlg(id);
}

// 打开设备配置
void MainWindow::onDevice(const QString& id)
{
    openDeviceDlg(id);
}

// 打开流程配置
void MainWindow::onJob(const QString& id)
{
    showJobDlg(id);
}

// 选中工具时若有缓存结果则显示
void MainWindow::onTool(const QString&)
{
    m_design->syncToolView(true);
}

// 弹出工站配置对话框
void MainWindow::openStationDlg(const QString& id)
{
    StationDlg dlg(m_session, &m_sol, id, this);
    connect(&dlg, SIGNAL(openJobRequested(QString)), this, SLOT(onJob(QString)));
    dlg.exec();
    ensureStationRuntime(m_sol.findStationConfig(oid(id)));
    m_design->reloadTree();
    updateTitle();
}

// 弹出设备配置对话框
void MainWindow::openDeviceDlg(const QString& id)
{
    DeviceDlg dlg(m_session, &m_sol, id, this);
    dlg.exec();
    m_design->reloadTree();
}

// 弹出流程配置对话框
void MainWindow::showJobDlg(const QString& jobId)
{
    JobCfg* config = m_sol.findJobConfig(oid(jobId));
    if (!config) {
        return;
    }
    ensureStationRuntime(m_sol.findStationCfgOfJob(oid(jobId)));
    Job* job = m_station ? m_station->findJob(oid(jobId)) : nullptr;
    Job tempJob(config);
    if (!job) {
        job = &tempJob;
    }
    JobDlg dlg(m_session, &m_sol, job, jobId, this);
    dlg.exec();
    ensureStationRuntime(m_sol.findStationCfgOfJob(oid(jobId)));
    m_design->reloadTree();
}

// 用户管理（需管理用户权限）
void MainWindow::openUsers()
{
    if (!ensurePerm(PermManageUsers, T("仅有用户管理权限的账户可打开。"))) {
        return;
    }
    UserMgrDlg dlg(m_session, this);
    dlg.exec();
    onUserChanged();
}

// 控件权限（需系统设置权限）
void MainWindow::openPerms()
{
    if (!ensurePerm(PermEditAppSettings, T("仅管理员可改控件权限。"))) {
        return;
    }
    PermDlg dlg(m_session, this);
    dlg.exec();
    onUserChanged();
}

// 打开本机设置中心（无编辑权也可查看路径/关于）
void MainWindow::openAppSettings()
{
    SettingsDlg dlg(m_session, this);
    dlg.exec();
}

// 配置运行主界面：方案级多工站布局 + 各工站窗格
void MainWindow::openRuntimeView()
{
    if (!ensurePerm(PermEditSolution, T("当前用户不能改运行画面。"))) {
        return;
    }
    StationCfg* st = m_sol.primaryStation();
    if (!st) {
        QMessageBox::warning(this, T("画面配置"), T("请先添加工站。"));
        return;
    }
    if (m_station && !m_boundStationId.isEmpty()) {
        StationCfg* cur = m_sol.findStationConfig(oid(m_boundStationId));
        if (cur) {
            st = cur;
        }
    }
    RuntimeViewDlg dlg(&m_sol, st, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    syncDirty();
    m_runtime->bindContext(&m_sol, st, m_station);
    rebindLivePanels();
    // 画面配置会重建面板：把累计产量刷回
    for (size_t i = 0; i < m_sol.stations.size(); ++i) {
        const ObjectId id = m_sol.stations[i].id;
        StationRuntimePanel* panel = m_runtime->panelFor(id);
        if (!panel) {
            continue;
        }
        const StationProdAccum a = m_prodAccum.value(qs(id));
        panel->setCounts(a.total, a.pass, a.fail, a.cycleMs);
    }
}

// 日志管控：查看需 ViewLogs，删除在对话框内再判 ManageLogs
void MainWindow::openLogs()
{
    if (!ensurePerm(PermViewLogs, T("当前用户不能查看日志。"))) {
        return;
    }
    LogDlg dlg(m_session, this);
    dlg.exec();
}

// 当前用户改口令
void MainWindow::changePwd()
{
    if (!m_session->isLoggedIn()) {
        return;
    }
    ChangePwdDlg dlg(m_session, qs(m_session->user().loginName), true, this);
    dlg.exec();
}

// 注销后开发阶段直接回到管理员
void MainWindow::logout()
{
    m_session->logout();
    m_session->enterAdminForDev();
    applyMode(m_session->has(PermEnterDesign) ? ModeDesign : ModeRuntime);
}

// 关于对话框
void MainWindow::about()
{
    AboutDlg dlg(this);
    dlg.exec();
}

// 打开版本信息
void MainWindow::openVersionManager()
{
    VersionDlg dlg(this);
    dlg.exec();
}

void MainWindow::openParaAdapt()
{
    ParaAdaptDlg dlg(&m_sol, this);
    dlg.exec();
}

void MainWindow::openComposites()
{
    CompositeMgr::instance().setOpenSolution(&m_sol);
    CompositeMgrDlg dlg(&m_sol, this);
    dlg.exec();
    m_design->reloadTree();
    m_design->reloadToolLib();
}

// 把工具输出送到设计窗，可选同步运行窗
void MainWindow::applyToolOut(const QString& toolId, const ToolOut& out, bool toRuntime)
{
    m_design->showToolResult(toolId, out.image, qg(out.objects), &out);
    if (toRuntime) {
        m_runtime->setRunImages(out.source, out.image, qg(out.objects));
    }
}

// 调试运行当前选中工具（线程池，不堵 UI）
void MainWindow::runThisTool()
{
    if (!m_session->has(PermRunJobDebug)) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能调试运行。"));
        return;
    }
    if (m_running || isRuntimeLive()) {
        QMessageBox::warning(this, T("运行"), T("工站循环中，请先停止再调试。"));
        return;
    }
    const QString id = m_design->currentToolId();
    if (id.isEmpty()) {
        QMessageBox::information(this, T("运行"), T("请先在资源树选中一个工具。"));
        return;
    }
    if (DraftRegistry::toolDraftBlocksFormalRun(oid(id))) {
        QMessageBox::warning(this, T("运行"),
                             T("该工具有未保存的参数修改，请先在调试窗口中保存后再运行。"));
        return;
    }
    StationCfg* st = nullptr;
    m_sol.findJobCfgOfTool(oid(id), &st);
    ensureStationRuntime(st);
    Job* job = m_station ? m_station->findJobOfTool(oid(id)) : nullptr;
    if (!job) {
        QMessageBox::warning(this, T("运行"), T("找不到该工具所属流程。"));
        return;
    }
    if (job->isRunning()) {
        QMessageBox::information(this, T("运行"), T("该流程正在执行，请稍候。"));
        return;
    }
    const ObjectId toolOid = oid(id);
    Solution* sol = &m_sol;
    ++m_designBusy;
    ThreadPool::shared().enqueue([this, job, sol, id, toolOid]() {
        const ToolOut out = job->runToolChain(sol, toolOid);
        ToolOut* heap = new ToolOut(out);
        QMetaObject::invokeMethod(this, "onDesignToolFinished", Qt::QueuedConnection,
                                  Q_ARG(QString, id), Q_ARG(void*, heap));
    });
}

// 调试运行当前流程（只读 formal，阻止未保存 draft）
void MainWindow::runThisJob()
{
    if (!m_session->has(PermRunJobDebug)) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能调试运行。"));
        return;
    }
    if (m_running || isRuntimeLive()) {
        QMessageBox::warning(this, T("运行"), T("工站循环中，请先停止再调试。"));
        return;
    }
    QString id = m_design->currentJobId();
    if (id.isEmpty() && m_sol.primaryStation() && !m_sol.primaryStation()->jobs.empty()) {
        id = qs(m_sol.primaryStation()->jobs[0].id);
    }
    if (!id.isEmpty() && DraftRegistry::anyToolInJobBlocked(&m_sol, oid(id))) {
        QMessageBox::warning(this, T("运行"),
                             T("该流程或其工具有未保存的修改，请先在流程/工站配置中保存后再运行。"));
        return;
    }
    runJobById(id);
}

// 按 ID 异步运行流程
void MainWindow::runJobById(const QString& jobId)
{
    if (jobId.isEmpty()) {
        QMessageBox::information(this, T("运行"), T("没有可运行的流程。"));
        return;
    }
    if (m_running || isRuntimeLive()) {
        QMessageBox::warning(this, T("运行"), T("工站循环中，请先停止再调试。"));
        return;
    }
    ensureStationRuntime(m_sol.findStationCfgOfJob(oid(jobId)));
    Job* job = m_station ? m_station->findJob(oid(jobId)) : nullptr;
    if (!job) {
        QMessageBox::warning(this, T("运行"), T("找不到该流程运行实例。"));
        return;
    }
    if (job->isRunning()) {
        QMessageBox::information(this, T("运行"), T("该流程正在执行，请稍候。"));
        return;
    }
    Solution* sol = &m_sol;
    ++m_designBusy;
    ThreadPool::shared().enqueue([this, job, sol, jobId]() {
        const ToolOut out = job->run(sol);
        ToolOut* heap = new ToolOut(out);
        QMetaObject::invokeMethod(this, "onDesignJobFinished", Qt::QueuedConnection,
                                  Q_ARG(QString, jobId), Q_ARG(void*, heap));
    });
}

// 设计态工具跑完
void MainWindow::onDesignToolFinished(const QString& toolId, void* outPtr)
{
    ToolOut* out = static_cast<ToolOut*>(outPtr);
    if (m_designBusy > 0) {
        --m_designBusy;
    }
    if (m_shuttingDown) {
        if (out) {
            delete out;
            out = nullptr;
        }
        return;
    }
    if (!out) {
        return;
    }
    LogMgr::appendDebug(sessionUser(m_session), sessionRole(m_session),
                           "debug_tool", ss(toolId), out->message, out->ok ? "ok" : "fail");
    applyToolOut(toolId, *out, false);
    if (out) {
        delete out;
        out = nullptr;
    }
}

// 设计态流程跑完
void MainWindow::onDesignJobFinished(const QString& jobId, void* outPtr)
{
    ToolOut* out = static_cast<ToolOut*>(outPtr);
    if (m_designBusy > 0) {
        --m_designBusy;
    }
    if (m_shuttingDown) {
        if (out) {
            delete out;
            out = nullptr;
        }
        return;
    }
    if (!out) {
        return;
    }
    LogMgr::appendDebug(sessionUser(m_session), sessionRole(m_session),
                           "debug_job", ss(jobId), out->message, out->ok ? "ok" : "fail");
    m_design->syncAfterJobRun(*out);
    m_runtime->setRunImages(out->source, out->image, qg(out->objects));
    if (out) {
        delete out;
        out = nullptr;
    }
}

// 引擎日志写入设计区
void MainWindow::onEngineLog(const QString& line)
{
    if (m_shuttingDown || !m_design) {
        return;
    }
    m_design->appendLog(line);
}

// 运行日志：级别与消息分列
void MainWindow::onEngineRunLog(const QString& level, const QString& message)
{
    if (m_shuttingDown || !m_design) {
        return;
    }
    m_design->appendLog(level, message);
}

// 工站循环线程 → UI（user 为 LiveStationSlot*）
void MainWindow::stationCycleThunk(void* user, const StationRunRst& result)
{
    LiveStationSlot* slot = static_cast<LiveStationSlot*>(user);
    if (!slot || !slot->window || slot->window->m_shuttingDown || !slot->window->m_running) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(slot->uiMu);
        slot->pendingResult = result;
        ++slot->pendingTotal;
        if (result.ok) {
            ++slot->pendingPass;
        } else {
            ++slot->pendingFail;
        }
    }
    // 合并刷屏；预转换在本工站线程做，不占检测用共享线程池（避免停机 join 死锁）
    if (!slot->uiQueued.exchange(true)) {
        MainWindow* w = slot->window;
        CycleUiPayload* heap = new CycleUiPayload();
        heap->stationId = slot->stationId;
        {
            std::lock_guard<std::mutex> lock(slot->uiMu);
            heap->result = slot->pendingResult;
            heap->dTotal = slot->pendingTotal;
            heap->dPass = slot->pendingPass;
            heap->dFail = slot->pendingFail;
            slot->pendingTotal = 0;
            slot->pendingPass = 0;
            slot->pendingFail = 0;
        }
        if (!w->m_running || w->m_shuttingDown) {
            slot->uiQueued.store(false);
            if (heap) {
                delete heap;
                heap = nullptr;
            }
            return;
        }
        const long long nowMs = static_cast<long long>(QDateTime::currentMSecsSinceEpoch());
        heap->addHist = (!heap->result.ok || nowMs - slot->lastHistMs >= 500);
        if (heap->addHist && !heap->result.image.isNull()) {
            heap->histThumb = toQImage(heap->result.image);
        }
        heap->overlay = qg(heap->result.objects);
        QMetaObject::invokeMethod(w, "onStationCycleFinished", Qt::QueuedConnection,
                                  Q_ARG(void*, heap));
    }
}
// 启动各工站循环并进入运行页
void MainWindow::startStation()
{
    if (!m_session->has(PermRuntimeStartStop)) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能开始/停止。"));
        return;
    }
    if (isRuntimeLive()) {
        return;
    }
    if (m_serviceRun) {
        SharedFrames::instance().writeCommand(IpcCmdStartOwned, 0);
        applyMode(ModeRuntime);
        m_running = true;
        if (m_runtime) {
            m_runtime->setLive(true);
            m_runtime->setLoopRunning(true);
        }
        return;
    }
    if (m_designBusy > 0) {
        QMessageBox::warning(this, T("运行"), T("设计调试仍在执行，请稍候。"));
        return;
    }
    if (m_sol.stations.empty()) {
        QMessageBox::warning(this, T("运行"), T("方案里没有工站。"));
        return;
    }
    clearLiveStations();
    ThreadPool::shared().waitIdle();
    applyMode(ModeRuntime);
    // 不重建运行面板，避免停止后再开清空画面与产量
    m_runtime->ensureHost(&m_sol);

    for (size_t i = 0; i < m_sol.stations.size(); ++i) {
        StationCfg* config = &m_sol.stations[i];
        LiveStationSlot* slot = new LiveStationSlot();
        slot->window = this;
        slot->stationId = config->id;
        restoreProdToSlot(slot);
        slot->station = new Station(config);
        slot->station->setListener(&m_runLogHub);
        slot->station->start();
        m_runtime->setStationInstance(config->id, slot->station);
        StationRuntimePanel* panel = m_runtime->panelFor(config->id);
        if (panel) {
            const StationProdAccum& a = m_prodAccum.value(qs(config->id));
            panel->setCounts(slot->total, slot->pass, slot->fail, a.cycleMs);
        }
        m_live.push_back(slot);
        slot->loop.start(slot->station, &m_sol, &ThreadPool::shared(),
                         &MainWindow::stationCycleThunk, slot);
    }
    m_running = true;
    auditLog("start_station", m_session->user().loginName, std::string("multi"));
    m_runtime->setLive(true);
    m_runtime->setLoopRunning(true);
}

// 停止全部工站循环（保留画面与产量，清零才复位）
void MainWindow::stopStation()
{
    if (!m_session->has(PermRuntimeStartStop)) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能开始/停止。"));
        return;
    }
    if (m_serviceRun) {
        SharedFrames::instance().writeCommand(IpcCmdStopOwned, 0);
        m_running = false;
        if (m_runtime) {
            m_runtime->setLive(false);
            m_runtime->setLoopRunning(false);
        }
        return;
    }
    const bool was = isRuntimeLive();
    m_running = false;
    if (m_runtime) {
        m_runtime->setLive(false);
        m_runtime->setLoopRunning(false);
    }
    // 立刻打断循环并丢掉排队中的 UI 预转换，再 join（产量写入累计表）
    for (size_t i = 0; i < m_live.size(); ++i) {
        if (m_live[i]) {
            m_live[i]->loop.requestStop();
        }
    }
    ThreadPool::shared().clearPending();
    clearLiveStations();
    if (m_runtime) {
        m_runtime->detachAllStations();
        m_runtime->setLive(false);
    }
    if (was) {
        auditLog("stop_station", sessionUser(m_session), std::string());
        m_design->appendLog(T("停止。"));
    }
}

// 软件触发：唤醒所有等触发的工站循环（各跑一轮）
void MainWindow::pulseSoftTrigger()
{
    if (!isRuntimeLive()) {
        return;
    }
    for (size_t i = 0; i < m_live.size(); ++i) {
        if (m_live[i]) {
            m_live[i]->loop.pulse();
        }
    }
}

// 运行态周期结果上屏（按工站分发）
void MainWindow::onStationCycleFinished(void* resultPtr)
{
    CycleUiPayload* payload = static_cast<CycleUiPayload*>(resultPtr);
    if (!payload) {
        return;
    }
    if (m_shuttingDown || !m_running || !isRuntimeLive()) {
        for (size_t i = 0; i < m_live.size(); ++i) {
            if (m_live[i] && m_live[i]->stationId == payload->stationId) {
                m_live[i]->uiQueued.store(false);
                break;
            }
        }
        if (payload) {
            delete payload;
            payload = nullptr;
        }
        return;
    }

    LiveStationSlot* slot = nullptr;
    for (size_t i = 0; i < m_live.size(); ++i) {
        if (m_live[i] && m_live[i]->stationId == payload->stationId) {
            slot = m_live[i];
            break;
        }
    }
    if (!slot) {
        if (payload) {
            delete payload;
            payload = nullptr;
        }
        return;
    }

    slot->total += payload->dTotal;
    slot->pass += payload->dPass;
    slot->fail += payload->dFail;

    StationRuntimePanel* panel = m_runtime ? m_runtime->panelFor(slot->stationId) : nullptr;
    if (panel) {
        if (payload->addHist) {
            panel->addResult(payload->result.ok,
                             QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss"))
                             + QLatin1Char(' ') + qs(payload->result.summary),
                             payload->histThumb);
            slot->lastHistMs = static_cast<long long>(QDateTime::currentMSecsSinceEpoch());
        }
        panel->setCounts(slot->total, slot->pass, slot->fail, payload->result.cycleMs);
        panel->setLastOk(payload->result.ok);
        panel->setRunImages(payload->result.source, payload->result.image, payload->overlay);
    }
    storeProdFromSlot(slot);
    {
        StationProdAccum& a = m_prodAccum[qs(slot->stationId)];
        a.cycleMs = payload->result.cycleMs;
    }

    if (payload) {
        delete payload;
        payload = nullptr;
    }

    bool again = false;
    {
        std::lock_guard<std::mutex> lock(slot->uiMu);
        again = (slot->pendingTotal > 0);
        if (!again) {
            slot->uiQueued.store(false);
        }
    }
    // 用 singleShot 让出事件循环，保证「停止」点击能插队处理
    if (again && m_running) {
        const ObjectId sid = slot->stationId;
        QTimer::singleShot(0, this, [this, sid]() {
            if (!m_running || m_shuttingDown) {
                for (size_t i = 0; i < m_live.size(); ++i) {
                    if (m_live[i] && m_live[i]->stationId == sid) {
                        m_live[i]->uiQueued.store(false);
                        break;
                    }
                }
                return;
            }
            LiveStationSlot* sl = nullptr;
            for (size_t i = 0; i < m_live.size(); ++i) {
                if (m_live[i] && m_live[i]->stationId == sid) {
                    sl = m_live[i];
                    break;
                }
            }
            if (!sl) {
                return;
            }
            CycleUiPayload* heap = new CycleUiPayload();
            heap->stationId = sid;
            {
                std::lock_guard<std::mutex> lock(sl->uiMu);
                heap->result = sl->pendingResult;
                heap->dTotal = sl->pendingTotal;
                heap->dPass = sl->pendingPass;
                heap->dFail = sl->pendingFail;
                sl->pendingTotal = 0;
                sl->pendingPass = 0;
                sl->pendingFail = 0;
            }
            const long long nowMs = static_cast<long long>(QDateTime::currentMSecsSinceEpoch());
            heap->addHist = (!heap->result.ok || nowMs - sl->lastHistMs >= 500);
            if (heap->addHist && !heap->result.image.isNull()) {
                heap->histThumb = toQImage(heap->result.image);
            }
            heap->overlay = qg(heap->result.objects);
            onStationCycleFinished(heap);
        });
    }
}
// 清零产量与运行历史
void MainWindow::resetCount()
{
    if (!m_session->has(PermRuntimeResetCount)) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能清零。"));
        return;
    }
    for (size_t i = 0; i < m_live.size(); ++i) {
        if (m_live[i]) {
            m_live[i]->total = m_live[i]->pass = m_live[i]->fail = 0;
        }
    }
    clearProdAccum();
    if (m_runtime) {
        m_runtime->resetHistory();
    }
    auditLog("reset_count", m_session->user().loginName, std::string());
}

void MainWindow::setResidentTray(bool on)
{
    m_residentTray = on;
}

void MainWindow::setServiceRun(bool on)
{
    m_serviceRun = on;
    if (on && !m_previewTimer) {
        m_previewTimer = new QTimer(this);
        connect(m_previewTimer, SIGNAL(timeout()), this, SLOT(pullSharedPreview()));
        m_previewTimer->start(66);
    }
}

void MainWindow::setForceQuit(bool on)
{
    m_forceQuit = on;
}

void MainWindow::showRuntimePage()
{
    applyMode(ModeRuntime);
}

void MainWindow::pullSharedPreview()
{
    if (!m_serviceRun || !m_runtime) {
        return;
    }
    SharedFrames& frames = SharedFrames::instance();
    if (!isVisible()) {
        frames.setSubscribe(false);
        return;
    }
    frames.setSubscribe(true);
    const QList<StationRuntimePanel*> panels = m_runtime->panels();
    const int localN = frames.localCount();
    const int remoteN = frames.remoteCount();
    for (int i = 0; i < panels.size(); ++i) {
        StationRuntimePanel* panel = panels.at(i);
        if (!panel || panel->stationId().isNull()) {
            continue;
        }
        const std::string sid = panel->stationId().toString();
        IpcPreview preview;
        bool got = false;
        for (int k = 0; k < localN; ++k) {
            IpcPreview one;
            if (frames.readLocal(k, &one) && one.stationId == sid) {
                preview = one;
                got = true;
            }
        }
        if (!got) {
            for (int k = 0; k < remoteN; ++k) {
                IpcPreview one;
                if (frames.readRemote(k, &one) && one.stationId == sid) {
                    preview = one;
                    got = true;
                }
            }
        }
        if (!got || preview.jpeg.empty()) {
            continue;
        }
        QImage img;
        if (!img.loadFromData(preview.jpeg.data(), static_cast<int>(preview.jpeg.size()), "JPEG")) {
            continue;
        }
        img = img.convertToFormat(QImage::Format_RGB888);
        const HsCommData::HsImage hs = HsCommData::HsImage::fromRaw(
            img.constBits(), img.width(), img.height(), img.bytesPerLine(), "rgb8", 0);
        QVector<HsCommData::Graphic> overlay;
        panel->setRunImages(hs, hs, overlay);
        panel->setCounts(preview.total, preview.pass, preview.fail, preview.cycleMs);
        panel->setLastOk(preview.ok);
    }
}

// 关闭前：托盘模式只藏窗口。真正退出才停线程并询问保存
void MainWindow::closeEvent(QCloseEvent* e)
{
    if (m_residentTray && !m_forceQuit) {
        SharedFrames::instance().setSubscribe(false);
        hide();
        e->ignore();
        return;
    }
    shutdownRuntime();
    if (!confirmSave()) {
        // 用户取消退出：恢复接受新任务（工站循环需手动再开）
        m_shuttingDown = false;
        ThreadPool::shared().setAccepting(true);
        m_runLogHub.addSink(this);
        m_runLogHub.addSink(&m_runLogFile);
        if (m_station) {
            m_station->setListener(&m_runLogHub);
        }
        e->ignore();
        return;
    }
    e->accept();
}

}

#include "moc_MainWindow.cpp"
