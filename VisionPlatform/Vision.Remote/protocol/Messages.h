/**
 * @file Messages.h
 * @brief VRMT 第 2 版负载字段。多字节整数大端，短文本前有 1 字节长度
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_MESSAGES_H
#define VISION_REMOTE_MESSAGES_H

#include "Frame.h"
#include <stdint.h>
#include <string>

namespace Vision {

/**
 * @class RemoteConst
 * @brief 远程控制的协议常量
 */
class RemoteConst {
public:
    static const int kPort = 19100;
    static const int kHeartbeatMs = 1000;
    static const int kOfflineMs = 3000;
    static const int kCaptureMs = 66;   ///< 默认约 15 fps（1000/15）
    static const int kLongEdge = 1280;
    static const int kJpegQuality = 60;
    static const int kMaxFpsMin = 5;
    static const int kMaxFpsMax = 500;
    static const int kMaxFpsDefault = 60;
    static const int kTextMax = 64;
    static const int kDiscoverPort = 19101;
    static const int kAuthFailMax = 5;          ///< 连续失败次数上限，之后锁定
    static const int kAuthLockSec = 300;        ///< 首次锁定秒数（5 分钟），再失败翻倍，最长 3600
    static const int kConfirmSec = 30;          ///< 来访确认超时秒
    static const int kFileChunk = 256 * 1024;   ///< 文件块大小
    static const int kFileWindow = 8;           ///< 最多同时在途的块数
};

/** @brief 会话权限位，可按位或 */
enum RemotePerm {
    PermView = 1 << 0,          ///< 观看（必有）
    PermOperate = 1 << 1,       ///< 键鼠操作
    PermClipboard = 1 << 2,     ///< 剪贴板
    PermFile = 1 << 3,          ///< 文件
    PermPower = 1 << 4,         ///< 电源
    PermPrivacy = 1 << 5,       ///< 隐私屏
    PermDefaultRequest = PermView | PermOperate,
    PermAll = PermView | PermOperate | PermClipboard | PermFile | PermPower | PermPrivacy
};

/** @brief 认证方式 */
enum AuthMethod {
    AuthMethodCode = 1,         ///< 一次性验证码
    AuthMethodPassword = 2,     ///< 无人值守密码（挑战应答）
    AuthMethodTrusted = 3,      ///< 信任设备指纹
    AuthMethodHighest = 4       ///< 最高权限：跳过确认与口令，申请满权限
};

struct HelloMsg {
    std::string name;
    std::string source;
    uint8_t share;
    uint8_t visible;
    uint8_t monitor;
    std::string title;
    std::string process;
    std::string deviceId;
    uint8_t monitors;
    std::string system;
    std::string address;
    std::string fingerprint;    ///< 被控端证书指纹（SHA-256 十六进制），第二期新增；旧字段后可选
    uint8_t forceRecord;        ///< 1 时中控台取得操作后必须开始录屏
    HelloMsg() : share(0), visible(0), monitor(0), monitors(1), forceRecord(0) {}
};

struct ConfigMsg {
    uint8_t share;
    uint8_t monitor;
    std::string title;
    std::string process;
    uint16_t maxFps;        ///< 帧率上限 5–500；0 表示沿用被控端当前值/默认
    uint8_t jpegQuality;    ///< JPEG 质量 1–100；0 表示沿用
    uint16_t longEdge;      ///< 画面长边像素上限；0 表示沿用
    uint8_t codec;          ///< 0 沿用；1 JPEG；2 H.264；3 原图 BGRA
    uint16_t bitrateKbps;   ///< H.264 目标码率 kbps；0 表示按画质推算
    ConfigMsg() : share(1), monitor(0), maxFps(0), jpegQuality(0), longEdge(0), codec(0), bitrateKbps(0) {}
};

struct HelloAckMsg {
    uint8_t code;
    std::string name;
    std::string fingerprint;    ///< 控制端证书指纹
    HelloAckMsg() : code(1) {}
};

