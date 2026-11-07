/**
 * @file transport.h
 * @brief 四种传输的共同接口。会话只交出完整的一帧
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 槽只在共享内存上有意义。管道、队列、套接字的 publishSlot 返回失败。
 */
#ifndef HS_IPC_TRANSPORT_H
#define HS_IPC_TRANSPORT_H

#include "hs_ipc.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace HsIpc {

class Transport {
public:
    virtual ~Transport() {}
    virtual bool open(const char* name, const char* host, int port, int capacity,
        uint32_t imageSlotBytes, uint32_t fileSlotBytes, std::string* err) = 0;
    virtual void close() = 0;
    virtual int sendFrame(const uint8_t* frame, uint32_t bytes) = 0;
    /** 共享内存：占一个空槽并写入像素。其它传输不使用。 */
    virtual int publishSlot(int slotKind, const uint8_t* pixels, uint32_t n, uint32_t* index)
    {
        (void)slotKind;
        (void)pixels;
        (void)n;
        (void)index;
        return HS_IPC_ERR_IO;
    }
    virtual int copySlot(int slotKind, uint32_t index, std::vector<uint8_t>* pixels)
    {
        (void)slotKind;
        (void)index;
        (void)pixels;
        return HS_IPC_ERR_IO;
    }
    virtual void releaseSlot(int slotKind, uint32_t index)
    {
        (void)slotKind;
        (void)index;
    }
    virtual int recvFrame(std::vector<uint8_t>* frame, int timeoutMs) = 0;
};

Transport* createShm();
Transport* createPipe();
Transport* createQueue();
Transport* createSocket();

}
#endif
