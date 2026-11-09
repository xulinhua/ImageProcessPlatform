/**
 * @file ControlWindow.h
 * @brief 控制端主窗口：菜单、工具条、设备树、监控墙、状态栏、托盘。单台设备在 SessionWindow 里操作
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_WINDOW_H
#define VISION_CONTROL_WINDOW_H

#include "AuditLog.h"
#include "ControlConfig.h"
#include "ControlSession.h"
#include "H264Codec.h"
#include "RemoteAuthHost.h"
#include "DeviceStore.h"
#include "DeviceTree.h"
#include <QIcon>
#include <QImage>
#include <QWidget>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class QAction;
class QCloseEvent;
class QComboBox;
class QLabel;
class QMenu;
class QMenuBar;
class QPushButton;
class QSpinBox;
class QSystemTrayIcon;
class QTimer;

namespace Vision {

class RemoteLogWindow;
class SegmentBar;
class SessionWindow;
class StatusDot;
class WallView;

/**
 * @class ControlWindow
 * @brief 控制端主窗口：持有配置、设备库、审计和收发会话，100 ms 定时把会话状态刷到监控墙、设备树、会话窗口和状态栏
 * @note 设备上下线、取得/释放操作、操作被拒都在 trackTiles() 里写审计。局域网发现在后台线程，结果经 m_scanMu 交给界面
 */
class ControlWindow : public QWidget {
public:
    /** @brief 读配置与设备库、清理过期审计、搭界面、开始监听和局域网发现、建托盘 */
    explicit ControlWindow(QWidget* parent = 0);
    /** @brief 关掉所有会话窗口，停止发现与监听，保存设备库 */
    ~ControlWindow();

    /** @brief 配置要求启动时只留托盘 */
    bool startHidden() const;

protected:
    /** @brief 允许缩到托盘时隐藏窗口，否则（或从菜单退出时）退出程序 */
    void closeEvent(QCloseEvent* event) override;

private:
    /** @brief 每台设备的帧率、码率，按秒结算 */
    struct Rate {
        uint64_t seq;       ///< 上次结算时的主码流帧序号
        uint64_t thumbSeq;  ///< 上次结算时的缩略图帧序号（监控墙只用子码流时靠它）
        uint64_t bytes;     ///< 上次结算时的累计接收字节
        qint64 at;          ///< 上次结算时刻（epoch 毫秒），0 未开始
        double fps;         ///< 最近一秒帧率
        double kbps;        ///< 最近一秒下行码率 kbps
        /** @brief 全部清零 */
        Rate() : seq(0), thumbSeq(0), bytes(0), at(0), fps(0), kbps(0) {}
    };

    /** @brief 上一轮看到的状态，变化时写审计 */
    struct Seen {
        bool online;            ///< 上轮在线
        bool operating;         ///< 上轮持有操作权
        bool highestTried;      ///< 本次在线已发过最高权限自动申请（释放后不重申，直到重连）
        std::string hint;       ///< 上轮的提示文字，UTF-8
        qint64 since;           ///< 本次上线时刻（epoch 毫秒），算会话时长
        qint64 operateSince;    ///< 本次取得操作权时刻（epoch 毫秒），算操作时长
        /** @brief 离线、未操作 */
        Seen() : online(false), operating(false), highestTried(false), since(0), operateSince(0) {}
    };

    /** @brief 按全局 > 单台 > 账户算出该设备是否走最高权限 */
    bool highestForDevice(const QString& key) const;

    /** @brief 监控墙格子的解码缓存 */
    struct Picture {
        uint64_t seq;   ///< 已解码的画面帧序号
        bool gray;      ///< image 已转灰度（离线）
        uint8_t codec;  ///< 上次成功解码的编码，切换时丢掉静帧
        QImage image;   ///< 解码结果
        /** @brief 空缓存 */
        Picture() : seq(0), gray(false), codec(0) {}
    };

    /************************************************** 搭建 **************************************************/
    /** @brief 菜单栏：文件、设备、视图、工具、帮助；顺带注册翻页、全屏墙、Esc 窗口级快捷键 */
    QMenuBar* buildMenus();
    /** @brief 工具条：添加、刷新、布局、行列、轮巡、显示筛选、翻页、全屏墙 */
    QWidget* buildToolbar();
    /** @brief 状态栏：监听状态、在线数、中继、下行码率、当前用户 */
    QWidget* buildStatus();
    /** @brief 建托盘图标与菜单，单击或双击还原主窗 */
    void setupTray();
    /** @brief 从 exe 资源 1 号图标读出多种尺寸放进 m_icon */
    void loadIcon();
    /** @brief 把 HICON 按 side 尺寸画成位图加入 m_icon */
    void addIconSize(struct HICON__* icon, int side);

