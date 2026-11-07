/**
 * @file queue.cpp
 * @brief 命名消息队列。每条帧留到被取走，满了返回失败
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 队列本身放在共享内存里，不安装 MSMQ。和共享内存传输的槽不同：
 * 这里每一条都保留，图像跟在帧后面，不另占槽。单槽能放下一个分片。
 */
#include "transport.h"
#include "names.h"

#include <string.h>

namespace HsIpc {

static const uint32_t kQueueMagic = 0x51484853u;

struct QueueHead {
    uint32_t magic;
    uint32_t version;
    uint32_t capacity;
    uint32_t slotBytes;
    uint32_t writePos;
    uint32_t readPos;
};

struct QueueItem {
    uint32_t bytes;
    uint32_t reserved;
};

class QueueTransport : public Transport {
public:
    QueueTransport() : m_head(0), m_slots(0), m_stride(0) {}
    ~QueueTransport() { close(); }

    bool open(const char* name, const char* host, int port, int capacity,
        uint32_t imageSlotBytes, uint32_t fileSlotBytes, std::string* err)
    {
        (void)host;
        (void)port;
        (void)imageSlotBytes;
        (void)fileSlotBytes;
        if (capacity <= 0) {
            capacity = 32;
        }
        const uint32_t slotBytes = HS_IPC_CHUNK_BYTES + 8192u;
        m_stride = sizeof(QueueItem) + slotBytes;
        const uint64_t bytes = sizeof(QueueHead) + (uint64_t)capacity * m_stride;
        const std::wstring base = L"HsIpc." + widen(name) + L".q";
        bool created = false;
        if (!m_kernel.openMapping(base + L".mem", bytes, &created, err)) {
            return false;
        }
        if (!m_kernel.openMutex(base + L".mtx", err)) {
            return false;
        }
        if (!m_kernel.openEvent(base + L".ev", err)) {
            return false;
        }
        m_head = (QueueHead*)m_kernel.view();
        m_slots = (uint8_t*)m_kernel.view() + sizeof(QueueHead);
        if (created) {
            memset(m_kernel.view(), 0, (size_t)bytes);
            m_head->version = HS_IPC_VERSION;
            m_head->capacity = (uint32_t)capacity;
            m_head->slotBytes = slotBytes;
            MemoryBarrier();
            m_head->magic = kQueueMagic;
        } else {
            for (int i = 0; i < 2000 && m_head->magic != kQueueMagic; ++i) {
                Sleep(1);
            }
            if (m_head->magic != kQueueMagic) {
                if (err) {
                    *err = "消息队列尚未初始化";
                }
                return false;
            }
            m_stride = sizeof(QueueItem) + m_head->slotBytes;
        }
        return true;
    }

    void close()
    {
        m_kernel.close();
        m_head = 0;
    }

    int sendFrame(const uint8_t* frame, uint32_t bytes)
    {
        if (!m_head || !frame) {
            return HS_IPC_ERR_ARG;
        }
        if (bytes > m_head->slotBytes) {
            return HS_IPC_ERR_SIZE;
        }
        if (!m_kernel.lock(5000)) {
            return HS_IPC_ERR_IO;
        }
        if (m_head->writePos - m_head->readPos >= m_head->capacity) {
            m_kernel.unlock();
            return HS_IPC_ERR_FULL;
        }
        uint8_t* slot = m_slots + (size_t)(m_head->writePos % m_head->capacity) * m_stride;
        QueueItem item;
        item.bytes = bytes;
        item.reserved = 0;
        memcpy(slot, &item, sizeof(item));
        memcpy(slot + sizeof(item), frame, bytes);
        m_head->writePos += 1;
        m_kernel.unlock();
        m_kernel.signal();
        return HS_IPC_OK;
    }

    int recvFrame(std::vector<uint8_t>* frame, int timeoutMs)
    {
        const int ready = take(frame);
        if (ready == HS_IPC_OK) {
            return HS_IPC_OK;
        }
        const int waited = m_kernel.waitEvent(timeoutMs);
        if (waited != 0) {
            return take(frame) == HS_IPC_OK ? HS_IPC_OK : waited;
        }
        return take(frame) == HS_IPC_OK ? HS_IPC_OK : HS_IPC_TIMEOUT;
    }

private:
    int take(std::vector<uint8_t>* frame)
    {
        if (!m_kernel.lock(5000)) {
            return HS_IPC_ERR_IO;
        }
        if (m_head->readPos == m_head->writePos) {
            m_kernel.unlock();
            return HS_IPC_TIMEOUT;
        }
        uint8_t* slot = m_slots + (size_t)(m_head->readPos % m_head->capacity) * m_stride;
        QueueItem item;
        memcpy(&item, slot, sizeof(item));
        if (item.bytes > m_head->slotBytes) {
            m_head->readPos += 1;
            m_kernel.unlock();
            return HS_IPC_ERR_PROTOCOL;
        }
        frame->assign(slot + sizeof(item), slot + sizeof(item) + item.bytes);
        m_head->readPos += 1;
        m_kernel.unlock();
        return HS_IPC_OK;
    }

    Kernel m_kernel;
    QueueHead* m_head;
    uint8_t* m_slots;
    uint32_t m_stride;
};

Transport* createQueue()
{
    return new QueueTransport();
}

}
