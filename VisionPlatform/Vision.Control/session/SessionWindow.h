/**
 * @file SessionWindow.h
 * @brief 单台设备的远程会话窗口：工具条、画面、右侧面板（聊天/会话信息/快捷键）、状态栏、全屏浮条
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_SESSION_WINDOW_H
#define VISION_CONTROL_SESSION_WINDOW_H

#include "ControlSession.h"
#include "Mp4Record.h"
#include <QString>
#include <vector>
#include <stdio.h>
#include <QWidget>
#include <functional>

class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTabBar;
class QVBoxLayout;

namespace Vision {

class AuditLog;
class ControlConfig;
class FrameView;
class SegmentBar;
class StatusDot;
class TagLabel;

/**
 * @class SessionWindow
 * @brief 一台设备的会话窗口：看画面、申请/释放操作、切显示器、截图、全屏
 * @note 关闭即销毁（WA_DeleteOnClose）。不自己拉数据，由 ControlWindow 定时调 refresh() 推送
 */
class SessionWindow : public QWidget {
public:
    /**
     * @brief 搭建界面，按配置设缩放方式，把画面上的键鼠输入接到会话
     * @param session 收发会话，不拥有
     * @param config 控制端配置，不拥有
     * @param audit 审计日志，不拥有
     * @param key 设备库 key，用来与监控墙格子对应
     */
    SessionWindow(ControlSession* session, const ControlConfig* config, AuditLog* audit, const QString& key,
        QWidget* parent = 0);

    /************************************************** 会话 **************************************************/
    /** @brief 构造时传入的设备 key */
    QString key() const;

    /** @brief tile 为空表示设备不在线 */
    void refresh(const ControlSession::Tile* tile, const QString& name, double fps, double kbps);
    /** @brief 在线且未持有操作权时申请操作 */
    void requestOperate();
    /** @brief 文件传输对话框。需要已经取得操作权 */
    void openFiles();
    /** @brief 再邀请一台中控台来看。最多三台 */
    void inviteViewer();
    /** @brief 进入或退出全屏：隐藏工具条、面板和状态栏，顶部显示浮条 */
    void setFullScreenMode(bool full);
    /** @brief 点「监控墙」时的回调，回调后本窗口关闭 */
    void setOnBack(const std::function<void()>& back);
    /** @brief 窗口关闭时的回调，参数为设备 key */
    void setOnClosed(const std::function<void(const QString&)>& closed);
    /** @brief 用中控台设置里的默认画质和帧率，并下发 */
    void usePictureDefaults();

    /************************************************** 格式化 **************************************************/
    /** @brief 码率文字：>= 1000 kbps 显示 Mbps，保留一位小数 */
    static QString rateText(double kbps);
    /** @brief 共享种类文字：「桌面 · 显示器 n」或「窗口 · 共享源」 */
    static QString shareText(const ControlSession::Tile& tile);

protected:
    /** @brief 补发未松开的键鼠，持有操作权时释放，再通知 setOnClosed 回调 */
    void closeEvent(QCloseEvent* event) override;
    /** @brief 让全屏浮条保持在顶部居中 */
    void resizeEvent(QResizeEvent* event) override;

private:
    /************************************************** 搭建 **************************************************/
    /** @brief 顶部工具条；未实现的功能按钮置灰并提示所在期次 */
    QWidget* buildToolbar();
    /** @brief 右侧面板：聊天（暂记录会话事件）、会话信息、快捷键三页 */
    QWidget* buildPanel();
    /** @brief 底部状态栏 */
    QWidget* buildStatus();
    /** @brief 全屏时的顶部浮条，初始隐藏 */
    QWidget* buildFloatBar();
    /**
     * @brief 状态栏加一项
     * @param iconName 左侧图标名，空则不带图标
     * @return 富文本标签，由 syncControls() 填内容
     */
    QLabel* addStatus(QHBoxLayout* row, const char* iconName);
    /** @brief 会话信息页加一行「键 值」，返回值标签（初始为 -） */
    QLabel* addInfo(QGridLayout* grid, int row, const QString& key);

