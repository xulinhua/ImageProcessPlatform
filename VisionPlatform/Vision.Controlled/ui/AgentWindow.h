/**
 * @file AgentWindow.h
 * @brief 被控端窗口：本机 ID、当前会话、共享范围与预览；连接/安全等进设置对话框，托盘常驻
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_AGENT_WINDOW_H
#define VISION_CONTROLLED_AGENT_WINDOW_H

#include "AgentSession.h"
#include "HttpPreview.h"
#include "PrivacyGuard.h"
#include <QIcon>
#include <QStringList>
#include <QWidget>
#include <string>

class QAction;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QMenuBar;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QSystemTrayIcon;
class QTimer;
class QVBoxLayout;

namespace Vision {

class ConfirmDialog;
class ControlBar;
class ProcessCombo;
class RemoteLogWindow;
class StatusDot;
class TagLabel;

/**
 * @class AgentWindow
 * @brief 被控端主窗口：展示本机 ID、会话与共享范围；连接、安全、编码等在「设置」里改
 * @note 配置存 exe 旁的 controlled.json。每 200 ms 轮询会话刷新界面，并负责弹出确认框和被控提示条。
 *       关窗口缩到托盘，只有“退出”才真正结束
 */
class AgentWindow : public QWidget {
public:
    /** @brief 搭界面、读配置、启动会话和托盘 */
    explicit AgentWindow(QWidget* parent = 0);
    /** @brief 停止会话，释放确认框与提示条 */
    ~AgentWindow();

protected:
    /** @brief 有托盘且不是“退出”时只隐藏到托盘；否则退出程序 */
    void closeEvent(QCloseEvent* event) override;

private:
    /************************************************** 搭界面 **************************************************/
    /** @brief 菜单栏：文件、共享、安全、设置、查看、帮助 */
    QMenuBar* buildMenus();
    /** @brief 左上蓝色卡片：设备 ID（可复制）与一次性验证码占位 */
    QFrame* buildIdCard();
    /** @brief 当前会话卡片：服务状态、反连地址、会话行与断开、重新连接按钮 */
    QFrame* buildSessions();
    /** @brief 共享范围卡片：桌面/进程画面、显示器、进程与标题、预览 */
    QFrame* buildShare();
    /** @brief 设置面板：连接 / 安全 / 画面 / 服务四页，平时隐藏，设置对话框里显示 */
    QWidget* buildSettingsBody();
    /** @brief 设置 · 连接：反连、直连、局域网查看、中继 */
    QWidget* buildConnectionPage(QWidget* parent);
    /** @brief 设置 · 安全：来访确认、权限、白名单、录屏等 */
    QWidget* buildSecurityPage(QWidget* parent);
    /** @brief 设置 · 画面：抓/编并行、发送编码 */
    QWidget* buildPicturePage(QWidget* parent);
    /** @brief 设置 · 服务：安装 / 卸载 Windows 服务 */
    QWidget* buildServicePage(QWidget* parent);
    /** @brief 打开设置对话框 */
    void openSettings(int page = 0);
    /**
     * @brief 建一张白色卡片
     * @param title 标题，空时不画标题行
     * @param iconName 标题前的图标名，可空
     * @param col 输出卡片内的纵向布局
     */
    QFrame* card(const QString& title, const char* iconName, QVBoxLayout** col);
    /** @brief 把最多 5 个控件横排成一行（右侧留弹性空白）；a 不能为空，父对象取 a 的父 */
    QWidget* line(QWidget* a, QWidget* b = 0, QWidget* c = 0, QWidget* d = 0, QWidget* e = 0);

    /************************************************** 配置 **************************************************/
    /** @brief 先填默认值（127.0.0.1:19100、整个桌面），再从 controlled.json 覆盖 */
    void loadConfig();
    /** @brief 把界面当前设置和信任列表原子写回 controlled.json */
    void saveConfig() const;
    /** @brief 保存配置并按界面设置重启会话（会断开当前连接） */
    void apply();
    /** @brief 同步策略单选与菜单勾选、刷新信任台数，下发给会话并保存 */
    void applyPolicy();

