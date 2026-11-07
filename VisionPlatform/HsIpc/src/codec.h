/**
 * @file codec.h
 * @brief 32 字节帧头和报文的编码解码，与传输无关
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 小端。字符串是 uint16 字节数加 UTF-8，没有结尾 0。
 * 字段顺序以 docs/本机进程通讯库方案.md 第 4 节为准。
 */
#ifndef HS_IPC_CODEC_H
#define HS_IPC_CODEC_H

#include "hs_ipc.h"

#include <string>
#include <vector>

namespace HsIpc {

#pragma pack(push, 1)
struct FrameHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint16_t flags;
    uint16_t reserved;
    uint32_t seq;
    uint64_t timeMs;
    uint32_t bodyBytes;
    uint32_t reserved2;
};
#pragma pack(pop)

static const uint32_t kMagic = 0x43504948u; /* 字节 48 49 50 43 */

struct Decoded {
    FrameHeader head;
    std::string name;
    std::string text;
    std::string path;
    int32_t result;
    int32_t code;
    int statCount;
    std::vector<HsIpcStat> stats;
    std::vector<std::string> statKeys;
    uint16_t format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t frameSeq;
    uint32_t dataBytes;
    uint32_t slotIndex;
    std::vector<uint8_t> data;
    uint64_t totalBytes;
    uint32_t crc32;
    uint32_t ackSeq;
    int32_t ackResult;
    uint16_t helloVersion;
    uint16_t helloTransport;

    Decoded()
        : result(0)
        , code(0)
        , statCount(0)
        , format(0)
        , width(0)
        , height(0)
        , stride(0)
        , frameSeq(0)
        , dataBytes(0)
        , slotIndex(0)
        , totalBytes(0)
        , crc32(0)
        , ackSeq(0)
        , ackResult(0)
        , helloVersion(0)
        , helloTransport(0)
    {
        head.magic = 0;
        head.version = 0;
        head.type = 0;
        head.flags = 0;
        head.reserved = 0;
        head.seq = 0;
        head.timeMs = 0;
        head.bodyBytes = 0;
        head.reserved2 = 0;
    }
};

void appendBytes(std::vector<uint8_t>* out, const void* data, uint32_t n);
bool appendStr(std::vector<uint8_t>* out, const char* text);

bool buildBody(const HsIpcMessage* msg, uint16_t flags, uint32_t slotIndex, int helloTransport,
    std::vector<uint8_t>* body, std::string* err);
void writeFrame(std::vector<uint8_t>* frame, uint16_t type, uint16_t flags, uint32_t seq,
    uint64_t timeMs, const std::vector<uint8_t>& body);
bool parseFrame(const uint8_t* frame, uint32_t n, Decoded* out, std::string* err);

}
#endif