/**
 * @class AuthRequestMsg
 * @brief 控制端认证并申请操作（可只认证不申请操作：perms 仅 View）
 */
struct AuthRequestMsg {
    uint8_t method;             ///< AuthMethod
    uint32_t perms;             ///< 申请的权限位
    std::string token;          ///< 验证码明文，或密码挑战应答十六进制，或空（信任设备）
    std::string challenge;      ///< 被控端下发的挑战（密码方式时控制端带回应答）；首次可空由被控生成
    AuthRequestMsg() : method(AuthMethodTrusted), perms(PermDefaultRequest) {}
};

/**
 * @class AuthResultMsg
 * @brief 被控端认证结果
 * @note code：0 成功，1 未定，2 忙/已有操作，3 拒绝，4 超时，5 认证失败，6 已锁定
 */
struct AuthResultMsg {
    uint8_t code;
    uint32_t perms;             ///< 实际授予的权限
    uint8_t triesLeft;          ///< 剩余尝试次数（失败时）
    uint32_t lockSec;           ///< 锁定剩余秒（code=6）
    std::string challenge;      ///< 密码方式时给出的新挑战（需再发一次 AuthRequest）
    AuthResultMsg() : code(1), perms(0), triesLeft(0), lockSec(0) {}
};

struct HeartbeatMsg {
    uint64_t ms;
    HeartbeatMsg() : ms(0) {}
};

struct HeartbeatAckMsg {
    uint64_t peerMs;
    uint64_t selfMs;
    HeartbeatAckMsg() : peerMs(0), selfMs(0) {}
};

struct PictureMsg {
    uint8_t visible;
    uint8_t operable;
    uint32_t clientW;
    uint32_t clientH;
    uint32_t imageW;
    uint32_t imageH;
    uint8_t codec;          ///< 1 JPEG，2 H.264 Annex-B，3 原图 BGRA。负载仍放在 jpeg
    uint8_t key;            ///< 1 关键帧
    CommBytes jpeg;
    PictureMsg() : visible(0), operable(0), clientW(0), clientH(0), imageW(0), imageH(0), codec(1), key(0) {}
};

/** @brief 第三期：订阅主码流或子码流。stream 1=主 2=子 */
struct SubscribeMsg {
    uint8_t stream;
    uint8_t enable;
    uint16_t maxFps;        ///< 帧率上限 5–500；0 沿用
    uint8_t jpegQuality;
    uint16_t longEdge;
    uint8_t codec;          ///< 0 沿用被控端；1 JPEG；2 H.264；3 原图
    SubscribeMsg() : stream(2), enable(1), maxFps(2), jpegQuality(60), longEdge(480), codec(0) {}
};

/** @brief 第三期：当前码流参数。codec 1=JPEG 2=H.264 3=原图 */
struct StreamInfoMsg {
    uint8_t stream;
    uint8_t codec;
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    StreamInfoMsg() : stream(1), codec(1), width(0), height(0), fps(0) {}
};

/** @brief 第三期：控制端每秒回传，供被控端降码率 */
struct NetStatsMsg {
    uint16_t rttMs;
    uint8_t congested;
    NetStatsMsg() : rttMs(0), congested(0) {}
};

/** @brief 第三期：光标在图像坐标中的位置，控制端本地绘制 */
struct CursorPosMsg {
    uint16_t x;
    uint16_t y;
    uint8_t visible;
    CursorPosMsg() : x(0), y(0), visible(0) {}
};

/** @brief 光标形状。png 是 BGRA 压出来的 PNG，热点相对图标左上角 */
struct CursorShapeMsg {
    uint16_t hotX;
    uint16_t hotY;
    uint16_t width;
    uint16_t height;
    CommBytes png;
    CursorShapeMsg() : hotX(0), hotY(0), width(0), height(0) {}
};