    /************************************************** 刷新 **************************************************/
    /** @brief 定时刷新：同步控制端下发的共享范围、会话状态、确认框、提示条、托盘提示和预览 */
    void refresh();
    /** @brief 中控台弹窗。不挡住本窗口的刷新，关到托盘时仍能看到 */
    void showNotice(const QString& name, const QString& text);
    /** @brief 按桌面/进程画面启用对应输入框，并同步共享菜单的勾选 */
    void syncShare();
    /** @brief 改来访确认策略，confirm 取 AgentSession::Confirm 的值 */
    void setConfirm(int confirm);
    /** @brief 模态对话框：查看并移除信任的控制端（按证书指纹） */
    void manageTrusted();
    /** @brief 设置或清除无人值守密码 */
    void changePassword();
    /** @brief 刷新并显示一次性验证码 */
    void refreshCodeUi();
    /** @brief 安装 / 卸载被控端 Windows 服务 */
    void installService(bool install);
    /** @brief 把白名单文本拆成列表并下发给会话 */
    void applyWhitelist();
    /** @brief 按勾选和端口启停局域网 HTTP 查看，并刷新地址提示 */
    void applyHttp();
    /** @brief 直连、中继、生产保护 */
    void applyLink();
    /** @brief 按界面把运行日志落盘策略交给 RemoteLog */
    void applyRunLog();
    /** @brief 弹出运行日志窗口（抓屏/编码/发送耗时） */
    void openRunLog();
    /** @brief 弹出与中控台互传的双栏文件窗 */
    void openFileTransfer();

    /************************************************** 托盘 **************************************************/
    /** @brief 建托盘图标与菜单（打开界面、重新连接、退出），单击或双击打开界面 */
    void setupTray();
    /** @brief 从托盘恢复并激活窗口 */
    void showFromTray();
    /** @brief 标记真正退出后关闭窗口 */
    void quitApp();
    /** @brief 从 exe 资源 1 号图标加载 16 ~ 256 各尺寸到 m_icon */
    void loadIcon();
    /** @brief 把 HICON 按 side 边长画成位图加入 m_icon；参数用 HICON__* 以免头文件引入 windows.h */
    void addIconSize(struct HICON__* icon, int side);
    /** @brief 毫秒格式化为“时:分:秒”，各两位 */
    static QString clock(uint64_t ms);
    /** @brief 显示器下拉：最后一项为全部（255） */
    int monitorValue() const;
    /** @brief 解析额外反连 host:port 行 */
    std::vector<std::pair<std::string, int> > parseExtraHosts() const;

    AgentSession m_session;             ///< 被控端会话，窗口在它就在
    ConfirmDialog* m_confirmDialog;     ///< 来访确认框，无父对象，析构时删除
    ControlBar* m_bar;                  ///< 被控提示条，无父对象，析构时删除

    TagLabel* m_sessionCount;           ///< 会话数标签：已连接 1，否则 0
    QLabel* m_svcState;                 ///< 服务状态文字：服务运行中 / 正在连接 / 已手动断开
    StatusDot* m_svcDot;                ///< 服务状态点：绿 / 灰 / 橙
    QLabel* m_svcLink;                  ///< 反连说明：控制端名、地址、明文 TCP
    QWidget* m_sessionRow;              ///< 会话行，已连接时显示
    QLabel* m_sessionAvatar;            ///< 会话行头像，显示控制端名首字
    QLabel* m_sessionName;              ///< 会话行控制端名
    QLabel* m_sessionMeta;              ///< 会话行地址与权限（观看 / 观看 操作）
    TagLabel* m_sessionTag;             ///< 会话行状态标签：观看 / 操作中 / 本地暂停
    QLabel* m_sessionTime;              ///< 会话行计时：操作中为操作时长，否则为连接时长
    QLabel* m_noSession;                ///< 无会话时的提示文字
    QPlainTextEdit* m_notes;            ///< 中控台发来的聊天记录
    QPushButton* m_disconnectAll;       ///< “断开全部”，已连接时可用
    QPushButton* m_reconnect;           ///< “重新连接”，手动断开后才显示

    QRadioButton* m_desktop;            ///< 共享整个桌面
    QRadioButton* m_window;             ///< 共享进程画面
    QComboBox* m_monitor;               ///< 显示器选择；最后一项“全部显示器”=255
    QRadioButton* m_injectPost;         ///< 进程画面：窗口消息注入
    QRadioButton* m_injectFront;        ///< 进程画面：前台 SendInput
    ProcessCombo* m_process;            ///< 进程画面的进程 exe 名，可下拉选或手输
    QLineEdit* m_title;                 ///< 进程画面的窗口标题子串
    QLabel* m_previewInfo;              ///< 预览标题，带图像尺寸
    QLabel* m_preview;                  ///< 送出画面预览（300×169），无画面时显示状态文字
    QLineEdit* m_host;                  ///< 反连的控制端地址
    QSpinBox* m_port;                   ///< 反连的控制端端口
    QPlainTextEdit* m_extraHosts;       ///< 额外反连中控台，每行 host:port

