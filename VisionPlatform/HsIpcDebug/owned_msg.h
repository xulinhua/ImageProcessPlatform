/**
 * @file owned_msg.h
 * @brief 调试器自己持有的报文副本
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * HsIpcMessage 里的指针只在一次 send 或 poll 期间有效。
 * 界面和工作线程之间传递时，字符串和像素都放在这份结构里。
 */

#ifndef HS_IPC_DEBUG_OWNED_MSG_H
#define HS_IPC_DEBUG_OWNED_MSG_H

#include "hs_ipc.h"

#include <cstring>
#include <string>
#include <vector>

/** @brief 发送前由界面填好，工作线程再转成 HsIpcMessage */
struct OwnedMsg {
    uint16_t type;
    std::string name;
    std::string text;
    std::string path;
    int32_t result;
    int32_t code;
    std::vector<std::string> statKeys;
    std::vector<HsIpcStat> stats;
    uint16_t format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t frameSeq;
    std::vector<uint8_t> data;
    uint64_t totalBytes;
    uint32_t crc32;
    uint32_t ackSeq;
    int32_t ackResult;

    OwnedMsg()
        : type(HS_IPC_SIGNAL)
        , result(HS_IPC_RESULT_OK)
        , code(0)
        , format(HS_IPC_IMAGE_JPEG)
        , width(0)
        , height(0)
        , stride(0)
        , frameSeq(0)
        , totalBytes(0)
        , crc32(0)
        , ackSeq(0)
        , ackResult(0)
    {}

    /** @brief 指针指向本对象内部缓冲，返回后不要再改这些 vector */
    HsIpcMessage toMessage()
    {
        for (size_t i = 0; i < stats.size() && i < statKeys.size(); ++i) {
            stats[i].key = statKeys[i].c_str();
        }
        HsIpcMessage msg;
        std::memset(&msg, 0, sizeof(msg));
        msg.type = type;
        msg.name = name.c_str();
        msg.result = result;
        msg.code = code;
        msg.text = text.c_str();
        msg.statCount = (int)stats.size();
        msg.stats = stats.empty() ? 0 : &stats[0];
        msg.format = format;
        msg.width = width;
        msg.height = height;
        msg.stride = stride;
        msg.frameSeq = frameSeq;
        msg.data = data.empty() ? 0 : &data[0];
        msg.dataBytes = (uint32_t)data.size();
        msg.totalBytes = totalBytes;
        msg.crc32 = crc32;
        msg.path = path.c_str();
        msg.ackSeq = ackSeq;
        msg.ackResult = ackResult;
        return msg;
    }
};

/** @brief 返回码的中文，界面和记录共用 */
inline const char* ipcCodeText(int code)
{
    if (code == HS_IPC_OK) {
        return "成功";
    }
    if (code == HS_IPC_TIMEOUT) {
        return "超时";
    }
    if (code == HS_IPC_ERR_ARG) {
        return "参数错";
    }
    if (code == HS_IPC_ERR_FULL) {
        return "满";
    }
    if (code == HS_IPC_ERR_IO) {
        return "IO";
    }
    if (code == HS_IPC_ERR_PROTOCOL) {
        return "协议";
    }
    if (code == HS_IPC_ERR_SIZE) {
        return "过大";
    }
    return "失败";
}

inline const char* ipcTypeText(uint16_t type)
{
    switch (type) {
    case HS_IPC_HELLO: return "Hello";
    case HS_IPC_HEARTBEAT: return "心跳";
    case HS_IPC_SIGNAL: return "信号";
    case HS_IPC_STATS: return "统计";
    case HS_IPC_IMAGE: return "图像";
    case HS_IPC_FILE: return "文件";
    case HS_IPC_ACK: return "Ack";
    case HS_IPC_BYE: return "Bye";
    default: return "未知";
    }
}

#endif