struct InputMsg {
    enum Action {
        ActionLegacy = 0,
        ActionMove = 1,
        ActionPress = 2,
        ActionRelease = 3
    };
    uint16_t x;
    uint16_t y;
    uint8_t buttons;
    int16_t wheel;
    uint16_t vk;
    uint8_t down;
    uint8_t action;
    InputMsg() : x(0), y(0), buttons(0), wheel(0), vk(0), down(0), action(ActionLegacy) {}
};

struct OperateMsg {
    uint8_t action;             ///< OperateAction
    std::string who;            ///< 交接目标的证书指纹，其他动作为空
    OperateMsg() : action(Frame::OperateRequest) {}
};

struct SpecialMsg {
    enum Kind {
        KindCad = 1,            ///< Ctrl+Alt+Del
        KindWin = 2,
        KindAltTab = 3,
        KindPrintScreen = 4,
        KindProcessList = 5     ///< 请求被控端进程列表（回 SysInfo，前缀 PROCESSES\n）
    };
    uint8_t kind;
    SpecialMsg() : kind(KindCad) {}
};

/**
 * @class Messages
 * @brief 各 type 的负载读写。pack 返回可直接 write 的字节（多分片时已拼在一起）
 */
class Messages {
public:
    /************************************************** 会话 **************************************************/
    /** @brief 打包 Hello */
    static CommBytes packHello(const HelloMsg& msg, std::string* err);
    /** @brief 拆 Hello */
    static bool unpackHello(const CommBytes& payload, HelloMsg* msg);

    /** @brief 打包 Hello 应答 */
    static CommBytes packHelloAck(const HelloAckMsg& msg, std::string* err);
    /** @brief 拆 Hello 应答 */
    static bool unpackHelloAck(const CommBytes& payload, HelloAckMsg* msg);

    /** @brief 打包认证请求 */
    static CommBytes packAuthRequest(const AuthRequestMsg& msg, std::string* err);
    /** @brief 拆认证请求 */
    static bool unpackAuthRequest(const CommBytes& payload, AuthRequestMsg* msg);

    /** @brief 打包认证结果 */
    static CommBytes packAuthResult(const AuthResultMsg& msg, std::string* err);
    /** @brief 拆认证结果 */
    static bool unpackAuthResult(const CommBytes& payload, AuthResultMsg* msg);

    /** @brief 打包心跳 */
    static CommBytes packHeartbeat(const HeartbeatMsg& msg, std::string* err);
    /** @brief 拆心跳 */
    static bool unpackHeartbeat(const CommBytes& payload, HeartbeatMsg* msg);

    /** @brief 打包心跳应答 */
    static CommBytes packHeartbeatAck(const HeartbeatAckMsg& msg, std::string* err);
    /** @brief 拆心跳应答 */
    static bool unpackHeartbeatAck(const CommBytes& payload, HeartbeatAckMsg* msg);

    /** @brief 画面走视频通道。channel 为 ChVideo（主）或 ChThumb（子），负载为 JPEG */
    static CommBytes packPicture(const PictureMsg& msg, std::string* err);
    static CommBytes packPicture(const PictureMsg& msg, uint8_t channel, std::string* err);
    /** @brief 拆画面。编码在负载里，不看通道 */
    static bool unpackPicture(const CommBytes& payload, PictureMsg* msg);

