/**
 * @file session.cpp
 * @brief C 接口。组帧后交给当前传输
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 共享内存上，带内容的图像和文件先占槽，帧里只放槽号；poll 再把槽里的字节拷出来并释放槽。
 * path 非空的文件不占槽。其它传输上超过 HS_IPC_CHUNK_BYTES 的载荷拆成多帧，
 * 除最后一帧外带 HS_IPC_FLAG_CONT。
 */
#include "transport.h"
#include "codec.h"
#include "names.h"

#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

struct HsIpcSession {
    HsIpc::Transport* transport;
    int kind;
    uint32_t seq;
    uint64_t lastRecv;
    std::string name;
    std::string text;
    std::string path;
    std::vector<uint8_t> data;
    std::vector<HsIpcStat> stats;
    std::vector<std::string> statKeys;
};

namespace {

void writeErr(char* err, int errBytes, const char* text)
{
    if (!err || errBytes <= 0) {
        return;
    }
    if (!text) {
        err[0] = 0;
        return;
    }
    int i = 0;
    for (; text[i] && i < errBytes - 1; ++i) {
        err[i] = text[i];
    }
    err[i] = 0;
}

HsIpc::Transport* makeTransport(int kind)
{
    switch (kind) {
    case HS_IPC_SHARED_MEMORY:
        return HsIpc::createShm();
    case HS_IPC_NAMED_PIPE:
        return HsIpc::createPipe();
    case HS_IPC_MESSAGE_QUEUE:
        return HsIpc::createQueue();
    case HS_IPC_SOCKET:
        return HsIpc::createSocket();
    default:
        return 0;
    }
}

int sendEncoded(HsIpcSession* session, const HsIpcMessage* msg, uint16_t flags, uint32_t slotIndex)
{
    std::vector<uint8_t> body;
    std::string err;
    if (!HsIpc::buildBody(msg, flags, slotIndex, session->kind, &body, &err)) {
        return HS_IPC_ERR_ARG;
    }
    std::vector<uint8_t> frame;
    HsIpc::writeFrame(&frame, msg->type, flags, ++session->seq, GetTickCount64(), body);
    return session->transport->sendFrame(&frame[0], (uint32_t)frame.size());
}

void publish(HsIpcSession* session, const HsIpc::Decoded& decoded, HsIpcMessage* msg)
{
    memset(msg, 0, sizeof(*msg));
    msg->type = decoded.head.type;
    msg->flags = decoded.head.flags;
    msg->seq = decoded.head.seq;
    msg->timeMs = decoded.head.timeMs;
    session->name = decoded.name;
    session->text = decoded.text;
    session->path = decoded.path;
    session->data = decoded.data;
    session->statKeys = decoded.statKeys;
    session->stats = decoded.stats;
    for (size_t i = 0; i < session->stats.size() && i < session->statKeys.size(); ++i) {
        session->stats[i].key = session->statKeys[i].c_str();
    }
    msg->name = session->name.c_str();
    msg->text = session->text.c_str();
    msg->path = session->path.c_str();
    msg->result = decoded.result;
    msg->code = decoded.code;
    msg->statCount = (int)session->stats.size();
    msg->stats = session->stats.empty() ? 0 : &session->stats[0];
    msg->format = decoded.format;
    msg->width = decoded.width;
    msg->height = decoded.height;
    msg->stride = decoded.stride;
    msg->frameSeq = decoded.frameSeq;
    msg->dataBytes = session->data.empty() ? decoded.dataBytes : (uint32_t)session->data.size();
    msg->data = session->data.empty() ? 0 : &session->data[0];
    msg->totalBytes = decoded.totalBytes;
    msg->crc32 = decoded.crc32;
    msg->ackSeq = decoded.ackSeq;
    msg->ackResult = decoded.ackResult;
}

}

