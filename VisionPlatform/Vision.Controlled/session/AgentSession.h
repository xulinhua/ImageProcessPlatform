/**
 * @file AgentSession.h
 * @brief 被控端连出、送画面、接收操作。操作申请可按策略交给现场确认
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_AGENT_SESSION_H
#define VISION_CONTROLLED_AGENT_SESSION_H

#include "AgentAudit.h"
#include "DirectListen.h"
#include "H264Codec.h"
#include "Messages.h"
#include "RemoteAuth.h"
#include "RemoteBeacon.h"
#include "RemoteIdentity.h"
#include "RemoteTcp.h"
#include "DxgiCapture.h"
#include "WindowCapture.h"
#include <stdio.h>
#include <utility>
#include "WindowInput.h"
#include "WindowWatch.h"

class QImage;
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Vision {

/**
 * @class AgentSession
 * @brief 被控端会话：反连控制端，抓屏发 JPEG，收键鼠输入并注入，处理操作申请与现场确认
 * @note 内部线程：抓屏、可选编码流水线、收包与重连、发包；
 *       共享状态都由 m_mu 保护。界面线程只通过公有方法轮询和下指令
 */
class WatchLink;

class AgentSession {
    friend class WatchLink;
public:
    /** @brief 来访确认：从不、信任的控制端免确认、每次都确认 */
    enum Confirm { ConfirmNever = 0, ConfirmUntrusted = 1, ConfirmAlways = 2 };

    /** @brief 文件传输窗口轮询用的进度快照（被控端发起时） */
    struct FileUi {
        bool online;                ///< 已连上中控台
        std::string dirList;        ///< 中控台回的目录列表
        uint64_t dirSeq;            ///< 目录列表序号
        uint64_t fileSeq;           ///< 一次传输结束时加一
        uint64_t fileGot;           ///< 已传字节
        uint64_t fileBytes;         ///< 总字节，未知为 0
        std::string fileNote;       ///< 状态文字
        FileUi()
            : online(false)
            , dirSeq(0)
            , fileSeq(0)
            , fileGot(0)
            , fileBytes(0)
        {}
    };

    /** @brief 界面轮询用的连接快照 */
    struct Info {
        std::string status;         ///< 状态文字，如“未连接”“只看”“操作中”“本地暂停”“已断开”
        bool connected;             ///< TCP 已连上控制端
        bool suspended;             ///< 现场手动断开，resume 前不重连
        bool operating;             ///< 控制端持有操作权
        bool localPause;            ///< 现场正在用鼠标，远程输入被暂时挡住
        bool privacy;               ///< 隐私屏开着
        std::string controller;     ///< 控制端显示名（来自 HelloAck），可空
        std::string host;           ///< 反连的控制端地址
        int port;                   ///< 反连的控制端端口
        uint64_t connectedMs;       ///< 本次连接已持续 ms，未连接为 0
        uint64_t operateMs;         ///< 本次操作已持续 ms，未在操作为 0
        /** @brief 全部置为未连接 */
        Info() : connected(false), suspended(false), operating(false), localPause(false), privacy(false), port(0), connectedMs(0), operateMs(0) {}
    };

    /** @brief 等现场确认的操作申请 */
    struct Pending {
        bool active;                ///< 是否有申请在等确认
        uint64_t seq;               ///< 申请序号，每次新申请加 1，答复时用来对号
        std::string controller;     ///< 申请方显示名，可空
        std::string host;           ///< 申请方地址（即反连的控制端地址）
        int secondsLeft;            ///< 剩余秒数，向上取整；无申请为 0
        uint32_t requestPerms;      ///< 申请中的权限位
        /** @brief 无申请 */
        Pending() : active(false), seq(0), secondsLeft(0), requestPerms(PermView) {}
    };

    static const int kConfirmMs = 30000;    ///< 现场确认的等待时长 ms，超时回 AuthAck code 4

    /************************************************** 生命周期 **************************************************/
    /** @brief 读好设备 ID 与系统版本，不启动线程 */
    AgentSession();
    /** @brief 停止全部线程并断开 */
    ~AgentSession();