    /************************************************** 网络 **************************************************/
    /** @brief 按配置开始监听，结果显示在状态栏，失败弹框 */
    void startListen();
    /** @brief 配置开启发现时起后台线程，每轮扫 700 ms、歇约 5 秒 */
    void startScan();
    /** @brief 停后台发现线程；发现已关闭时清空结果 */
    void stopScan();
    /** @brief 在界面线程立即扫一次（阻塞约 700 ms），更新并返回结果 */
    std::vector<HelloMsg> foundNow();

    /************************************************** 刷新 **************************************************/
    /** @brief 定时器入口：审计跟踪、码率、监控墙、会话窗口每次刷新；设备树和状态栏每 5 次刷新；脏数据 30 秒存一次 */
    void refresh();
    /** @brief 在线设备订子码流；打开会话的设备才订主码流 */
    void syncStreams(const std::vector<ControlSession::Tile>& tiles);
    /** @brief 按筛选和当前页生成格子，解码有新帧的画面（离线转灰度） */
    void refreshWall(const std::vector<ControlSession::Tile>& tiles);
    /** @brief 刷设备树行、局域网发现列表（结果有变才刷）和监控墙筛选下拉框 */
    void refreshTree(const std::vector<ControlSession::Tile>& tiles);
    /** @brief 刷状态栏在线数、总下行码率和托盘提示 */
    void refreshStatus(const std::vector<ControlSession::Tile>& tiles);
    /** @brief 按 IP 登记的条目并到上报的设备 ID 下，在线设备写入设备库（新设备记审计），并对比上一轮状态写会话/操作审计 */
    void trackTiles(const std::vector<ControlSession::Tile>& tiles);
    /** @brief 按帧序号和字节数增量结算每台设备的帧率、码率，间隔不足 1 秒不结算 */
    void updateRates(const std::vector<ControlSession::Tile>& tiles);
    /** @brief 找设备 key 对应的状态，优先在线的那条；没有返回空 */
    const ControlSession::Tile* tileOf(const std::vector<ControlSession::Tile>& tiles, const QString& key) const;
    /** @brief 状态对应的设备 key；还没收到 Hello（无绑定键）返回空 */
    QString keyOf(const ControlSession::Tile& tile) const;
    /** @brief 设备显示名，库里没有时返回 key */
    QString nameOf(const QString& key) const;
    /** @brief 按「显示」筛选（全部、在线、某分组）得到的监控墙设备 key，按设备库顺序 */
    std::vector<QString> wallKeys() const;

    /************************************************** 监控墙 **************************************************/
    /** @brief 设行列（限制 1~4），同步工具条与菜单，回到第一页，变化时存配置 */
    void setGrid(int rows, int cols);
    /** @brief 前后翻页，首尾循环 */
    void turnPage(int delta);
    /** @brief 开关轮巡，同步按钮与菜单 */
    void setTour(bool on);
    /** @brief 全屏墙：隐藏菜单、工具条、状态栏和设备树，Esc 退出 */
    void setWallFull(bool full);

    /************************************************** 设备 **************************************************/
    /** @brief 打开或激活设备的会话窗口，记最近打开时间；key 不在库里返回空 */
    SessionWindow* openSession(const QString& key);
    /** @brief 向选中设备发魔术包。没有 MAC 时先询问 */
    void wakeSelected();
    /** @brief 向一台、多台或全部在线被控端发聊天或弹窗 */
    void sendNotice();
    /** @brief 添加设备对话框，确认后把选中项写入设备库并记审计 */
    void openAddDevice();
    /** @brief 设备属性对话框 */
    void openProperties(const QString& key);
    /** @brief 设置对话框，应用时按需重启监听和发现、刷新布局、清理审计 */
    void openSettings(int page);
    /** @brief 审计日志对话框 */
    void openAudit();
    /** @brief 弹出运行日志窗口（画面链路耗时） */
    void openRunLog();
    /** @brief 重命名设备；与上报名相同或留空时清掉别名 */
    void renameDevice(const QString& key);
    /** @brief 确认后删除设备，关闭其会话窗口并记审计 */
    void removeDevice(const QString& key);
    /** @brief 设备移到分组，group 为空即未分组 */
    void moveToGroup(const QString& key, const QString& group);
    /** @brief 输入名称新建分组 */
    void newGroup();
    /** @brief 输入新名称重命名分组 */
    void renameGroup(const QString& group);
    /** @brief 确认后删除分组，组内设备移到未分组 */
    void removeGroup(const QString& group);
    /** @brief 把局域网发现列表第 index 项加入设备库（已有则忽略） */
    void addFound(int index);
    /**
     * @brief 右键菜单
     * @param key 设备 key，非空时弹设备菜单
     * @param group 分组名，key 为空且非空时弹分组菜单；都为空弹空白处菜单
     * @param pos 全局坐标
     */
    void showDeviceMenu(const QString& key, const QString& group, const QPoint& pos);

