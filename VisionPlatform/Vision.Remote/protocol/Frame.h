/**
 * @file Frame.h
 * @brief 远程控制 VRMT 帧（第 2 版）。不使用节点帧 VPRT
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_FRAME_H
#define VISION_REMOTE_FRAME_H

#include "CommPeer.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class Frame
 * @brief 拆组 VRMT 第 2 版帧。半包留在对象里，多字节整数用大端
 *
 * 帧头 16 字节：magic(4) + version(1) + type(1) + channel(1) + flags(1) + seq(4) + payloadLength(4)
 * 分片：同一逻辑报文可拆成多帧；收齐 FlagFin 后合并为一个 Packet
 */
class Frame {
public:
    static const int kHeaderBytes = 16;                         ///< 第 2 版帧头字节数
    static const uint32_t kMaxPayload = 1u * 1024u * 1024u;      ///< 单帧负载上限 1 MB
    static const int kVersion = 2;                               ///< 帧版本；与第 1 期不兼容

    /** @brief 通道号，与方案 8.2 一致 */
    enum Channel {
        ChControl = 0,      ///< 握手、认证、心跳、权限、配置
        ChInput = 1,        ///< 键鼠
        ChCursor = 2,       ///< 光标
        ChVideo = 3,        ///< 主码流
        ChThumb = 4,        ///< 子码流
        ChClip = 5,         ///< 剪贴板
        ChFile = 6,         ///< 文件
        ChChat = 7          ///< 聊天
    };

    /** @brief flags 位 */
    enum Flag {
        FlagFin = 0x01,     ///< 本逻辑报文的最后一帧（单帧也置位）
        FlagAck = 0x02,     ///< 需要确认（第二期暂不发确认帧）
        FlagCont = 0x04     ///< 分片续帧（非首片）
    };

    /**
     * @brief 报文类型。类型号在各自通道内独立；收包须先看 channel
     * @note 控制通道编号与方案 8.3 对齐
     */
    enum Type {
        // —— 控制通道 ChControl ——
        TypeHello = 1,
        TypeHelloAck = 2,
        TypeAuthRequest = 3,    ///< 认证 / 申请操作（含方式、挑战、权限）
        TypeAuthResult = 4,     ///< 认证结果
        TypeHeartbeat = 5,
        TypeHeartbeatAck = 6,
        TypeSubscribe = 7,      ///< 第三期码流订阅；第二期暂不用
        TypeStreamInfo = 8,     ///< 第三期
        TypeNetStats = 9,       ///< 第三期
        TypeOperate = 10,       ///< 申请 / 释放 / 交接
        TypeOperateState = 11,
        TypeConfig = 12,
        TypePower = 13,         ///< 第四期
        TypePrivacy = 14,       ///< 第四期
        TypeError = 15,
        TypeBye = 16,
        TypeSysInfo = 17,       ///< 第四期：被控端系统信息文本
        TypeViewers = 18,       ///< 第四期：正在看这台机器的中控台
        TypeInvite = 19,        ///< 第四期：请被控端再连一台中控台
        TypeProbe = 21,
        TypeAnnounce = 22
    };

    /** @brief 输入通道类型（与控制通道编号独立，收包须先看 channel） */
    enum TypeInput {
        TypeMouse = 1,
        TypeKey = 2,
        TypeText = 3,
        TypeSpecial = 4
    };

    /** @brief 剪贴板通道类型 */
    enum TypeClip {
        TypeClipText = 1,
        TypeClipImage = 2,
        TypeClipFiles = 3
    };

    /** @brief 文件通道类型 */
    enum TypeFile {
        TypeListDir = 1,
        TypeDirEntries = 2,
        TypeFileOpen = 3,
        TypeFileChunk = 4,
        TypeFileAck = 5,
        TypeFileDone = 6,
        TypeMkDir = 7           ///< 在被控端建目录，含上级
    };

    /** @brief 聊天通道类型 */
    enum TypeChat {
        TypeChatText = 1
    };

    /** @brief 视频通道类型 */
    enum TypeVideo {
        TypeVideoFrame = 1,
        TypeKeyFrameRequest = 2
    };

    /**
     * @brief 光标通道类型
     * @note 位置沿用已上线的 type 1。形状用 type 3，避免把现有位置包改号
     */
    enum TypeCursor {
        TypeCursorPos = 1,
        TypeCursorShape = 3
    };

    /** @brief Operate 动作（TypeOperate 负载首字节） */
    enum OperateAction {
        OperateRequest = 1,     ///< 申请操作
        OperateRelease = 2,     ///< 释放操作
        OperateHandoff = 3      ///< 交接（第四期）
    };

