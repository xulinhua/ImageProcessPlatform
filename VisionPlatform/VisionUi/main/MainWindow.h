/**
 * @file MainWindow.h
 * @brief MainWindow头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_MAINWINDOW_H
#define VISION_MAINWINDOW_H
#include "VisionUiExport.h"

#include "Solution.h"
#include "Station.h"
#include "StationLoop.h"
#include "Session.h"
#include "RunLogHub.h"
#include "VisionLogRunSink.h"
#include "WidgetPermBinder.h"
#include <QHash>
#include <QMainWindow>
#include <atomic>
#include <mutex>
#include <vector>

class QLabel;
class QStackedWidget;
class QTimer;
class QPushButton;
class QAction;
class QToolBar;

namespace Vision {

class DesignWorkspace;
class RuntimeWorkspace;
class RecheckWorkspace;
class MainWindow;

/**
 * @class LiveStationSlot
 * @brief 运行态一个工站的实例、循环线程与产量计数
 */
struct LiveStationSlot {
    MainWindow* window;   ///< 回投 UI 的主窗
    ObjectId stationId;   ///< 工站 id
    Station* station;     ///< 运行实例（拥有）
    StationLoop loop;     ///< 专属循环
    int total;            ///< 产量
    int pass;             ///< OK
    int fail;             ///< NG
    std::mutex uiMu;      ///< 保护待刷结果与待加计数
    StationRunRst pendingResult; ///< 最近一拍（合并刷新）
    int pendingTotal;     ///< 待并入 UI 的产量增量
    int pendingPass;      ///< 待并入 OK
    int pendingFail;      ///< 待并入 NG
    std::atomic<bool> uiQueued; ///< 是否已有 Queued 刷屏任务
    long long lastHistMs;    ///< 上次追加历史的时刻（节流）
    /** @brief 计数清零，未绑定主窗和工站 */
    LiveStationSlot()
        : window(nullptr)
        , station(nullptr)
        , total(0)
        , pass(0)
        , fail(0)
        , pendingTotal(0)
        , pendingPass(0)
        , pendingFail(0)
        , uiQueued(false)
        , lastHistMs(0)
    {}
};

/**
 * @class MainWindow
 * @brief 主窗：设计/运行页切换、方案存取、权限闸门。实现 IRunListener / ISessionListener
 */
class VISION_UI_API MainWindow : public QMainWindow, public IRunListener, public ISessionListener {
    Q_OBJECT
public:
    /** @brief session 不拥有，由 VisionApp 持有 */
    explicit MainWindow(Session* session, QWidget* parent = 0);
    /** @brief 释放工站运行实例 */
    virtual ~MainWindow();