    /**
     * @brief 先 stop，再按新设置起抓屏、收包、发包三条线程和扫描应答；连接在收包线程里异步建立
     * @param host 控制端地址
     * @param port 控制端端口
     * @param title 进程画面时的窗口标题子串
     * @param processName 进程画面时的进程 exe 名
     * @param desktop true 共享整个桌面，false 共享进程画面
     * @param monitor 整个桌面时的显示器序号，从 0 起
     * @param err 目前不写
     * @return 目前恒为 true
     */
    bool start(const std::string& host, int port, const std::wstring& title, const std::wstring& processName,
        bool desktop, int monitor, std::string* err);
    /** @brief 停线程、断开连接、停窗口查找和扫描应答，阻塞到线程退出；有待发 Bye 时先尽力发出 */
    void stop();

    /************************************************** 状态 **************************************************/
    /** @brief 当前状态文字 */
    std::string status() const;
    /** @brief 当前连接快照，供界面定时刷新 */
    Info info() const;
    /**
     * @brief 中控台发来、界面还没取走的消息
     * @note kind 0 记在会话记录里，1 还要弹窗
     */
    struct Notice {
        std::string name;
        std::string text;
        uint8_t kind;
    };
    /** @brief 取出并清空待显示的消息 */
    void takeNotices(std::vector<Notice>* out);
    /** @brief 最近一帧的 JPEG 与图像尺寸，可能为空 */
    CommBytes preview(uint32_t* imageW, uint32_t* imageH) const;
    /**
     * @brief 控制端下发过共享范围且界面还没取时返回 true 并带出新设置，取后清标志
     * @note 各输出参数可空
     */
    bool takePushed(bool* desktop, int* monitor, std::wstring* title, std::wstring* process);