    /** @brief 打包订阅 */
    static CommBytes packSubscribe(const SubscribeMsg& msg, std::string* err);
    /** @brief 拆订阅 */
    static bool unpackSubscribe(const CommBytes& payload, SubscribeMsg* msg);
    /** @brief 打包码流参数 */
    static CommBytes packStreamInfo(const StreamInfoMsg& msg, std::string* err);
    /** @brief 拆码流参数 */
    static bool unpackStreamInfo(const CommBytes& payload, StreamInfoMsg* msg);
    /** @brief 打包往返与拥塞 */
    static CommBytes packNetStats(const NetStatsMsg& msg, std::string* err);
    /** @brief 拆往返与拥塞 */
    static bool unpackNetStats(const CommBytes& payload, NetStatsMsg* msg);
    /** @brief 打包光标位置 */
    static CommBytes packCursorPos(const CursorPosMsg& msg, std::string* err);
    /** @brief 拆光标位置 */
    static bool unpackCursorPos(const CommBytes& payload, CursorPosMsg* msg);
    /** @brief 打包光标形状 */
    static CommBytes packCursorShape(const CursorShapeMsg& msg, std::string* err);
    /** @brief 拆光标形状 */
    static bool unpackCursorShape(const CommBytes& payload, CursorShapeMsg* msg);

    /** @brief 键鼠走输入通道；vk!=0 为 Key，否则 Mouse */
    static CommBytes packInput(const InputMsg& msg, std::string* err);
    /** @brief 拆键鼠 */
    static bool unpackInput(const CommBytes& payload, InputMsg* msg);

    /** @brief 打包操作权申请、释放或交接 */
    static CommBytes packOperate(const OperateMsg& msg, std::string* err);
    /** @brief 拆操作权 */
    static bool unpackOperate(const CommBytes& payload, OperateMsg* msg);

    /** @brief 打包特殊键，如 Ctrl+Alt+Del */
    static CommBytes packSpecial(const SpecialMsg& msg, std::string* err);
    /** @brief 拆特殊键 */
    static bool unpackSpecial(const CommBytes& payload, SpecialMsg* msg);

    /** @brief 打包剪贴板文本 */
    static CommBytes packClipText(const std::string& text, std::string* err);
    /** @brief 拆剪贴板文本 */
    static bool unpackClipText(const CommBytes& payload, std::string* text);
    /** @brief 打包剪贴板 PNG */
    static CommBytes packClipImage(const CommBytes& png, std::string* err);
    /** @brief 拆剪贴板 PNG */
    static bool unpackClipImage(const CommBytes& payload, CommBytes* png);
    /** @brief 打包剪贴板文件路径，一行一个 */
    static CommBytes packClipFiles(const std::string& paths, std::string* err);
    /** @brief 拆剪贴板文件路径 */
    static bool unpackClipFiles(const CommBytes& payload, std::string* paths);
    /**
     * @brief 打包聊天
     * @param kind 0 普通聊天，1 被控端弹窗。旧包没有这一字节时按聊天处理
     */
    static CommBytes packChat(const std::string& name, const std::string& text, std::string* err, uint8_t kind = 0);
    /** @brief 拆聊天。kind 可空；负载末尾没有类型字节时写成 0 */
    static bool unpackChat(const CommBytes& payload, std::string* name, std::string* text, uint8_t* kind = 0);
    /** @brief 打包电源动作 */
    static CommBytes packPower(uint8_t action, std::string* err);
    /** @brief 拆电源动作 */
    static bool unpackPower(const CommBytes& payload, uint8_t* action);
    /** @brief 打包隐私屏开关 */
    static CommBytes packPrivacy(uint8_t on, std::string* err);
    /** @brief 拆隐私屏开关 */
    static bool unpackPrivacy(const CommBytes& payload, uint8_t* on);
    /** @brief 打包系统信息文本 */
    static CommBytes packSysInfo(const std::string& text, std::string* err);
    /** @brief 拆系统信息文本 */
    static bool unpackSysInfo(const CommBytes& payload, std::string* text);
    /** @brief 打包列目录请求 */
    static CommBytes packListDir(const std::string& path, std::string* err);
    /** @brief 拆列目录请求 */
    static bool unpackListDir(const CommBytes& payload, std::string* path);
    /** @brief 打包目录列表 */
    static CommBytes packDirEntries(const std::string& listing, std::string* err);
    /** @brief 拆目录列表 */
    static bool unpackDirEntries(const CommBytes& payload, std::string* listing);
    /** @brief 打包在被控端建目录 */
    static CommBytes packMkDir(uint32_t id, const std::string& path, std::string* err);
    /** @brief 拆建目录 */
    static bool unpackMkDir(const CommBytes& payload, uint32_t* id, std::string* path);
    /** @brief pull=1 被控端把文件发给控制端；pull=0 控制端上传 */
    static CommBytes packFileOpen(uint32_t id, uint8_t pull, const std::string& path, uint64_t size, std::string* err,
        const std::string& sha256 = std::string(), uint64_t resume = 0);
    static bool unpackFileOpen(const CommBytes& payload, uint32_t* id, uint8_t* pull, std::string* path, uint64_t* size,
        std::string* sha256 = 0, uint64_t* resume = 0);
    /** @brief 打包文件续传确认，offset 为已收字节 */
    static CommBytes packFileAck(uint32_t id, uint64_t offset, std::string* err);
    /** @brief 拆文件续传确认 */
    static bool unpackFileAck(const CommBytes& payload, uint32_t* id, uint64_t* offset);
    /** @brief 打包观看者列表 */
    static CommBytes packViewers(const std::string& listing, std::string* err);
    /** @brief 拆观看者列表 */
    static bool unpackViewers(const CommBytes& payload, std::string* listing);
    /** @brief 打包邀请另一台中控台 */
    static CommBytes packInvite(const std::string& host, uint16_t port, std::string* err);
    /** @brief 拆邀请 */
    static bool unpackInvite(const CommBytes& payload, std::string* host, uint16_t* port);
    /** @brief 打包文件块 */
    static CommBytes packFileChunk(uint32_t id, uint32_t offset, const CommBytes& data, std::string* err);
    /** @brief 拆文件块 */
    static bool unpackFileChunk(const CommBytes& payload, uint32_t* id, uint32_t* offset, CommBytes* data);
    /** @brief 打包文件结束。ok 非 0 表示成功 */
    static CommBytes packFileDone(uint32_t id, uint8_t ok, const std::string& note, std::string* err);
    /** @brief 拆文件结束 */
    static bool unpackFileDone(const CommBytes& payload, uint32_t* id, uint8_t* ok, std::string* note);