    /** @brief 运行日志 sink：排队切到 UI 线程再刷面板 */
    void onRunLog(const RunLogEvent& ev);
    /** @brief 登录用户变化，刷新动作可用性 */
    void onUserChanged();
    /** @brief 语言/主题热切换后刷主窗与工作区文案 */
    void onUiLocaleChanged();
    /**
     * @brief 托盘常驻。关窗口只藏起来
     * @param on 为真时关闭主窗口回到托盘
     */
    void setResidentTray(bool on);
    /**
     * @brief 分布式时工站在 Vision.Station 里跑，界面只收 JPEG
     * @param on 单机为假，工站在本进程里跑
     */
    void setServiceRun(bool on);
    /** @brief 托盘选退出时才真正关掉进程 */
    void setForceQuit(bool on);
    /** @brief 切到运行页。托盘「运行画面」调用 */
    void showRuntimePage();

protected:
    /** @brief 未保存则询问 */
    void closeEvent(QCloseEvent* e);

private slots:
    /** @brief 切设计态（可能提权） */
    void switchDesign();
    /** @brief 切运行态 */
    void switchRuntime();
    /** @brief 切复检态 */
    void switchRecheck();
    /** @brief 复检批跑前绑定所选工站运行实例 */
    void onRecheckPrepareStation(const QString& stationId);
    /**
     * @brief 复检跳转设计调试
     * @param jobId 流程 id
     * @param toolId 工具 id
     * @param imagePath 复检图路径
     */
    void onRecheckOpenDebug(const QString& jobId, const QString& toolId, const QString& imagePath);
    /** @brief 新建方案 */
    void newSolution();
    /** @brief 打开方案 */
    void openSolution();
    /** @brief 保存方案 */
    void saveSolutionFile();
    /** @brief 另存方案 */
    void saveSolutionAs();
    /** @brief 打开底图文件（调试） */
    void openImage();
    /** @brief 工站管理 */
    void openStations();
    /** @brief 设计态快捷新建工站 */
    void newStation();
    /** @brief 设计态快捷新建流程（挂到当前工站） */
    void newJob();
    /** @brief 设备管理 */
    void openDevices();
    /** @brief 标定管理 */
    void openCalib();
    /** @brief 打开工具库管理，可热加载新插件 */
    void openToolLib();
    /** @brief 通信管理 */
    void openComm();
    /** @brief 用户管理 */
    void openUsers();
    /** @brief 权限说明（只读表） */
    void openPerms();
    /** @brief 打开本机设置中心 */
    void openAppSettings();
    /** @brief 运行主界面画面配置 */
    void openRuntimeView();
    /** @brief 全局变量表 */
    void openGlobalVars();
    /** @brief 自定义 UI 设计器 */
    void openUiDesigner();
    /** @brief 日志管控 */
    void openLogs();
    /** @brief 改当前用户口令 */
    void changePwd();
    /** @brief 注销 */
    void logout();
    /** @brief 关于对话框 */
    void about();
    /** @brief 打开版本管理对话框 */
    void openVersionManager();
    /** @brief 打开参数版本对话框 */
    void openParaAdapt();
    /** @brief 打开组合工具管理对话框 */
    void openComposites();
    /** @brief 设计树选中工站 */
    void onStation(const QString& id);
    /** @brief 设计树选中 JobCfg */
    void onJob(const QString& id);
    /** @brief 设计树选中设备 */
    void onDevice(const QString& id);
    /** @brief 设计树选中工具 */
    void onTool(const QString& id);
    /** @brief 跑当前工具 */
    void runThisTool();
    /** @brief 跑当前 JobCfg */
    void runThisJob();
    /** @brief 按 id 跑 JobCfg */
    void runJobById(const QString& jobId);
    /** @brief 循环跑全部工站 */
    void startStation();
    /** @brief 软件触发：等触发模式下跑一轮 */
    void pulseSoftTrigger();
    /** @brief 停止循环 */
    void stopStation();
    /** @brief 清零产量 */
    void resetCount();
    /** @brief UI 线程写日志（整行） */
    void onEngineLog(const QString& line);
    /** @brief UI 线程写运行日志（级别 + 正文） */
    void onEngineRunLog(const QString& level, const QString& message);
    /** @brief 设计态异步跑工具完成（outPtr 为堆上 ToolOut*，本槽释放） */
    void onDesignToolFinished(const QString& toolId, void* outPtr);
    /** @brief 设计态异步跑流程完成（outPtr 为堆上 ToolOut*） */
    void onDesignJobFinished(const QString& jobId, void* outPtr);
    /** @brief 运行态周期完成（resultPtr 为堆上 CycleUiPayload*） */
    void onStationCycleFinished(void* resultPtr);
    /** @brief 状态栏时钟 */
    void onClock();
    /** @brief 窗口可见时从共享内存刷运行画面 */
    void pullSharedPreview();
    /** @brief 刷新窗口标题 */
    void updateTitle();

private:
    /** @brief 工站循环线程回调 → Queued 到 UI */
    static void stationCycleThunk(void* user, const StationRunRst& result);
    /** @brief 无设计权限则弹登录提示 */
    bool ensureDesignAccess();
    /** @brief 无权限则提示 */
    bool ensurePerm(Perm perm, const QString& deniedText);
    /** @brief dirty 时询问保存 */
    bool confirmSave();
    /** @brief 应用设计/运行模式 */
    void applyMode(AppMode mode);
    /** @brief 按权限刷新菜单/工具栏 */
    void updateActions();
    /** @brief 重建菜单栏（语言热切换） */
    void buildMenus();
    /** @brief 刷新主窗静态文案 */
    void retranslateUi();
    /** @brief 记下当前可存盘内容，作为未改动基线 */
    void capturePersistBase();
    /** @brief 用可存盘内容对照基线，刷新 dirty */
    void syncDirty();
    /** @brief 清 dirty */
    void markSaved();
    /** @brief 打开工站编辑 */
    void openStationDlg(const QString& id);
    /** @brief 打开设备编辑 */
    void openDeviceDlg(const QString& id);
    /** @brief 打开 JobCfg 属性 */
    void showJobDlg(const QString& jobId);
    /** @brief 把工具输出送到设计/运行图像窗 */
    void applyToolOut(const QString& toolId, const ToolOut& out, bool toRuntime);
    /** @brief 方案读入后刷界面 */
    void applyLoadedSolution();
    /** @brief 按主工站配置重建 Station 运行实例 */
    void rebuildStation();
    /**
     * @brief 确保设计/复检用运行实例指向指定工站
     * @param config 方案内 StationCfg；空则释放实例
     */
    void ensureStationRuntime(StationCfg* config);
    /** @brief 是否有运行态循环在跑 */
    bool isRuntimeLive() const;
    /** @brief 停止并释放全部运行态工站槽（先把产量写入累计表） */
    void clearLiveStations();
    /** @brief 把 live 实例重新挂到运行壳面板 */
    void rebindLivePanels();
    /** @brief 从累计表恢复槽内产量 */
    void restoreProdToSlot(LiveStationSlot* slot);
    /** @brief 把槽内产量写回累计表 */
    void storeProdFromSlot(const LiveStationSlot* slot);
    /** @brief 清零累计产量（换方案或点清零） */
    void clearProdAccum();
    /** @brief 记下上次方案路径 */
    void rememberSolutionPath();
    /** @brief 启动时尝试打开上次方案；失败则提示，不挡住主窗 */
    void tryLastSolution();
    /**
     * @brief 退出前停工站线程、拒收/排空线程池、卸监听（可重复调用）
     */
    void shutdownRuntime();