    /************************************************** 安全 **************************************************/
    /**
     * @brief 设置来访确认策略
     * @param confirm 取 Confirm 的值
     * @param trusted 信任的控制端显示名（UTF-8），ConfirmUntrusted 时免确认
     * @param lockOnEnd 被操作过的会话由对方结束或断线时锁定本机
     */
    void setPolicy(int confirm, const std::vector<std::string>& trusted, bool lockOnEnd);
    /** @brief 允许远端的权限，Perm* 按位或。观看始终保留。关掉操作会立刻收回键鼠 */
    void setAllow(uint32_t mask);
    /**
     * @brief 允许最高权限：对中控台开放全部权限，跳过现场确认/工站拦截；中控台 AuthMethodHighest 始终放行
     */
    void setAllowHighest(bool on);
    /** @brief 当前是否允许这一项。bit 取 PermOperate 等 */
    bool allows(uint32_t bit) const;
    /** @brief 允许远端的权限掩码，含观看 */
    uint32_t allowMask() const;
    /** @brief 文件传输允许的根目录。空=全部盘符；非空=仅这些盘或目录 */
    void setFileRoots(const std::vector<std::wstring>& roots);
    /**
     * @brief 抓屏 / 编码流水线。0=抓屏线程内联编码，1=独立编码线程（抓/编/发重叠）
     * @note JPEG 编码不可重入，最多 1；改完下次 start 生效
     */
    void setPipelineWorkers(int workers);
    /** @brief 当前流水线并行度 */
    int pipelineWorkers() const;
    /**
     * @brief 主码流编码：0 自动(优先 H.264)，1 JPEG，2 H.264，3 原图 BGRA
     * @note 中控台 Subscribe/Config 带 codec≠0 时会覆盖；本地设置作默认
     */
    void setPreferCodec(int codec);
    /** @brief 当前主码流编码偏好 */
    int preferCodec() const;
    /** @brief 文件传输窗口轮询 */
    FileUi fileUi() const;
    /** @brief 向中控台要目录列表。空路径列盘符 */
    void requestRemoteDir(const std::string& path);
    /** @brief 在中控台建目录 */
    void requestRemoteMkDir(uint32_t id, const std::string& remotePath);
    /**
     * @brief 本机文件推到中控台。返回 SHA-256，打不开为空
     * @note 须已连接且允许文件；与中控台侧上传对称
     */
    std::string armPushToRemote(uint32_t id, const std::string& localPath, const std::string& remotePath);
    /**
     * @brief 从中控台拉文件到本机。须在发出前调用，resume 为续传偏移
     */
    void armPullFromRemote(uint32_t id, const std::string& localPath, const std::string& remotePath,
        uint64_t total, uint64_t resume);
    /** @brief 中控台取得操作权后是否必须录屏 */
    void setForceRecord(bool on);
    /** @brief 打开后抓屏线程额外留一份 JPEG，给局域网网页看 */
    void setHttpView(bool on);
    /** @brief 允许控制端连入本机端口。port 默认 19102 */
    bool setDirect(bool on, int port, std::string* err);
    /** @brief 反连失败时改走中继。口令须与中继、中控台一致 */
    void setRelay(bool on, const std::string& host, int port, const std::string& token);
    /** @brief 生产保护：操作必须现场确认。工站文件 block 为真时直接拒绝操作 */
    void setProduction(bool on);
    /**
     * @brief 直连监听接到一条连接。已有反连时返回 false，调用方关掉套接字
     */
    bool offerInbound(uintptr_t sock, const std::string& remote);
    /** @brief 复制当前网页用的 JPEG。还没有画面时返回 false */
    bool copyHttpJpeg(CommBytes* jpeg, uint64_t* seq) const;
    /** @brief IP/主机白名单；空表示不限制。反连时校验控制端 host */
    void setWhitelist(const std::vector<std::string>& allow);
    /** @brief 设置或清除无人值守密码；返回 salt:hash 供配置持久化，空串表示已清除 */
    std::string setPassword(const std::string& password);
    /** @brief 从配置装入密码哈希 */
    void loadPasswordHash(const std::string& stored);
    /** @brief 当前一次性验证码（过期自动刷新） */
    std::string currentCode();
    /** @brief 手动刷新验证码 */
    std::string refreshCode();
    /** @brief 是否已设无人值守密码 */
    bool hasPassword() const;
    /** @brief 本机 TLS 证书指纹；未就绪时退回设备 ID 派生值 */
    std::string fingerprint() const;
    /** @brief 当前对端（控制端）证书指纹 */
    std::string peerFingerprint() const;
    /** @brief 是否已启用 TLS */
    bool tlsReady() const;
    /** @brief 当前等确认的申请快照 */
    Pending pending() const;
    /** @brief 进程画面键鼠：0 PostMessage，1 前台 SendInput */
    void setInjectMode(int mode);
    /** @brief 主控制台之外还要尝试反连的 host:port 列表 */
    void setExtraHosts(const std::vector<std::pair<std::string, int> >& hosts);
    /** @brief 向当前中控台发一条聊天；name 空则用本机名 */
    void sendChat(const std::string& text, const std::string& name = std::string());
    /** @brief 现场的答复。seq 不是当前申请时忽略；allow 时 perms 为授予的权限位 */
    void answerAuth(uint64_t seq, bool allow, uint32_t perms = PermView);
    /** @brief 现场收回操作，控制端回到只看 */
    void pauseOperate();
    /** @brief 断开当前控制端，直到 resume 才重连 */
    void disconnect();
    /** @brief 清除手动断开，收包线程随后重连；此时已没有连接则撤销待发的 Bye */
    void resume();

    /************************************************** 本机 **************************************************/
    /** @brief 9 位数字设备 ID，形如“123 456 789” */
    const std::string& deviceId() const;
    /** @brief 系统版本，形如“Windows 11 23H2” */
    const std::string& systemName() const;

private:
    /************************************************** 线程 **************************************************/
    /** @brief 收包线程：断线时重连，判心跳超时，按类型处理控制端报文，申请超时时回 code 4 */
    void readMain();
    /** @brief 发包线程：有积压不空等；按需发 Bye、Hello、心跳、新画面和待发队列 */
    void writeMain();
    /** @brief 抓屏线程：抓 BGRA；流水线关闭时当场编码，打开时交给编码线程 */
    void captureMain();
    /** @brief 编码线程：取最新一帧 BGRA 编码并写入 m_picture */
    void encodeMain();