HsIpcSession* hs_ipc_open(const HsIpcConfig* cfg, char* err, int errBytes)
{
    if (!cfg || !HsIpc::validChannelName(cfg->name)) {
        writeErr(err, errBytes, "通道名无效");
        return 0;
    }
    HsIpc::Transport* transport = makeTransport(cfg->transport);
    if (!transport) {
        writeErr(err, errBytes, "传输种类无效");
        return 0;
    }
    std::string reason;
    const int capacity = cfg->signalCapacity > 0 ? cfg->signalCapacity : 32;
    const uint32_t imageBytes = cfg->imageSlotBytes ? cfg->imageSlotBytes : HS_IPC_CHUNK_BYTES;
    const uint32_t fileBytes = cfg->fileSlotBytes ? cfg->fileSlotBytes : HS_IPC_CHUNK_BYTES;
    if (!transport->open(cfg->name, cfg->host, cfg->port, capacity, imageBytes, fileBytes, &reason)) {
        writeErr(err, errBytes, reason.empty() ? "打开通道失败" : reason.c_str());
        delete transport;
        return 0;
    }
    HsIpcSession* session = new HsIpcSession();
    session->transport = transport;
    session->kind = cfg->transport;
    session->seq = 0;
    session->lastRecv = 0;
    return session;
}

void hs_ipc_close(HsIpcSession* session)
{
    if (!session) {
        return;
    }
    delete session->transport;
    delete session;
}

int hs_ipc_send(HsIpcSession* session, const HsIpcMessage* msg)
{
    if (!session || !session->transport || !msg) {
        return HS_IPC_ERR_ARG;
    }
    const bool hasPath = msg->path && msg->path[0];
    const bool bulk = (msg->type == HS_IPC_IMAGE && msg->dataBytes > 0)
        || (msg->type == HS_IPC_FILE && !hasPath && msg->dataBytes > 0);
    if (bulk && !msg->data) {
        return HS_IPC_ERR_ARG;
    }
    if (bulk && session->kind != HS_IPC_SHARED_MEMORY && msg->dataBytes > HS_IPC_CHUNK_BYTES) {
        uint32_t offset = 0;
        while (offset < msg->dataBytes) {
            const uint32_t piece = msg->dataBytes - offset > HS_IPC_CHUNK_BYTES
                ? HS_IPC_CHUNK_BYTES
                : msg->dataBytes - offset;
            HsIpcMessage part = *msg;
            part.data = msg->data + offset;
            part.dataBytes = piece;
            const uint16_t flags = (offset + piece < msg->dataBytes) ? (uint16_t)HS_IPC_FLAG_CONT : 0;
            const int rc = sendEncoded(session, &part, flags, 0);
            if (rc != HS_IPC_OK) {
                return rc;
            }
            offset += piece;
        }
        return HS_IPC_OK;
    }
    if (bulk && session->kind == HS_IPC_SHARED_MEMORY) {
        uint32_t index = 0;
        const int published = session->transport->publishSlot(msg->type, msg->data, msg->dataBytes, &index);
        if (published != HS_IPC_OK) {
            return published;
        }
        const int rc = sendEncoded(session, msg, HS_IPC_FLAG_SLOT, index);
        if (rc != HS_IPC_OK) {
            session->transport->releaseSlot(msg->type, index);
        }
        return rc;
    }
    return sendEncoded(session, msg, 0, 0);
}

int hs_ipc_poll(HsIpcSession* session, HsIpcMessage* msg, int timeoutMs)
{
    if (!session || !session->transport || !msg) {
        return HS_IPC_ERR_ARG;
    }
    std::vector<uint8_t> frame;
    const int received = session->transport->recvFrame(&frame, timeoutMs);
    if (received != HS_IPC_OK) {
        return received;
    }
    HsIpc::Decoded decoded;
    std::string err;
    if (!HsIpc::parseFrame(frame.empty() ? 0 : &frame[0], (uint32_t)frame.size(), &decoded, &err)) {
        return HS_IPC_ERR_PROTOCOL;
    }
    if (decoded.head.type == HS_IPC_HELLO && decoded.helloVersion != HS_IPC_VERSION) {
        return HS_IPC_ERR_PROTOCOL;
    }
    if (decoded.head.flags & HS_IPC_FLAG_SLOT) {
        const int copied = session->transport->copySlot(decoded.head.type, decoded.slotIndex, &decoded.data);
        session->transport->releaseSlot(decoded.head.type, decoded.slotIndex);
        if (copied != HS_IPC_OK) {
            return copied;
        }
    }
    publish(session, decoded, msg);
    session->lastRecv = GetTickCount64();
    return HS_IPC_OK;
}

uint64_t hs_ipc_last_recv_ms(HsIpcSession* session)
{
    return session ? session->lastRecv : 0;
}