    QRadioButton* m_confirmAlways;      ///< 策略：每次都要现场确认
    QRadioButton* m_confirmTrusted;     ///< 策略：信任的控制端免确认
    QRadioButton* m_confirmNever;       ///< 策略：从不确认
    QLabel* m_trustedCount;             ///< 信任的控制端台数
    QLineEdit* m_whitelist;             ///< IP/主机白名单，逗号或换行分隔
    QCheckBox* m_allowOp;              ///< 允许远端键鼠
    QCheckBox* m_allowClip;            ///< 允许远端剪贴板
    QCheckBox* m_allowFile;            ///< 允许远端文件
    QCheckBox* m_allowPower;           ///< 允许远端电源
    QCheckBox* m_allowPrivacy;         ///< 允许远端隐私屏
    QCheckBox* m_lockOnEnd;             ///< 断开后锁屏
    QCheckBox* m_forceRecord;           ///< 对方取得操作后必须录屏
    QLineEdit* m_fileRoots;             ///< 文件根目录，分号分隔。空=全部盘，可指定盘/目录
    QCheckBox* m_httpOn;                ///< 局域网 HTTP 查看
    QCheckBox* m_directOn;              ///< 允许直连
    QSpinBox* m_directPort;             ///< 直连端口
    QCheckBox* m_relayOn;               ///< 启用中继
    QLineEdit* m_relayHost;             ///< 中继地址
    QSpinBox* m_relayPort;              ///< 中继端口
    QLineEdit* m_relayKey;              ///< 中继口令
    QCheckBox* m_production;            ///< 生产保护，操作必须现场确认
    QCheckBox* m_allowHighest;          ///< 允许中控台最高权限（满权限开放）
    QCheckBox* m_logDisk;               ///< 运行日志写盘，默认关
    QCheckBox* m_logPictureDisk;        ///< 画面链路 Info 写盘
    QComboBox* m_logRotate;             ///< 滚动间隔分钟
    QSpinBox* m_logKeepDays;            ///< 运行日志保留天数
    QSpinBox* m_httpPort;               ///< HTTP 端口
    QSpinBox* m_pipelineWorkers;        ///< 抓/编并行：0 内联，1–2 独立编码线程
    QComboBox* m_preferCodec;           ///< 发送编码：自动 / JPEG / H.264 / 原图
    QLabel* m_httpUrl;                  ///< 浏览器地址
    HttpPreview m_http;                 ///< 局域网查看服务
    QLabel* m_codeLabel;                ///< 一次性验证码大号显示
    TagLabel* m_passwordTag;            ///< 无人值守密码状态
    QLabel* m_fpLabel;                  ///< 本机证书指纹（短显）
    QWidget* m_settingsBody;            ///< 设置面板（四页），对话框打开时临时挂过去

    QAction* m_actDesktop;              ///< 菜单“共享 > 整个桌面”
    QAction* m_actWindow;               ///< 菜单“共享 > 进程画面”
    QAction* m_actAlways;               ///< 菜单“安全 > 每次都要现场确认”
    QAction* m_actTrusted;              ///< 菜单“安全 > 信任的控制端免确认”
    QAction* m_actNever;                ///< 菜单“安全 > 从不确认”
    QAction* m_actLock;                 ///< 菜单“安全 > 远程操作结束后锁屏”，与 m_lockOnEnd 同步

    QTimer* m_timer;                    ///< 界面刷新定时器，200 ms 调一次 refresh
    QWidget* m_shade;                   ///< 隐私屏，盖住本机桌面
    PrivacyGuard m_guard;               ///< 隐私屏期间丢掉本机键鼠，保留注入输入
    QSystemTrayIcon* m_tray;            ///< 托盘图标，setupTray 前为空
    QMenu* m_trayMenu;                  ///< 托盘右键菜单
    QIcon m_icon;                       ///< 窗口与托盘图标，exe 资源取不到时用内置线条图标
    bool m_quit;                        ///< 用户选了“退出”，closeEvent 不再缩到托盘
    int m_confirm;                      ///< 当前来访确认策略，取 AgentSession::Confirm 的值
    QStringList m_trusted;              ///< 信任的控制端证书指纹（兼容旧配置里的显示名）
    uint64_t m_shownSeq;                ///< 已弹出过确认框的申请序号，避免同一申请重复弹
    QString m_passwordHash;             ///< 无人值守密码 salt:hash，空表示未设置
    RemoteLogWindow* m_logWin;          ///< 运行日志窗，按钮打开，关窗只隐藏
};

}

#endif