    /************************************************** 会话 **************************************************/
    /** @brief 放入待发队列，满 64 条时丢最旧的；空帧忽略 */
    void enqueue(const CommBytes& frame);
    /** @brief 用当前共享设置刷新局域网扫描的应答内容 */
    void publishBeacon();
    /**
     * @brief 应用控制端下发的共享范围与画质：收回操作权、切换窗口查找、标记重发 Hello 并通知界面
     * @note maxFps / jpegQuality / longEdge 为 0 时保留当前值
     */
    void applyControl(const ConfigMsg& cfg);
    /** @brief 码流订阅：主/子码流开关与编码偏好。调用方不必持锁 */
    void applySubscribe(const SubscribeMsg& msg);
    /** @brief 网络统计：延迟高时略降分辨率/帧率，平稳后再收回 */
    void applyNetStats(const NetStatsMsg& msg);
    /** @brief 剪贴板 / 文件 / 聊天 / 电源 / 隐私屏 / 系统信息。处理了返回 true */
    bool takePhase4(const Frame::Packet& packet);
    /** @brief 写线程：推剪贴板变化、系统信息、文件块。调用方不持锁 */
    void pumpPhase4(std::vector<CommBytes>* extra);
    /** @brief 剪贴板图片的像素散列。本机和中控台共用剪贴板时，写回去不能再当成新图 */
    uint64_t hashClipImage(const QImage& image) const;
    /** @brief 绝对路径且不含 .. 。空路径表示列出盘符 */
    bool pathAllowed(const std::wstring& path) const;
    /** @brief 把路径收成绝对路径并解开符号链接，失败时退回 GetFullPathName 的结果 */
    std::wstring canonicalPath(const std::wstring& path) const;
    /** @brief 主码流 H.264，失败返回 false */
    bool encodePicture(const CommBytes& bgra, int width, int height, int quality, int longEdge, int fps, PictureMsg* shot);
    /** @brief 主码流原图 BGRA（按 longEdge 缩放） */
    bool encodeRaw(const CommBytes& bgra, int width, int height, int longEdge, PictureMsg* shot);
    /**
     * @brief 按 m_preferCodec 编码一帧。成功写入 shot
     * @return false 表示本帧跳过（H.264 预热中）
     */
    bool encodeByPrefer(const CommBytes& bgra, int width, int height, int quality, int longEdge, int fps,
        PictureMsg* shot, const char** pathName);
    /** @brief 光标形状变了才组一包，没变返回空。约 100ms 节流 */
    CommBytes captureCursorShape();
    /** @brief 把抓到的 BGRA 交给编码流水线；workers=0 时不调用 */
    void publishRaw(CommBytes& bgra, int width, int height, int originX, int originY, bool desktop,
        bool operable, bool forcePublish, uint32_t shareGen, int jpegQuality, int longEdge, int captureMs,
        bool subMain, bool subThumb, uint64_t started, uint64_t grabMs);
    /**
     * @brief 编码完成后写入主画面 / 子码流 / 状态。调用方不持锁
     * @note desktop 分支会更新显示器原点；window 分支 operable 进 PictureMsg
     */
    void commitEncoded(PictureMsg& shot, PictureMsg* thumb, bool haveThumb, const CursorPosMsg* cursor,
        bool haveCursor, int originX, int originY, bool desktop, bool forcePublish, uint32_t shareGen,
        bool subMain, int captureMs, uint64_t started, const char* pathName, uint64_t grabMs,
        uint64_t encodeMs, uint64_t previewMs, uint64_t thumbMs);
    /** @brief 网页查看开着时，把这一帧 JPEG 留一份。调用方不必持锁 */
    void publishHttp(const CommBytes& jpeg);
    /** @brief 网页查看开着时，把 BGRA 压成一帧较小的 JPEG */
    void publishHttpBgra(CommBytes& bgra, int width, int height);
    /** @brief 留一张 JPEG 给本机预览。主码流可能是 H.264，界面不能拿它当 JPEG */
    void keepPreview(const PictureMsg& jpegShot);
    /**
     * @brief 本机预览：BGRA 压成小 JPEG。约 250 ms 一次，避免跟主码流抢 CPU
     * @note 只在抓屏线程调用；nowMs 用 AgentSession::nowMs()
     */
    void refreshLocalPreview(CommBytes& bgra, int width, int height, uint64_t nowMs);
    /** @brief 取出一条待接入的直连套接字并挂到 m_tcp。没有则返回 false */
    bool takeInbound();
    /** @brief 连中继并完成登记。成功后套接字留在 m_tcp，调用方再做 TLS */
    bool openViaRelay(std::string* err);
    /** @brief 读工站状态文件，生产中时强制确认 */
    void refreshStation();
    /** @brief 拥塞时先降质量，再降帧率，再降分辨率。调用方须持有 m_mu */
    void stepAdapt(bool bad, uint64_t now);
    /** @brief 列出目录。空路径返回根目录。不允许的路径返回空 */
    std::string listDirectory(const std::wstring& path) const;
    /** @brief 逐级创建目录。须在允许的根下，目录已存在也算成功 */
    bool makeDirectory(const std::wstring& path) const;
    /** @brief CPU、内存、磁盘，以及工站一句 */
    std::string systemText() const;
    /** @brief 进程列表，前缀 PROCESSES\\n，供会话独立进程页 */
    std::string processListText() const;
    /** @brief 文件 SHA-256 小写十六进制。打不开返回空 */
    std::string sha256File(const std::wstring& path) const;
    /** @brief 再反连一台中控台，只看；操作权要等交接 */
    /** @brief 停掉受邀观看的连接 */
    void stopWatches();
    /** @brief 把观看端排队的键鼠应用到本机。只在读线程调用 */
    void drainViewerInputs();
    /** @brief 再连一台中控台，只看 */
    void inviteViewer(const std::string& host, int port);
    /** @brief 把操作权交给这台中控台的证书指纹 */
    void handoffTo(const std::string& fingerprint);
    /** @brief 这台中控台是否拿着操作权 */
    bool viewerMayOperate(const std::string& fingerprint) const;
    /** @brief 观看者名单，一行一台 */
    std::string viewerReport() const;
    /** @brief 观看端投递一笔键鼠，读线程稍后应用 */
    void submitViewerInput(const std::string& fingerprint, const InputMsg& input);
    /** @brief 把一笔观看端键鼠注入本机 */
    void applyViewerInput(const std::string& fingerprint, const InputMsg& input);
    /** @brief 复制当前主画面。观看线程用 */
    PictureMsg pictureCopy() const;
    /** @brief 主画面序号 */
    uint64_t pictureSeqCopy() const;
    /** @brief 复制当前光标位置 */
    CursorPosMsg cursorCopy() const;
    /** @brief 填一份 Hello，给受邀的中控台 */
    bool fillHello(HelloMsg* hello) const;
    /** @brief 受邀连接做 TLS 客户端握手 */
    bool openViewerTls(RemoteTcpClient* tcp, std::string* peerFp, std::string* err);
    /** @brief 把 maxFps 钳到 [kMaxFpsMin, kMaxFpsMax]，并换算成抓屏间隔 ms */
    static int captureMsFromFps(int maxFps);
    /** @brief 按策略与控制端显示名判断这次申请是否要现场确认。调用方须持有 m_mu */
    bool needConfirm() const;
    /**
     * @brief 清掉会话状态（操作权、申请、控制端名、半包）
     * @param remote 由对方结束或断线时为 true；为 true 且本次被操作过、开了锁屏时锁定本机
     * @note 调用方须持有 m_mu
     */
    void endSession(bool remote);