    /** @brief 已经拆开并拼完分片的一帧逻辑报文 */
    struct Packet {
        uint8_t type;           ///< 报文类型
        uint8_t channel;        ///< 通道
        uint8_t flags;          ///< 末片 flags（含 FlagFin）
        uint32_t seq;           ///< 首片序号（同报文各片共用）
        CommBytes payload;      ///< 合并后的完整负载
        Packet() : type(0), channel(0), flags(0), seq(0) {}
    };

    Frame();

    /**
     * @brief 组一帧或多片。payload 超过 kMaxPayload 时自动分片，返回首片；余片经 encodeMore 取
     * @note 简便重载：channel=ChControl，flags 由分片逻辑填写，seq=0
     */
    static CommBytes encode(uint8_t type, const CommBytes& payload, std::string* err);
    /** @brief 指定通道与序号组帧；大负载自动分片时只返回第一片，其余用 encodeRest */
    static CommBytes encode(uint8_t type, uint8_t channel, uint32_t seq, const CommBytes& payload, std::string* err);

    /**
     * @brief 把超限负载拆成多片完整帧（每片都带帧头）
     * @return 空表示失败；单片时 size==1
     */
    static std::vector<CommBytes> encodeAll(uint8_t type, uint8_t channel, uint32_t seq,
                                            const CommBytes& payload, std::string* err);

    /**
     * @brief 喂入新收到的字节，拆出完整包
     * @return 负载非法时返回 false，并写 err
     */
    bool push(const CommBytes& chunk, std::vector<Packet>* out, std::string* err);
    /** @brief 丢掉半包，准备下一条连接 */
    void clear();

    /************************************************** 写整数 **************************************************/
    static void appendU8(CommBytes* out, uint8_t v);
    /** @brief 小端写入 16 位无符号 */
    static void appendU16(CommBytes* out, uint16_t v);
    /** @brief 小端写入 32 位无符号 */
    static void appendU32(CommBytes* out, uint32_t v);
    /** @brief 小端写入 64 位无符号 */
    static void appendU64(CommBytes* out, uint64_t v);
    /** @brief 小端写入 16 位有符号 */
    static void appendI16(CommBytes* out, int16_t v);

    /************************************************** 读整数 **************************************************/
    static bool readU8(const CommBytes& in, size_t* off, uint8_t* v);
    /** @brief 小端读出 16 位无符号。不够时返回 false，off 不前进 */
    static bool readU16(const CommBytes& in, size_t* off, uint16_t* v);
    /** @brief 小端读出 32 位无符号 */
    static bool readU32(const CommBytes& in, size_t* off, uint32_t* v);
    /** @brief 小端读出 64 位无符号 */
    static bool readU64(const CommBytes& in, size_t* off, uint64_t* v);
    /** @brief 小端读出 16 位有符号 */
    static bool readI16(const CommBytes& in, size_t* off, int16_t* v);

private:
    struct RawFrame {
        uint8_t type;
        uint8_t channel;
        uint8_t flags;
        uint32_t seq;
        CommBytes payload;
        RawFrame() : type(0), channel(0), flags(0), seq(0) {}
    };

    struct Asm {
        uint8_t type;
        uint32_t seq;
        CommBytes body;
        bool active;
        Asm() : type(0), seq(0), active(false) {}
    };

    /** @brief 从 m_pending 取出一帧。半包返回 false 且 err 为空 */
    bool takeRaw(RawFrame* out, std::string* err);
    /** @brief 把一帧送进对应通道的分片装配。凑齐时写出 Packet */
    bool feedAsm(const RawFrame& raw, Packet* out, std::string* err);

    /** @brief 组一片不超过上限的帧 */
    static CommBytes encodeOne(uint8_t type, uint8_t channel, uint8_t flags, uint32_t seq,
                               const unsigned char* data, uint32_t len, std::string* err);

    CommBytes m_pending;        ///< 还没凑成一帧的字节
    Asm m_asm[8];               ///< 每通道一个分片装配缓冲
};

/**
 * @class FrameTx
 * @brief 按通道维护递增序号，组出发送帧
 */
class FrameTx {
public:
    /** @brief 各通道序号从 0 起 */
    FrameTx();
    /** @brief 取本通道下一个序号并组帧（可多片） */
    std::vector<CommBytes> pack(uint8_t type, uint8_t channel, const CommBytes& payload, std::string* err);
    /** @brief 控制通道简便接口 */
    std::vector<CommBytes> packControl(uint8_t type, const CommBytes& payload, std::string* err);

private:
    uint32_t m_seq[8];          ///< 各通道下一个序号
};

}

#endif
