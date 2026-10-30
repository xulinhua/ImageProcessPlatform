/**
 * @file TrayApp.h
 * @brief 常驻托盘。关主窗口回到这里，不退出进程，也不停服务
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_APP_TRAYAPP_H
#define VISION_APP_TRAYAPP_H

#include "ConsoleHost.h"

#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMenu;
class QTimer;

namespace Vision {
class MainWindow;
}

/**
 * @class TrayApp
 * @brief 图标颜色跟心跳和故障走。启动、停止只写共享内存命令
 */
class TrayApp : public QObject {
public:
    /**
     * @brief 创建托盘并打开主界面。分布式才拉起对应服务
     * @param stationSvc 为真时启动 Vision.Station
     * @param linkSvc 为真时启动 Vision.Link
     */
    explicit TrayApp(Vision::MainWindow* window, bool stationSvc, bool linkSvc, QObject* parent = 0);
    /** @brief 托盘随进程结束 */
    ~TrayApp();

private:
    /************************************************** 托盘 **************************************************/
    /** @brief 左键或双击才打开主界面。右键只出菜单 */
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    /** @brief 按心跳年龄和故障换图标 */
    void refreshIcon();

    /************************************************** 界面 **************************************************/
    /** @brief 打开主界面 */
    void openMain();
    /** @brief 打开运行画面 */
    void openRuntime();

    /************************************************** 工站命令 **************************************************/
    /** @brief 写启动命令 */
    void startOwned();
    /** @brief 写停止命令 */
    void stopOwned();

    /************************************************** 菜单切换 **************************************************/
    /** @brief 服务在运行则停止，否则启动 */
    void toggleServices();
    /** @brief 主界面可见则藏起，否则打开 */
    void toggleUi();
    /** @brief 控制台可见则藏起，否则打开 */
    void toggleConsole();

    /************************************************** 服务 **************************************************/
    /** @brief 弹出服务状态 */
    void showServices();
    /** @brief 拉起两个服务。没安装就启动同目录 exe */
    void startServices();
    /** @brief 停两个服务 */
    void stopServices();

    /************************************************** 退出 **************************************************/
    /** @brief 停服务和同名进程，再关界面 */
    void quitApp();
    /** @brief 先停工站和服务，再关界面进程 */
    void stopAllAndQuit();

private:
    /************************************************** 图标 **************************************************/
    /** @brief 应用图标右下角叠状态点 */
    QIcon makeTrayIcon(const QColor& status) const;
    /** @brief 菜单用的线框图标。kind：0 服务，1 主界面，2 控制台 */
    QIcon makeMenuIcon(int kind, const QColor& color) const;

    Vision::MainWindow* m_window; ///< 主界面
    QSystemTrayIcon* m_tray;      ///< 托盘图标
    QMenu* m_menu;                ///< 右键菜单
    QAction* m_actService;        ///< 服务。运行中着色，未启动置灰
    QAction* m_actUi;             ///< 主界面。显示时着色，隐藏时置灰
    QAction* m_actConsole;        ///< 控制台。显示时着色，隐藏或未打开时置灰
    QTimer* m_timer;              ///< 刷新图标
    bool m_stationSvc;            ///< 本机要跑 Vision.Station
    bool m_linkSvc;               ///< 本机要跑 Vision.Link
    ConsoleHost m_console;        ///< 托盘里打开的控制台
    QIcon m_appIcon;              ///< 应用图标
};

#endif
