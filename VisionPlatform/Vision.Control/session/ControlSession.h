/**
 * @file ControlSession.h
 * @brief 控制端监听多台。每台留最新画面，操作只发给单画面里的那一台
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_SESSION_H
#define VISION_CONTROL_SESSION_H

#include "H264Codec.h"
#include "Messages.h"
#include "RemoteIdentity.h"
#include "RemoteTcp.h"
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Vision {

/**
 * @class ControlSession
 * @brief 监听被控端反连，维护每台的状态与最新画面，转发操作申请、共享配置和键鼠输入
 * @note 内部一个读线程、一个写线程；公开接口可在界面线程调用，状态由 m_mu 保护。
 *       界面只能通过 tiles() 拿快照，不直接持有内部条目
 */
class ControlSession {
public:
    /** @brief 一台被控端的状态快照（界面侧只读） */
    struct Tile {
        std::string peerId;     ///< TCP 连接 ID；离线时清空
        std::string address;    ///< 对端 IP（不含端口）
        std::string bindKey;    ///< 绑定键 d:地址 或 w:地址\n共享源，收到 Hello 后才有；为空的条目离线即删
        std::string name;       ///< 被控端名称，UTF-8；收到 Hello 前是对端地址串
        std::string source;     ///< 共享源描述，UTF-8，窗口共享时有意义
        uint8_t share;          ///< 共享种类：1 桌面，其他为进程窗口
        uint8_t monitor;        ///< 共享的显示器序号，从 0 起
        std::string title;      ///< 共享窗口标题
        std::string process;    ///< 共享进程名
        std::string deviceId;   ///< 9 位设备 ID，旧版被控端为空
        std::string system;     ///< 被控端系统版本
        uint8_t monitors;       ///< 被控端显示器块数
        bool online;            ///< 连接在线
        bool operating;         ///< 本端持有操作权
        bool operable;          ///< 最新画面是否可操作（被控端报告）
        std::string hint;       ///< 给用户看的提示（申请中、被拒原因等），UTF-8，可空
        CommBytes jpeg;         ///< 主码流编码包（JPEG / Annex-B / 原图 BGRA）
        CommBytes thumb;        ///< 子码流 JPEG，监控墙优先用它
        /** @brief 收包线程已解出的 BGRA；H.264 必须逐帧解，界面只能拿最新一帧，用 shared_ptr 避免 tiles() 大拷贝 */
        std::shared_ptr<CommBytes> pixels;
        uint32_t imageW;        ///< 画面宽（像素）
        uint32_t imageH;        ///< 画面高（像素）
        uint32_t pixelsW;       ///< pixels 宽
        uint32_t pixelsH;       ///< pixels 高
        uint64_t pictureSeq;    ///< 主码流帧计数
        uint64_t thumbSeq;      ///< 子码流帧计数
        uint64_t pixelsSeq;     ///< 成功解出 BGRA 的计数；界面按它刷新
        uint16_t cursorX;       ///< 远程光标图像坐标
        uint16_t cursorY;
        uint8_t cursorOn;       ///< 1 光标在画面内
        uint8_t codec;          ///< 主码流 1 JPEG，2 H.264，3 原图 BGRA
        uint8_t forceRecord;    ///< 被控端要求取得操作后录屏
        CommBytes cursorShape;  ///< 光标 PNG
        uint16_t cursorHotX;
        uint16_t cursorHotY;
        uint16_t cursorShapeW;
        uint16_t cursorShapeH;
        uint64_t cursorShapeSeq;
        uint64_t bytesRx;       ///< 累计收到的画面负载字节数
        uint64_t onlineSince;   ///< 上线时刻，GetTickCount64 毫秒
        int rttMs;              ///< 心跳往返延迟 ms，-1 未知
        std::string clipText;   ///< 被控端剪贴板文本
        CommBytes clipPng;      ///< 被控端剪贴板 PNG
        std::string clipFiles;  ///< 被控端剪贴板文件路径，一行一个
        uint64_t clipSeq;       ///< 剪贴板变化计数
        std::string chatLine;   ///< 最近一条聊天
        uint64_t chatSeq;
        std::string sysInfo;    ///< CPU、内存、磁盘，以及工站文字
        std::string station;    ///< 从系统信息里拆出的工站一句，空表示没有
        bool stationNg;         ///< 工站信息里带 NG
        std::string dirList;    ///< 目录列表，一行：D/F/E、大小、名称、修改时间
        uint64_t dirSeq;
        std::string fileNote;   ///< 当前文件传输说明
        uint64_t fileGot;       ///< 已传输字节
        uint64_t fileBytes;     ///< 文件总字节，未知为 0
        uint64_t fileSeq;       ///< 一次传输结束时加一，文件窗口用来排队
        std::string viewers;    ///< 观看端，一行：名字、指纹、是否在操作
        uint64_t viewerSeq;
        /** @brief 离线、单显示器、延迟未知 */
        Tile()
            : share(0)
            , monitor(0)
            , monitors(1)
            , online(false)
            , operating(false)
            , operable(false)
            , imageW(0)
            , imageH(0)
            , pixelsW(0)
            , pixelsH(0)
            , pictureSeq(0)
            , thumbSeq(0)
            , pixelsSeq(0)
            , cursorX(0)
            , cursorY(0)
            , cursorOn(0)
            , codec(1)
            , forceRecord(0)
            , cursorHotX(0)
            , cursorHotY(0)
            , cursorShapeW(0)
            , cursorShapeH(0)
            , cursorShapeSeq(0)
            , bytesRx(0)
            , onlineSince(0)
            , rttMs(-1)
            , clipSeq(0)
            , chatSeq(0)
            , dirSeq(0)
            , fileGot(0)
            , fileBytes(0)
            , fileSeq(0)
            , viewerSeq(0)
            , stationNg(false)
        {}
    };