    /************************************************** 动作 **************************************************/
    /** @brief 聊天页追加一条带时间的会话事件 */
    void addEvent(const QString& text);
    /** @brief 显示器块数或共享种类变化时重建显示器按钮，否则只同步选中项 */
    void rebuildMonitors();
    /** @brief 桌面共享时切到第 index 块显示器，下发配置并记审计 */
    void pickMonitor(int index);
    /** @brief 按当前画质档与帧率上限组装 ConfigMsg（共享范围取自 m_tile） */
    ConfigMsg makeConfig() const;
    /** @brief 画质档或帧率上限变化时下发配置 */
    void pushPictureConfig(const QString& reason);
    /** @brief 用中控台设置里的默认画质和帧率，并下发 */
    /** @brief 画质档位 → JPEG 质量、长边；帧率上限取自 m_maxFps */
    void fillQuality(ConfigMsg* cfg) const;
    /** @brief 在线时切换操作权：持有则释放，否则申请 */
    void toggleOperate();
    /** @brief 当前画面存 PNG 到截图目录并记审计 */
    void takeShot();
    /** @brief 把输入框里的文字发给被控端 */
    void sendChat();
    /** @brief 把本机剪贴板推到被控端 */
    void pushClipboard();
    /** @brief 发锁屏、注销、重启或关机 */
    void sendPower(uint8_t action);
    /** @brief 开关被控端隐私屏 */
    void togglePrivacy();
    /** @brief 开始或停止录屏 */
    void toggleRecord();
    /** @brief 停掉 MP4 或 AVI，并记审计 */
    void stopRecord();
    /** @brief 往当前录像追加一帧。优先 MP4，失败时写 AVI */
    void appendRecord(const CommBytes& jpeg, uint32_t width, uint32_t height);
    /** @brief 把对方剪贴板里的文件逐个拉到临时目录，齐了再放到本机剪贴板 */
    void pullClipFiles(const QString& listing);
    /** @brief 上一个剪贴板文件传完后发下一个 */
    void advanceClipPull(const ControlSession::Tile& tile);
    /** @brief 临时目录里的文件放进本机剪贴板 */
    void publishClipFiles();
    /** @brief 持有操作权时按顺序按下、逆序抬起一组虚拟键 */
    void sendCombo(const uint16_t* keys, int count);
    /** @brief 按 m_online、m_tile 刷新按钮、画面提示、状态栏和会话信息 */
    void syncControls();
    /** @brief 在画面上显示 3 秒提示 */
    void flash(const QString& text);

    ControlSession* m_session;      ///< 收发会话，不拥有
    const ControlConfig* m_config;  ///< 控制端配置，不拥有
    AuditLog* m_audit;              ///< 审计日志，不拥有
    QString m_key;                  ///< 设备库 key
    QString m_name;                 ///< 设备显示名，refresh() 更新
    ControlSession::Tile m_tile;    ///< 最近一次状态快照，不含 JPEG
    bool m_online;                  ///< 设备在线（有快照、online 且有连接 ID）
    bool m_wasOnline;               ///< 上次 refresh 的在线状态，用于记连接/离线事件
    bool m_wasOperating;            ///< 上次 refresh 的操作状态，用于记取得/释放事件
    std::string m_lastHint;         ///< 上次的提示文字，变化时记事件
    uint64_t m_seen;                ///< 已显示的画面帧序号（pictureSeq）
    uint64_t m_thumbSeen;           ///< 主码流还没解出来时，用过的缩略图序号
    int m_monitorCount;             ///< 显示器按钮数，-1 未建
    int m_monitorShare;             ///< 建显示器按钮时的共享种类，-1 未建
    bool m_full;                    ///< 处于全屏
    QString m_flash;                ///< 临时提示文字，空为无
    qint64 m_flashUntil;            ///< 临时提示截止时刻（epoch 毫秒）
    double m_fps;                   ///< 帧率，由 ControlWindow 统计后传入
    double m_kbps;                  ///< 下行码率 kbps，由 ControlWindow 统计后传入