    Session* m_session;              ///< 会话（不拥有）
    Solution m_sol;                  ///< 当前方案
    std::string m_persistBase; ///< 上次打开或保存的可存盘指纹
    AppMode m_mode;                  ///< 设计/运行
    Station* m_station;              ///< 设计/复检用工站实例（拥有）
    QString m_boundStationId;        ///< m_station 绑定的工站 id（防 stations 扩容悬空）
    std::vector<LiveStationSlot*> m_live; ///< 运行态多工站槽（拥有）
    /**
     * @class StationProdAccum
     * @brief 工站产量累计（跨停止/再开保留，清零或换方案才清）
     */
    struct StationProdAccum {
        int total; ///< 产量
        int pass;  ///< OK
        int fail;  ///< NG
        int cycleMs; ///< 最近节拍
        /** @brief 全部清零 */
        StationProdAccum() : total(0), pass(0), fail(0), cycleMs(0) {}
    };
    QHash<QString, StationProdAccum> m_prodAccum; ///< stationId → 产量累计
    RunLogHub m_runLogHub;           ///< 运行日志扇出（UI + VisionLog）
    VisionLogRunSink m_runLogFile;   ///< 运行日志落盘 sink
    bool m_running;                  ///< 是否在循环跑
    bool m_residentTray;             ///< 关窗口回托盘
    bool m_serviceRun;               ///< 分布式：工站在 Vision.Station，界面拉 JPEG
    bool m_forceQuit;                ///< 托盘退出，这次关闭不再藏窗口
    bool m_shuttingDown;             ///< 正在退出，异步回调勿再碰 UI/方案
    int m_designBusy;                ///< 设计态异步任务计数（>0 时勿开运行）
    QStackedWidget* m_stack;         ///< 设计/运行页
    DesignWorkspace* m_design;       ///< 设计工作区
    RuntimeWorkspace* m_runtime;     ///< 运行工作区
    RecheckWorkspace* m_recheck;     ///< 复检工作区
    QPushButton* m_tabDesign;        ///< 顶栏设计
    QPushButton* m_tabRuntime;       ///< 顶栏运行
    QPushButton* m_tabRecheck;       ///< 顶栏复检
    QToolBar* m_mainTb;              ///< 主工具条
    QAction* m_tbNew;                ///< 工具栏新建
    QAction* m_tbOpen;               ///< 工具栏打开
    QAction* m_tbSave;               ///< 工具栏保存
    QAction* m_tbSep1;               ///< 分隔
    QAction* m_tbNewStation;         ///< 新建工站
    QAction* m_tbNewJob;             ///< 新建流程
    QAction* m_tbStart;              ///< 开始
    QAction* m_tbStop;               ///< 停止
    QAction* m_tbReset;              ///< 清零
    QAction* m_actUsers;             ///< 用户管理
    QAction* m_actPerms;             ///< 权限
    QAction* m_actSettings;          ///< 系统设置
    QAction* m_actRuntimeView;       ///< 画面配置
    QAction* m_actGlobalVars;        ///< 全局变量
    QAction* m_actUiDesigner;        ///< UI 设计器
    QAction* m_actLogs;              ///< 日志管控
    QAction* m_actNew;               ///< 菜单新建
    QAction* m_actSave;              ///< 菜单保存
    WidgetPermBinder m_binder;       ///< 页面控件与权限绑定
    QLabel* m_userLabel;             ///< 状态栏用户
    QLabel* m_solLabel;              ///< 状态栏方案名
    QLabel* m_stLabel;               ///< 状态栏工站
    QLabel* m_clockLabel;            ///< 状态栏时钟
    QTimer* m_clock;                 ///< 时钟定时器
    QTimer* m_previewTimer;          ///< 运行画面打开时拉共享内存
};

}

#endif