    /************************************************** 托盘 **************************************************/
    /** @brief 还原并激活主窗，全屏墙时保持全屏 */
    void showFromTray();
    /** @brief 真正退出（不缩托盘） */
    void quitApp();

    ControlConfig m_config;     ///< 控制端配置
    RemoteAuthHost m_auth;      ///< 可选平台账号。没有插件和账号文件时不登录
    DeviceStore m_store;        ///< 设备库
    AuditLog m_audit;           ///< 审计日志
    ControlSession m_session;   ///< 收发会话，会话窗口和对话框借用其指针

    QMenuBar* m_menu;                   ///< 菜单栏
    QWidget* m_toolbar;                 ///< 工具条
    QWidget* m_status;                  ///< 状态栏
    DeviceTree* m_tree;                 ///< 左侧设备树
    WallView* m_wall;                   ///< 监控墙
    SegmentBar* m_layoutSeg;            ///< 布局快捷 1×1~4×4，行列不等时无选中
    QSpinBox* m_rows;                   ///< 行数 1~4
    QSpinBox* m_cols;                   ///< 列数 1~4
    QPushButton* m_tour;                ///< 轮巡开关按钮
    QComboBox* m_tourSeconds;           ///< 轮巡间隔，item data 为秒数
    QComboBox* m_filter;                ///< 显示筛选，data：空为全部，"\x01online" 为在线，其余为分组名
    QLabel* m_page;                     ///< 页码「当前 / 总数」
    QPushButton* m_prev;                ///< 上一页
    QPushButton* m_next;                ///< 下一页
    QAction* m_actTour;                 ///< 视图菜单：轮巡
    QAction* m_actFull;                 ///< 视图菜单：全屏墙（F11）
    QAction* m_actTree;                 ///< 视图菜单：显示设备列表
    std::vector<QAction*> m_actGrid;    ///< 视图菜单：1×1~4×4，下标为 n-1

    StatusDot* m_listenDot;     ///< 状态栏：监听状态点
    QLabel* m_listen;           ///< 状态栏：监听地址或失败信息
    QLabel* m_relayStatus;      ///< 状态栏：中继是否启用
    QLabel* m_userStatus;       ///< 状态栏：当前登录用户
    QLabel* m_online;           ///< 状态栏：在线数 / 总数
    QLabel* m_rate;             ///< 状态栏：总下行码率

    QTimer* m_timer;            ///< 100 ms 刷新定时器
    QTimer* m_tourTimer;        ///< 轮巡翻页定时器
    QSystemTrayIcon* m_tray;    ///< 托盘图标
    QMenu* m_trayMenu;          ///< 托盘菜单
    QIcon m_icon;               ///< 程序图标，资源缺失时用内置图标
    RemoteLogWindow* m_logWin;  ///< 运行日志窗，按钮打开，关窗只隐藏

    std::map<QString, SessionWindow*> m_sessions;   ///< 已打开的会话窗口，按设备 key；窗口关闭时移除
    std::map<std::string, int> m_stream;            ///< peerId -> 已下发的码流：1 子码流，2 主+子
    std::map<QString, Rate> m_rates;                ///< 按设备 key 的帧率、码率
    std::map<QString, Seen> m_seen;                 ///< 按设备 key 的上一轮状态
    std::map<QString, Picture> m_pictures;          ///< 按设备 key 的监控墙画面缓存
    std::map<QString, H264Codec> m_wallH264;        ///< 监控墙主码流 H.264 解码，按设备 key
    std::vector<QString> m_keys;                    ///< 未使用

    std::thread m_scan;                 ///< 局域网发现后台线程
    std::mutex m_scanMu;                ///< 保护 m_found
    std::atomic<bool> m_scanRun;        ///< 发现线程运行标志
    std::vector<HelloMsg> m_found;      ///< 最近一次发现结果
    QString m_foundSig;                 ///< 上次推给设备树的发现结果签名，变化才刷新

    bool m_quit;            ///< 正在退出，closeEvent 不再缩托盘
    bool m_wallFull;        ///< 全屏墙状态
    int m_pageIndex;        ///< 监控墙当前页，从 0 起
    int m_tick;             ///< 刷新计数，用于降频刷设备树和状态栏
    qint64 m_lastSave;      ///< 上次保存设备库的时刻（epoch 毫秒）
    bool m_dirty;           ///< 设备库有未保存的 lastSeen 等变更
};

}

#endif
