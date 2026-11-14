/**
 * @file DeskWindow.h
 * @brief 远程首页。本机被控和连接远端都在这个进程里，不打开原来的中控台或被控端
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DESK_WINDOW_H
#define VISION_DESK_WINDOW_H

#include <QIcon>
#include <QWidget>
#include <vector>

class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QTimer;
class QVBoxLayout;

namespace Vision {

class AgentSession;
class ControlSession;
class DeskView;
class RelayHub;

/**
 * @class DeskWindow
 * @brief 启动页。左边开本机被控，右边按设备号连接，画面在远程自己的窗口里
 */
class DeskWindow : public QWidget {
public:
    explicit DeskWindow(QWidget* parent = 0);
    ~DeskWindow();
    /** @brief 本进程要占用的端口。局域网用远程自己的一组，19280 只留给识别码隧道 */
    static void collectPorts(std::vector<int>* tcp, std::vector<int>* udp);

protected:
    void closeEvent(QCloseEvent* event);
    void showEvent(QShowEvent* event);

private:
    QWidget* makeCard();
    void toggleAgent();
    void connectPeer();
    void copyLocalId();
    /** @brief 读本程序自己的图标，和控制端、被控端分开 */
    QIcon loadIcon();
    void ensureControl();
    /** @brief 识别码隧道在本机的入口。外网用设备代码和验证码走这里 */
    void ensureTunnel();
    /** @brief 放开直连和发现端口，同一局域网才能连进来 */
    void openLan();
    /** @brief 外网配对。对方刚挂上时多试几次 */
    bool openWan(const QString& deviceId, const QString& code, std::string* err);
    void openView(const QString& deviceId, const QString& code);
    void refreshTitles();
    void refreshRecent();
    void rememberRecent(const QString& deviceId);

    AgentSession* m_agent;
    ControlSession* m_control;
    RelayHub* m_relay;
    DeskView* m_view;
    bool m_agentOn;
    bool m_lanRule;
    QTimer* m_timer;
    QPushButton* m_openAgent;
    QPushButton* m_connect;
    QLineEdit* m_peerId;
    QLineEdit* m_peerCode;
    QLabel* m_localId;
    QLabel* m_code;
    QLabel* m_agentChip;
    QLabel* m_controlChip;
    QLabel* m_hint;
    QVBoxLayout* m_recentLay;
};

}

#endif