    /************************************************** 工具 **************************************************/
    /** @brief 本机计算机名（UTF-8），取不到为“pc” */
    std::string machineName() const;
    /** @brief 把 MachineGuid（取不到用计算机名）做 FNV-1a 散列，取 9 位并按 3 位分组 */
    static std::string readDeviceId();
    /** @brief 从注册表读产品名与版本号；内部版本 >= 22000 时把“Windows 10”改成“Windows 11” */
    static std::string readSystemName();
    /** @brief 宽字符转 UTF-8，超过 64 字节时在字符边界截断 */
    static std::string toUtf8(const std::wstring& text);
    /** @brief 不截断的 UTF-8，给路径、目录和剪贴板用 */
    static std::string utf8Full(const std::wstring& text);
    /** @brief UTF-8 转宽字符 */
    static std::wstring toWide(const std::string& text);
    /** @brief 开机以来的毫秒数（GetTickCount64） */
    uint64_t nowMs() const;

    RemoteTcpClient m_tcp;          ///< 反连控制端的 TCP 连接，收包线程负责打开
    RemoteBeacon m_beacon;          ///< 局域网扫描应答
    Frame m_frame;                  ///< 收包拆帧器，只在收包线程用，断线时清半包
    WindowWatch m_watch;            ///< 进程画面时查找目标窗口
    WindowCapture m_capture;        ///< 抓屏与 JPEG 编码，只在抓屏线程用
    DxgiCapture m_dxgi;             ///< 桌面 DXGI；失败时改走 m_capture
    H264Codec m_h264;               ///< 主码流硬编，失败再 OpenH264，再不行 JPEG
    AgentAudit m_audit;             ///< 被控端本地审计
    WindowInput m_input;            ///< 键鼠注入与松开，只在收包线程用（stop 在线程退出后再松开一次）；抓屏线程只调无状态的 operable
    mutable std::mutex m_mu;        ///< 保护以下全部会话状态
    std::thread m_read;             ///< 收包线程
    std::thread m_write;            ///< 发包线程
    std::thread m_captureThread;    ///< 抓屏线程
    std::vector<std::thread> m_encodeThreads; ///< 编码流水线，条数由 m_pipelineWorkers 决定
    bool m_run;                     ///< 线程是否继续，stop 时置 false
    int m_pipelineWorkers;          ///< 0=内联编码，1=独立编码线程
    int m_preferCodec;              ///< 0 自动，1 JPEG，2 H.264，3 原图
    std::string m_host;             ///< 控制端地址
    int m_port;                     ///< 控制端端口
    std::vector<std::pair<std::string, int> > m_extraHosts; ///< 额外反连目标
    size_t m_connectTry;            ///< 收包线程重连时轮询的目标下标
    std::string m_status;           ///< 状态文字，界面显示用；“本地暂停”同时作为 localPause 的判断依据
    bool m_desktop;                 ///< true 共享整个桌面，false 共享进程画面
    std::string m_source;           ///< 共享源显示文字，进 Hello 与扫描应答
    std::wstring m_title;           ///< 进程画面的窗口标题子串
    std::wstring m_process;         ///< 进程画面的进程 exe 名
    int m_monitor;                  ///< 整个桌面时的显示器序号
    int m_originX;                  ///< 当前显示器左上角在虚拟屏幕上的横坐标，抓屏线程更新
    int m_originY;                  ///< 当前显示器左上角在虚拟屏幕上的纵坐标，抓屏线程更新
    int m_captureMs;                ///< 抓屏间隔 ms，由 maxFps 换算；抓屏线程每帧末尾 Sleep
    int m_jpegQuality;              ///< JPEG 质量 1–100，主码流
    int m_longEdge;                 ///< 主码流长边上限
    int m_targetBitrateBps;         ///< 目标码率（比特/秒），0 不限；兼作发送侧带宽封顶
    uint64_t m_sendWindowStart;     ///< 发送窗口起点（ms）
    size_t m_sendWindowBytes;       ///< 本窗口已发送字节
    bool m_subMain;                 ///< 已订阅主码流；未收到 Subscribe 前保持开，兼容旧控制端
    bool m_subThumb;                ///< 已订阅子码流（监控墙）
    uint32_t m_shareGen;            ///< 共享源变化计数。抓屏线程看到增加就补一帧关键帧
    uint32_t m_shareSeen;           ///< 抓屏线程已处理到的 m_shareGen，只在抓屏线程写
    bool m_mainLatched;             ///< 这次主码流订阅是否已经送出过一帧。只在抓屏线程写
    int m_baseCaptureMs;            ///< 订阅下来的间隔，拥塞时在此基础上拉长
    int m_baseQuality;              ///< 订阅下来的 JPEG 质量，自适应只在此以下浮动
    int m_baseEdge;                 ///< 订阅下来的长边，自适应只在此以下浮动
    int m_rttBase;                  ///< 近期往返基线，毫秒
    uint64_t m_lastAdapt;           ///< 上次升降码率的时刻
    uint8_t m_sentCodec;            ///< 已通知控制端的编码
    uint32_t m_streamW;             ///< 已通知的画面宽
    uint32_t m_streamH;             ///< 已通知的画面高
    uint64_t m_cursorHash;          ///< 上一帧光标像素散列，形状没变不重发
    uint64_t m_lastCursorShapeMs;   ///< 上次抓光标形状的时刻，节流用
    std::vector<std::wstring> m_roots; ///< 文件根目录
    std::string m_dirList;          ///< 中控台回的目录列表，给本机文件窗
    uint64_t m_dirSeq;              ///< 目录列表序号
    uint64_t m_fileUiSeq;           ///< 本机发起的传输结束序号
    uint64_t m_fileUiGot;           ///< 本机发起传输已传字节
    uint64_t m_fileUiBytes;         ///< 本机发起传输总字节
    std::string m_fileUiNote;       ///< 本机发起传输状态
    bool m_forceRecord;             ///< Hello 里带给中控台，取得操作后自动录屏
    bool m_httpView;                ///< 局域网网页查看
    CommBytes m_httpJpeg;           ///< 网页用的 JPEG，和主码流分开
    CommBytes m_previewJpeg;        ///< 本机预览 JPEG。主码流为 H.264 时不能用 m_picture.jpeg
    uint32_t m_previewW;            ///< 预览宽
    uint32_t m_previewH;            ///< 预览高
    uint64_t m_httpSeq;             ///< 网页画面序号
    DirectListen m_direct;          ///< 直连监听
    uintptr_t m_inbound;            ///< 待接管的直连套接字，0 表示没有
    std::string m_inboundRemote;    ///< 直连对端
    bool m_relayOn;                 ///< 反连失败后走中继
    std::string m_relayHost;        ///< 中继地址
    int m_relayPort;                ///< 中继端口
    std::string m_relayToken;       ///< 中继口令
    bool m_production;              ///< 生产保护：操作必须现场确认
    bool m_allowHighest;            ///< 允许中控台最高权限（满权限、免确认）
    bool m_stationBlock;            ///< 工站文件要求禁止操作
    std::string m_stationText;      ///< 工站状态，拼进系统信息
    PictureMsg m_thumb;             ///< 子码流 JPEG
    uint64_t m_thumbSeq;            ///< 子码流帧序号
    uint64_t m_thumbSent;           ///< 已发出的子码流序号
    uint64_t m_lastThumbMs;         ///< 上次压子码流的时刻
    uint64_t m_lastPreviewMs;       ///< 上次本机预览 JPEG 的时刻
    CursorPosMsg m_cursor;          ///< 最新光标（图像坐标）
    bool m_cursorDirty;             ///< 光标待发
    bool m_privacy;                 ///< 隐私屏
    std::string m_clipText;         ///< 最近一次剪贴板文本，避免把自己写入的内容再发出去
    std::vector<Notice> m_notices;  ///< 中控台消息，界面取走后清空，最多留 32 条
    uint64_t m_clipImageHash;       ///< 最近一次收发的图片像素散列，相同则不再发
    uint32_t m_clipSeq;             ///< GetClipboardSequenceNumber
    bool m_filePull;                ///< 正在把本机文件发给控制端
    uint32_t m_fileId;              ///< 当前文件传输号
    uint32_t m_fileOff;             ///< 已写入的偏移
    uint32_t m_fileAcked;           ///< 对方已确认的偏移
    uint32_t m_fileNext;            ///< 下一块要发的偏移
    uint64_t m_fileSize;            ///< 文件总字节
    std::string m_fileSha;          ///< SHA-256 十六进制
    std::wstring m_filePath;        ///< 本地路径
    FILE* m_file;                   ///< 正在写的文件，空表示没打开
    bool m_fileStop;                ///< 断线后由写线程关掉文件
    std::mutex m_fileMu;            ///< 保护文件传输进度
    std::string m_grantFp;          ///< 交接后允许操作的观看端指纹；空表示主连接自己操作
    std::vector<WatchLink*> m_watches; ///< 受邀的中控台连接
    mutable std::mutex m_watchMu;   ///< 保护 m_watches 与观看端输入队列
    std::vector<std::pair<std::string, InputMsg> > m_viewerInput; ///< 观看端排队的键鼠，指纹在前
    bool m_operating;               ///< 控制端当前持有操作权
    bool m_operated;                ///< 本次连接是否曾被操作过，决定断开时是否锁屏
    bool m_needHello;               ///< 发包线程下一轮要发 Hello（新连接或共享范围变了）
    bool m_pushed;                  ///< 控制端改了共享范围、界面还没同步，见 takePushed
    bool m_suspended;               ///< 现场手动断开，resume 前不重连
    bool m_releaseInput;            ///< 失去操作权（收回、Release、换共享源、会话结束、断开）后置位，收包线程下一轮松开已注入的键鼠
    bool m_wantBye;                 ///< 待发 Bye：连着时发包线程发出并断开，stop 时尽力发出；resume 时若没有连接则撤销
    uint64_t m_lastRx;              ///< 最近收到 HelloAck、心跳或心跳回应的时刻 ms，超过 kOfflineMs 判断线
    uint64_t m_connectedAt;         ///< 本次会话登记时刻 ms，0 为未登记；发包线程只在它非 0 时发 Hello、心跳、画面和队列
    uint64_t m_operateAt;           ///< 本次获得操作权的时刻 ms
    std::string m_controller;       ///< 控制端显示名，来自 HelloAck，断开时清空
    PictureMsg m_picture;           ///< 最新一帧，编码后写，发包线程与界面预览读
    uint64_t m_pictureSeq;          ///< 已编码帧数，每成功一帧加 1
    uint64_t m_sentSeq;             ///< 已发出的最新帧序号，与 m_pictureSeq 不等时发新帧
    std::vector<CommBytes> m_out;   ///< 待发报文队列（AuthAck、Release、HeartbeatAck 等），最多 64 条；登记新会话时清空