    QWidget* m_toolbar;             ///< 顶部工具条
    QWidget* m_panel;               ///< 右侧面板，宽 300
    QWidget* m_status;              ///< 底部状态栏
    QWidget* m_floatBar;            ///< 全屏浮条
    FrameView* m_view;              ///< 画面区，负责显示与键鼠采集
    QPushButton* m_operate;         ///< 工具条上的申请/释放操作按钮
    QPushButton* m_floatOperate;    ///< 浮条上的申请/释放操作按钮
    SegmentBar* m_monitors;         ///< 显示器选择，末项「全部」暂不可用
    SegmentBar* m_fitSeg;           ///< 缩放方式：0 适应，1 原始 1:1
    QComboBox* m_quality;           ///< 画质档位：流畅 / 均衡 / 清晰
    QComboBox* m_codec;             ///< 发送编码：沿用 / JPEG / H.264 / 原图
    QSpinBox* m_maxFps;             ///< 帧率上限 5–500
    QPushButton* m_win;             ///< 发送 Win 键
    QPushButton* m_altTab;          ///< 发送 Alt+Tab
    QPushButton* m_chat;            ///< 显示/隐藏右侧面板
    QPushButton* m_shot;            ///< 截图
    QPushButton* m_record;          ///< 录屏
    QPushButton* m_privacy;         ///< 隐私屏
    QLineEdit* m_chatEdit;          ///< 聊天输入
    QComboBox* m_chatKind;          ///< 0 聊天，1 弹窗
    uint64_t m_clipSeen;            ///< 已应用到本机的剪贴板序号
    uint64_t m_cursorSeen;          ///< 已画过的光标形状序号
    QStringList m_clipQueue;        ///< 还没拉完的对方剪贴板文件
    QStringList m_clipSaved;        ///< 已经落到临时目录的文件
    uint32_t m_clipPullId;          ///< 当前剪贴板文件传输号
    bool m_clipPulling;             ///< 正在拉一个剪贴板文件
    bool m_clipSawBusy;             ///< 已经看见「正在传输」，避免把上一轮的完成当成这一轮
    uint64_t m_chatSeen;            ///< 已显示的聊天序号
    bool m_recording;               ///< 正在录屏
    bool m_recordArmed;             ///< 这次操作权已经尝试过自动录屏
    Mp4Record m_mp4;                ///< H.264 录像。失败时改用 m_avi
    FILE* m_avi;                    ///< MJPEG AVI，MP4 不可用时使用
    uint32_t m_aviFrames;           ///< 已写入的 AVI 帧数
    uint32_t m_aviW;                ///< AVI 宽
    uint32_t m_aviH;                ///< AVI 高
    std::vector<uint32_t> m_aviSizes; ///< 每帧字节数，关文件时回写索引
    qint64 m_aviLast;               ///< 上一帧写入时刻，用来限帧

    QTabBar* m_tabs;                ///< 面板页签
    QStackedWidget* m_pages;        ///< 面板页：聊天、会话信息、快捷键
    QVBoxLayout* m_events;          ///< 会话事件列表，末尾是伸展项
    QLabel* m_viewerCount;          ///< 在线人数
    QVBoxLayout* m_extraViewers;    ///< 被邀请的其他中控台
    uint64_t m_viewerSeen;
    TagLabel* m_meTag;              ///< 本人的权限标签：观看 / 操作中
    QLabel* m_siteName;             ///< 现场一行的名称

    QLabel* m_infoName;             ///< 信息页：设备
    QLabel* m_infoId;               ///< 信息页：设备 ID
    QLabel* m_infoAddress;          ///< 信息页：地址
    QLabel* m_infoSystem;           ///< 信息页：系统
    QLabel* m_infoShare;            ///< 信息页：共享
    QLabel* m_infoImage;            ///< 信息页：画面尺寸
    QLabel* m_infoFps;              ///< 信息页：帧率
    QLabel* m_infoRate;             ///< 信息页：下行码率
    QLabel* m_infoRtt;              ///< 信息页：延迟
    QLabel* m_infoBytes;            ///< 信息页：已接收 MB
    QLabel* m_infoUptime;           ///< 信息页：在线时长
    TagLabel* m_infoCrypto;         ///< 信息页：加密状态
    QLabel* m_infoFingerprint;      ///< 信息页：对端证书指纹

    StatusDot* m_stDot;             ///< 状态栏：在线状态点
    QLabel* m_stState;              ///< 状态栏：已连接 / 已断开
    QLabel* m_stImage;              ///< 状态栏：画面尺寸与帧率
    QLabel* m_stRtt;                ///< 状态栏：延迟，按阈值着色
    QLabel* m_stRate;               ///< 状态栏：下行码率
    QLabel* m_stPerm;               ///< 状态栏：当前权限

    std::function<void()> m_back;                   ///< 「监控墙」回调，可空
    std::function<void(const QString&)> m_closed;   ///< 关闭回调，可空
};

}

#endif