    /** @brief 未监听 */
    ControlSession();
    /** @brief 停止监听并等两个线程退出 */
    ~ControlSession();

    /************************************************** 监听 **************************************************/
    /** @brief 回给被控端的控制端显示名，超过 64 字节按字符截断 */
    void setSelfName(const std::string& name);
    /** @brief 是否在延迟很高时通知被控端降画质。关掉后帧率保持在会话上限 */
    void setAdaptive(bool on);
    /** @brief 无本地操作超过分钟数时断开全部会话，0 关闭 */
    void setIdleMinutes(int minutes);
    /** @brief 下行带宽上限 kbps，0 不限（会话下发时参考） */
    void setBandwidthKbps(int kbps);
    /** @brief 拨号顺序：reverse / direct / relay */
    void setConnectOrder(const std::vector<std::string>& order);
    /** @brief 记录中控台本地活动（键鼠、申请操作等） */
    void touchLocalActivity();
    /** @brief 空闲超时时关闭全部在线连接 */
    void disconnectIfIdle();
    /**
     * @brief 先 stop() 再开始监听，清空已知设备并启动读写线程
     * @param err 失败原因
     * @return 端口打开成功返回 true
     */
    bool start(const std::string& host, int port, std::string* err);
    /** @brief 停止监听、等线程退出，所有设备标为离线（条目保留） */
    void stop();
    /** @brief 是否在监听 */
    bool listening() const;
    /**
     * @brief 设备还没反连上来时，先直连地址 2 秒，再走中继
     * @param deviceId 被控端设备 ID
     */
    bool dialDevice(const std::string& address, const std::string& deviceId,
        const std::string& relayHost, int relayPort, const std::string& token, bool relayOn,
        const std::vector<std::string>& order, std::string* err);
    /** @brief 所有设备状态的拷贝，含最新 JPEG */
    std::vector<Tile> tiles() const;

    /************************************************** 操作 **************************************************/
    /**
     * @brief 向在线设备认证并申请操作权
     * @param method AuthMethod；token 为验证码或密码明文
     */
    void requestOperate(const std::string& peerId, uint8_t method = AuthMethodTrusted, const std::string& token = std::string());
    /** @brief 释放操作权，本地立即标为未操作 */
    void releaseOperate(const std::string& peerId);
    /** @brief 给在线设备下发共享配置（桌面/显示器/窗口），后发的覆盖未发出的 */
    void pushConfig(const std::string& peerId, const ConfigMsg& cfg);
    /** @brief 订阅或退订主码流 / 子码流 */
    void subscribe(const std::string& peerId, const SubscribeMsg& msg);
    /** @brief 把一帧放进该连接的发送队列 */
    void post(const std::string& peerId, const CommBytes& bytes);
    /** @brief 准备接收被控端发来的文件。须在发出 FileOpen 之前调用。total 为远端大小，未知传 0 */
    void armDownload(const std::string& peerId, uint32_t id, const std::string& localPath, uint64_t total = 0);
    /** @brief 准备把本机文件上传到被控端 */
    /** @return 本地文件的 SHA-256，打不开则为空 */
    std::string armUpload(const std::string& peerId, uint32_t id, const std::string& localPath, const std::string& remotePath);
    /** @brief 转发一条键鼠输入，只有持有操作权时才发 */
    void sendInput(const std::string& peerId, const InputMsg& msg);
    /** @brief 发送 Ctrl+Alt+Del 等特殊键 */
    void sendSpecial(const std::string& peerId, uint8_t kind);
    /** @brief 本机 TLS 证书指纹 */
    std::string fingerprint() const;
    /** @brief 指定连接的对端指纹 */
    std::string peerFingerprint(const std::string& peerId) const;

private:
    /** @brief 一台被控端的内部条目：拆帧缓冲、状态快照和待发数据 */
    struct Machine {
        Frame frame;                ///< 拆帧缓冲
        Tile tile;                  ///< 对外的状态
        uint64_t lastRx;            ///< 最近一次收包时刻 ms，超过 kOfflineMs 无数据判离线
        std::vector<CommBytes> out; ///< 待发包，最多 32 条，满了丢最旧的
        bool wantAuth;              ///< 待发操作申请
        bool wantRelease;           ///< 待发释放
        bool wantConfig;            ///< 待发共享配置
        bool wantSub;               ///< 待发订阅
        ConfigMsg config;           ///< 待下发的共享配置
        SubscribeMsg subscribe;     ///< 待下发的订阅
        AuthRequestMsg auth;        ///< 待发的认证请求
        std::string downPath;       ///< 正在接收的本地路径
        uint32_t downId;            ///< 正在接收的传输号
        bool upOn;                  ///< 正在向被控端上传
        std::string upLocal;        ///< 上传的本地路径
        std::string upRemote;       ///< 上传的远端路径
        uint32_t upId;              ///< 上传传输号
        uint32_t upOff;             ///< 已读出的偏移
        uint32_t upAcked;           ///< 对方已确认的偏移
        uint32_t upNext;            ///< 下一块偏移
        uint64_t upSize;            ///< 上传总字节
        std::string upSha;          ///< 上传文件的 SHA-256
        bool upReady;               ///< 对方已接受 FileOpen
        bool upFinished;            ///< 已发完并收到完成
        /** @brief 主码流 H.264 解码器；vector 重分配时用 unique_ptr 避免浅拷贝 */
        std::unique_ptr<H264Codec> videoDec;
        uint8_t videoDecCodec;      ///< 解码器当前对应的 codec，切换时重建
        /** @brief 无待发数据 */
        Machine()
            : lastRx(0)
            , wantAuth(false)
            , wantRelease(false)
            , wantConfig(false)
            , wantSub(false)
            , downId(0)
            , upOn(false)
            , upId(0)
            , upOff(0)
            , upAcked(0)
            , upNext(0)
            , upSize(0)
            , upReady(false)
            , upFinished(false)
            , videoDecCodec(0)
        {}
        Machine(Machine&&) = default;
        Machine& operator=(Machine&&) = default;
        Machine(const Machine&) = delete;
        Machine& operator=(const Machine&) = delete;
    };

