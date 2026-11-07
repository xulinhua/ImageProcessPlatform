/**
 * @file codec.cpp
 * @brief 帧的编码和解码
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 带 HS_IPC_FLAG_SLOT 的图像和文件，体在 dataBytes 之后是槽号，像素不在帧里。
 */
#include "codec.h"

#include <string.h>

namespace HsIpc {

static_assert(sizeof(FrameHeader) == 32, "frame header must be 32 bytes");

void appendBytes(std::vector<uint8_t>* out, const void* data, uint32_t n)
{
    if (!n) {
        return;
    }
    const size_t at = out->size();
    out->resize(at + n);
    memcpy(&(*out)[at], data, n);
}

bool appendStr(std::vector<uint8_t>* out, const char* text)
{
    const uint32_t n = text ? (uint32_t)strlen(text) : 0;
    if (n > 65535u) {
        return false;
    }
    const uint16_t len = (uint16_t)n;
    appendBytes(out, &len, sizeof(len));
    if (n) {
        appendBytes(out, text, n);
    }
    return true;
}

static bool readBytes(const uint8_t*& p, const uint8_t* end, void* dst, uint32_t n)
{
    if ((uint32_t)(end - p) < n) {
        return false;
    }
    memcpy(dst, p, n);
    p += n;
    return true;
}

static bool readStr(const uint8_t*& p, const uint8_t* end, std::string* out)
{
    uint16_t len = 0;
    if (!readBytes(p, end, &len, sizeof(len))) {
        return false;
    }
    if ((uint32_t)(end - p) < len) {
        return false;
    }
    out->assign((const char*)p, (size_t)len);
    p += len;
    return true;
}

bool buildBody(const HsIpcMessage* msg, uint16_t flags, uint32_t slotIndex, int helloTransport,
    std::vector<uint8_t>* body, std::string* err)
{
    body->clear();
    if (!msg) {
        if (err) {
            *err = "报文为空";
        }
        return false;
    }
    switch (msg->type) {
    case HS_IPC_HELLO: {
        const uint16_t version = HS_IPC_VERSION;
        const uint16_t transport = (uint16_t)helloTransport;
        appendBytes(body, &version, sizeof(version));
        appendBytes(body, &transport, sizeof(transport));
        return true;
    }
    case HS_IPC_HEARTBEAT:
    case HS_IPC_BYE:
        return true;
    case HS_IPC_SIGNAL:
        if (!appendStr(body, msg->name)) {
            if (err) {
                *err = "字符串超过 65535 字节";
            }
            return false;
        }
        appendBytes(body, &msg->result, sizeof(msg->result));
        appendBytes(body, &msg->code, sizeof(msg->code));
        if (!appendStr(body, msg->text)) {
            if (err) {
                *err = "字符串超过 65535 字节";
            }
            return false;
        }
        return true;
    case HS_IPC_STATS: {
        if (msg->statCount < 0 || (msg->statCount > 0 && !msg->stats)) {
            if (err) {
                *err = "统计项无效";
            }
            return false;
        }
        const uint16_t count = (uint16_t)msg->statCount;
        appendBytes(body, &count, sizeof(count));
        for (int i = 0; i < msg->statCount; ++i) {
            if (!appendStr(body, msg->stats[i].key)) {
                if (err) {
                    *err = "字符串超过 65535 字节";
                }
                return false;
            }
            const uint8_t kind = (uint8_t)msg->stats[i].type;
            appendBytes(body, &kind, sizeof(kind));
            if (kind == HS_IPC_VALUE_FLOAT) {
                appendBytes(body, &msg->stats[i].f64, sizeof(double));
            } else {
                appendBytes(body, &msg->stats[i].i64, sizeof(int64_t));
            }
        }
        return true;
    }
    case HS_IPC_IMAGE: {
        if (!appendStr(body, msg->name)) {
            if (err) {
                *err = "字符串超过 65535 字节";
            }
            return false;
        }
        appendBytes(body, &msg->format, sizeof(msg->format));
        appendBytes(body, &msg->width, sizeof(msg->width));
        appendBytes(body, &msg->height, sizeof(msg->height));
        appendBytes(body, &msg->stride, sizeof(msg->stride));
        appendBytes(body, &msg->frameSeq, sizeof(msg->frameSeq));
        appendBytes(body, &msg->dataBytes, sizeof(msg->dataBytes));
        if (flags & HS_IPC_FLAG_SLOT) {
            appendBytes(body, &slotIndex, sizeof(slotIndex));
            return true;
        }
        if (msg->dataBytes && !msg->data) {
            if (err) {
                *err = "图像数据为空";
            }
            return false;
        }
        if (msg->dataBytes) {
            appendBytes(body, msg->data, msg->dataBytes);
        }
        return true;
    }
    case HS_IPC_FILE: {
        if (!appendStr(body, msg->name)) {
            if (err) {
                *err = "字符串超过 65535 字节";
            }
            return false;
        }
        appendBytes(body, &msg->totalBytes, sizeof(msg->totalBytes));
        appendBytes(body, &msg->crc32, sizeof(msg->crc32));
        if (!appendStr(body, msg->path)) {
            if (err) {
                *err = "字符串超过 65535 字节";
            }
            return false;
        }
        const uint32_t n = (flags & HS_IPC_FLAG_SLOT) ? msg->dataBytes : (msg->data ? msg->dataBytes : 0);
        appendBytes(body, &n, sizeof(n));
        if (flags & HS_IPC_FLAG_SLOT) {
            appendBytes(body, &slotIndex, sizeof(slotIndex));
            return true;
        }
        if (n) {
            appendBytes(body, msg->data, n);
        }
        return true;
    }
    case HS_IPC_ACK:
        appendBytes(body, &msg->ackSeq, sizeof(msg->ackSeq));
        appendBytes(body, &msg->ackResult, sizeof(msg->ackResult));
        return true;
    default:
        if (err) {
            *err = "报文类型无效";
        }
        return false;
    }
}

void writeFrame(std::vector<uint8_t>* frame, uint16_t type, uint16_t flags, uint32_t seq,
    uint64_t timeMs, const std::vector<uint8_t>& body)
{
    FrameHeader head;
    head.magic = kMagic;
    head.version = HS_IPC_VERSION;
    head.type = type;
    head.flags = flags;
    head.reserved = 0;
    head.seq = seq;
    head.timeMs = timeMs;
    head.bodyBytes = (uint32_t)body.size();
    head.reserved2 = 0;
    frame->clear();
    appendBytes(frame, &head, sizeof(head));
    if (!body.empty()) {
        appendBytes(frame, &body[0], (uint32_t)body.size());
    }
}

bool parseFrame(const uint8_t* frame, uint32_t n, Decoded* out, std::string* err)
{
    if (!frame || n < sizeof(FrameHeader) || !out) {
        if (err) {
            *err = "帧长度不够";
        }
        return false;
    }
    memcpy(&out->head, frame, sizeof(FrameHeader));
    if (out->head.magic != kMagic) {
        if (err) {
            *err = "帧头魔数不对";
        }
        return false;
    }
    if (out->head.version != HS_IPC_VERSION) {
        if (err) {
            *err = "帧版本不认识";
        }
        return false;
    }
    if (n < sizeof(FrameHeader) + out->head.bodyBytes) {
        if (err) {
            *err = "帧体长度不够";
        }
        return false;
    }
    const uint8_t* p = frame + sizeof(FrameHeader);
    const uint8_t* end = p + out->head.bodyBytes;
    const uint16_t type = out->head.type;
    if (type == HS_IPC_HELLO) {
        return readBytes(p, end, &out->helloVersion, sizeof(out->helloVersion))
            && readBytes(p, end, &out->helloTransport, sizeof(out->helloTransport));
    }
    if (type == HS_IPC_HEARTBEAT || type == HS_IPC_BYE) {
        return true;
    }
    if (type == HS_IPC_SIGNAL) {
        return readStr(p, end, &out->name)
            && readBytes(p, end, &out->result, sizeof(out->result))
            && readBytes(p, end, &out->code, sizeof(out->code))
            && readStr(p, end, &out->text);
    }
    if (type == HS_IPC_STATS) {
        uint16_t count = 0;
        if (!readBytes(p, end, &count, sizeof(count))) {
            return false;
        }
        out->statKeys.resize(count);
        out->stats.resize(count);
        for (uint16_t i = 0; i < count; ++i) {
            if (!readStr(p, end, &out->statKeys[i])) {
                return false;
            }
            uint8_t kind = 0;
            if (!readBytes(p, end, &kind, sizeof(kind))) {
                return false;
            }
            out->stats[i].type = kind;
            out->stats[i].key = 0;
            out->stats[i].i64 = 0;
            out->stats[i].f64 = 0;
            if (kind == HS_IPC_VALUE_FLOAT) {
                if (!readBytes(p, end, &out->stats[i].f64, sizeof(double))) {
                    return false;
                }
            } else if (!readBytes(p, end, &out->stats[i].i64, sizeof(int64_t))) {
                return false;
            }
        }
        out->statCount = count;
        return true;
    }
    if (type == HS_IPC_IMAGE) {
        if (!readStr(p, end, &out->name)
            || !readBytes(p, end, &out->format, sizeof(out->format))
            || !readBytes(p, end, &out->width, sizeof(out->width))
            || !readBytes(p, end, &out->height, sizeof(out->height))
            || !readBytes(p, end, &out->stride, sizeof(out->stride))
            || !readBytes(p, end, &out->frameSeq, sizeof(out->frameSeq))
            || !readBytes(p, end, &out->dataBytes, sizeof(out->dataBytes))) {
            return false;
        }
        if (out->head.flags & HS_IPC_FLAG_SLOT) {
            return readBytes(p, end, &out->slotIndex, sizeof(out->slotIndex));
        }
        if ((uint32_t)(end - p) < out->dataBytes) {
            return false;
        }
        out->data.assign(p, p + out->dataBytes);
        return true;
    }
    if (type == HS_IPC_FILE) {
        uint32_t nData = 0;
        if (!readStr(p, end, &out->name)
            || !readBytes(p, end, &out->totalBytes, sizeof(out->totalBytes))
            || !readBytes(p, end, &out->crc32, sizeof(out->crc32))
            || !readStr(p, end, &out->path)
            || !readBytes(p, end, &nData, sizeof(nData))) {
            return false;
        }
        out->dataBytes = nData;
        if (out->head.flags & HS_IPC_FLAG_SLOT) {
            return readBytes(p, end, &out->slotIndex, sizeof(out->slotIndex));
        }
        if ((uint32_t)(end - p) < nData) {
            return false;
        }
        out->data.assign(p, p + nData);
        return true;
    }
    if (type == HS_IPC_ACK) {
        return readBytes(p, end, &out->ackSeq, sizeof(out->ackSeq))
            && readBytes(p, end, &out->ackResult, sizeof(out->ackResult));
    }
    if (err) {
        *err = "帧类型不认识";
    }
    return false;
}

}