    /** @brief 抓屏交给编码线程的原始帧；只保留最新一帧 */
    struct RawFrame {
        CommBytes bgra;
        int width;
        int height;
        int originX;
        int originY;
        bool desktop;
        bool operable;
        bool forcePublish;
        uint32_t shareGen;
        int jpegQuality;
        int longEdge;
        int captureMs;
        bool subMain;
        bool subThumb;
        uint64_t started;
        uint64_t grabMs;
        RawFrame()
            : width(0)
            , height(0)
            , originX(0)
            , originY(0)
            , desktop(false)
            , operable(true)
            , forcePublish(false)
            , shareGen(0)
            , jpegQuality(0)
            , longEdge(0)
            , captureMs(0)
            , subMain(true)
            , subThumb(false)
            , started(0)
            , grabMs(0)
        {}
    };
    mutable std::mutex m_rawMu;     ///< 保护原始帧槽
    std::condition_variable m_rawCv; ///< 有新原始帧或停机
    RawFrame m_raw;                 ///< 最新原始帧
    uint64_t m_rawSeq;              ///< 原始帧序号，抓屏线程写
    uint64_t m_rawTaken;            ///< 编码线程已取到的序号

    int m_confirm;                      ///< 来访确认策略，取 Confirm 的值
    std::vector<std::string> m_trusted; ///< 信任的控制端显示名（UTF-8）
    bool m_lockOnEnd;                   ///< 被操作过的会话结束时锁定本机
    uint32_t m_allow;                   ///< 允许远端的权限，Perm* 按位或，观看始终在内
    bool m_pendingActive;               ///< 是否有申请在等现场确认
    uint64_t m_pendingSeq;              ///< 当前申请序号，每次新申请加 1
    uint64_t m_pendingDeadline;         ///< 当前申请的截止时刻 ms（nowMs 基准）

    std::string m_deviceId;         ///< 9 位数字设备 ID，构造时算好
    std::string m_system;           ///< 系统版本，构造时读好
    RemoteAuth m_auth;              ///< 验证码、密码、防爆破
    uint32_t m_pendingPerms;        ///< 等确认期间暂存对方申请的权限
    RemoteIdentity m_identity;      ///< 本机 TLS 身份
    std::vector<std::string> m_whitelist; ///< 允许的控制端 host
    std::string m_peerFingerprint;  ///< 当前控制端证书指纹
};

}

#endif