    /************************************************** 线程 **************************************************/
    /** @brief 读线程：同步连接列表、收包拆帧、处理各类消息、判离线 */
    void readMain();
    /** @brief 写线程：按 kHeartbeatMs 发心跳，发出待发标志和队列里的包 */
    void writeMain();

    /************************************************** 内部 **************************************************/
    /** @brief 包放进设备的待发队列；调用方持锁 */
    void enqueue(Machine* machine, const CommBytes& bytes);
    /** @brief 按连接 ID 找条目下标，没有返回 -1；调用方持锁 */
    int indexOfPeer(const std::string& peerId);
    /** @brief 从 ip:port 取出 ip 部分 */
    static std::string hostOf(const std::string& remote);
    /**
     * @brief 处理 Hello：生成绑定键，踢掉并删除同绑定键的旧条目，填设备信息并回 HelloAck
     * @note 调用方持锁
     */
    void adoptHello(const std::string& peerId, const HelloMsg& hello);
    /** @brief 剪贴板 / 文件 / 聊天 / 系统信息等。处理了返回 true。调用方持锁 */
    bool takePhase4(Machine* machine, const Frame::Packet& packet);
    /** @brief 系统信息里若有工站一句，拆到 station / stationNg，界面不再自己找关键字 */
    void fillStation(Tile* tile, const std::string& text);
    /** @brief 上传下一块。调用方不持锁 */
    void pumpUpload(const std::string& peerId);
    /** @brief UTF-8 路径转宽字符 */
    static std::wstring widePath(const std::string& text);
    /** @brief 文件 SHA-256 小写十六进制。打不开返回空 */
    std::string sha256File(const std::wstring& path) const;
    /** @brief 列本机目录给被控端。空路径列盘符 */
    std::string listLocalDirectory(const std::wstring& path) const;
    /** @brief 本机逐级建目录 */
    bool makeLocalDirectory(const std::wstring& path) const;
    /** @brief 开机以来毫秒数 */
    uint64_t nowMs() const;

    RemoteTcpServer m_tcp;              ///< 监听反连的 TCP 服务端
    RemoteIdentity m_identity;          ///< 本机 TLS 身份
    mutable std::mutex m_mu;            ///< 保护 m_run、m_listening、m_selfName、m_machines
    std::thread m_read;                 ///< 读线程
    std::thread m_write;                ///< 写线程
    bool m_run;                         ///< 线程运行标志，stop() 置 false
    bool m_listening;                   ///< 监听中
    bool m_adaptive;                    ///< 延迟很高时才向被控端报拥塞
    int m_idleMinutes;                  ///< 无本地操作断开分钟，0 关
    int m_bandwidthKbps;                ///< 带宽上限
    std::vector<std::string> m_connectOrder;
    uint64_t m_lastLocalActivity;       ///< 中控台本地活动时间
    std::string m_selfName;             ///< 回给被控端的控制端显示名，UTF-8
    std::vector<Machine> m_machines;    ///< 已知被控端，含已绑定但离线的条目
};

}

#endif