    /** @brief 打包无负载帧，通道为控制 */
    static CommBytes packEmpty(uint8_t type, std::string* err);
    /** @brief 打包无负载帧，指定通道 */
    static CommBytes packEmpty(uint8_t type, uint8_t channel, std::string* err);

    /** @brief 打包共享配置 */
    static CommBytes packConfig(const ConfigMsg& msg, std::string* err);
    /** @brief 拆共享配置 */
    static bool unpackConfig(const CommBytes& payload, ConfigMsg* msg);

    /** @brief 打包局域网发现通告 */
    static CommBytes packAnnounce(const HelloMsg& msg, std::string* err);
    /** @brief 拆局域网发现通告 */
    static bool unpackAnnounce(const CommBytes& payload, HelloMsg* msg);

private:
    /** @brief 写入限长文本。超长返回 false */
    static bool packText(CommBytes* out, const std::string& text, std::string* err);
    /** @brief 读出限长文本 */
    static bool unpackText(const CommBytes& in, size_t* off, std::string* text);
    /** @brief 写入不截断的文本 */
    static bool packLong(CommBytes* out, const std::string& text, std::string* err);
    /** @brief 读出不截断的文本 */
    static bool unpackLong(const CommBytes& in, size_t* off, std::string* text);
    /** @brief 写入 Hello 负载，不含帧头 */
    static bool packHelloBody(CommBytes* body, const HelloMsg& msg, std::string* err);
    /** @brief 给负载加上帧头，超长则分片后拼在一起 */
    static CommBytes finish(uint8_t type, uint8_t channel, const CommBytes& body, std::string* err);
};

}

#endif
