/**
 * @file shm.cpp
 * @brief 共享内存：信号环、统计最新值、图像和文件各三槽
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 布局：头、统计缓冲、信号环、3 个图像槽、3 个文件槽。
 * 图像和文件先占一个空槽，再往环里放一条带槽号的短帧。环满或没有空槽都返回满。
 * 统计不进环，后来的覆盖先前的。写的人只占用 state 为空的槽，不覆盖还没取走的。
 */
#include "transport.h"
#include "codec.h"
#include "names.h"

#include <string.h>

namespace HsIpc {

static const uint32_t kShmMagic = 0x4D485348u;
static const uint32_t kRingSlotBytes = 64u * 1024u;
static const uint32_t kSlotFree = 0;
static const uint32_t kSlotWriting = 1;
static const uint32_t kSlotReady = 2;

static uint32_t align8(uint32_t v)
{
    return (v + 7u) & ~7u;
}

struct ShmHead {
    uint32_t magic;
    uint32_t version;
    uint32_t signalCapacity;
    uint32_t signalSlotBytes;
    uint32_t imageCount;
    uint32_t imageBytes;
    uint32_t fileCount;
    uint32_t fileBytes;
    uint32_t statsSeq;
    uint32_t statsBytes;
    uint32_t writePos;
    uint32_t readPos;
};

struct SlotHead {
    uint32_t state;
    uint32_t seq;
    uint32_t bytes;
    uint32_t reserved;
};

struct ItemHead {
    uint32_t bytes;
    uint32_t reserved;
};

class ShmTransport : public Transport {
public:
    ShmTransport()
        : m_head(0)
        , m_stats(0)
        , m_items(0)
        , m_images(0)
        , m_files(0)
        , m_signalEvent(0)
        , m_slotEvent(0)
        , m_deliveredStats(0)
    {
    }

    ~ShmTransport() { close(); }

    bool open(const char* name, const char* host, int port, int capacity,
        uint32_t imageSlotBytes, uint32_t fileSlotBytes, std::string* err)
    {
        (void)host;
        (void)port;
        if (capacity <= 0) {
            capacity = 32;
        }
        if (imageSlotBytes == 0) {
            imageSlotBytes = HS_IPC_CHUNK_BYTES;
        }
        if (fileSlotBytes == 0) {
            fileSlotBytes = HS_IPC_CHUNK_BYTES;
        }
        const uint32_t itemBytes = sizeof(ItemHead) + kRingSlotBytes;
        const uint32_t imageStride = sizeof(SlotHead) + imageSlotBytes;
        const uint32_t fileStride = sizeof(SlotHead) + fileSlotBytes;
        uint32_t off = (uint32_t)sizeof(ShmHead);
        off = align8(off);
        const uint32_t statsAt = off;
        off += kRingSlotBytes;
        off = align8(off);
        const uint32_t itemsAt = off;
        off += (uint32_t)capacity * itemBytes;
        off = align8(off);
        const uint32_t imagesAt = off;
        off += 3u * imageStride;
        off = align8(off);
        const uint32_t filesAt = off;
        off += 3u * fileStride;

        const std::wstring base = L"HsIpc." + widen(name);
        bool created = false;
        if (!m_kernel.openMapping(base + L".mem", off, &created, err)) {
            return false;
        }
        if (!m_kernel.openMutex(base + L".mtx", err)) {
            return false;
        }
        Security security;
        m_signalEvent = CreateEventW(security.attr(), FALSE, FALSE,
            objectName(m_kernel.global(), base + L".sig").c_str());
        m_slotEvent = CreateEventW(security.attr(), FALSE, FALSE,
            objectName(m_kernel.global(), base + L".slot").c_str());
        if (!m_signalEvent || !m_slotEvent) {
            if (err) {
                *err = "创建共享内存事件失败";
            }
            return false;
        }
        uint8_t* view = (uint8_t*)m_kernel.view();
        m_head = (ShmHead*)view;
        if (created) {
            memset(view, 0, off);
            m_head->version = HS_IPC_VERSION;
            m_head->signalCapacity = (uint32_t)capacity;
            m_head->signalSlotBytes = kRingSlotBytes;
            m_head->imageCount = 3;
            m_head->imageBytes = imageSlotBytes;
            m_head->fileCount = 3;
            m_head->fileBytes = fileSlotBytes;
            MemoryBarrier();
            m_head->magic = kShmMagic;
        } else {
            for (int i = 0; i < 2000 && m_head->magic != kShmMagic; ++i) {
                Sleep(1);
            }
            if (m_head->magic != kShmMagic) {
                if (err) {
                    *err = "共享内存尚未初始化";
                }
                return false;
            }
        }
        m_stats = view + statsAt;
        m_items = view + itemsAt;
        m_images = view + imagesAt;
        m_files = view + filesAt;
        m_itemBytes = sizeof(ItemHead) + m_head->signalSlotBytes;
        m_imageStride = sizeof(SlotHead) + m_head->imageBytes;
        m_fileStride = sizeof(SlotHead) + m_head->fileBytes;
        return true;
    }

    void close()
    {
        if (m_signalEvent) {
            CloseHandle(m_signalEvent);
            m_signalEvent = 0;
        }
        if (m_slotEvent) {
            CloseHandle(m_slotEvent);
            m_slotEvent = 0;
        }
        m_kernel.close();
        m_head = 0;
    }

    int sendFrame(const uint8_t* frame, uint32_t bytes)
    {
        if (!m_head || !frame || bytes < sizeof(FrameHeader)) {
            return HS_IPC_ERR_ARG;
        }
        FrameHeader head;
        memcpy(&head, frame, sizeof(head));
        if (!m_kernel.lock(5000)) {
            return HS_IPC_ERR_IO;
        }
        int rc = HS_IPC_OK;
        if (head.type == HS_IPC_STATS) {
            if (bytes > kRingSlotBytes) {
                rc = HS_IPC_ERR_SIZE;
            } else {
                memcpy(m_stats, frame, bytes);
                m_head->statsBytes = bytes;
                m_head->statsSeq += 1;
            }
        } else if (bytes > m_head->signalSlotBytes) {
            rc = HS_IPC_ERR_SIZE;
        } else if (m_head->writePos - m_head->readPos >= m_head->signalCapacity) {
            rc = HS_IPC_ERR_FULL;
        } else {
            uint8_t* slot = m_items + (size_t)(m_head->writePos % m_head->signalCapacity) * m_itemBytes;
            ItemHead item;
            item.bytes = bytes;
            item.reserved = 0;
            memcpy(slot, &item, sizeof(item));
            memcpy(slot + sizeof(item), frame, bytes);
            m_head->writePos += 1;
        }
        m_kernel.unlock();
        if (rc == HS_IPC_OK) {
            SetEvent(m_signalEvent);
        }
        return rc;
    }

    int publishSlot(int slotKind, const uint8_t* pixels, uint32_t n, uint32_t* index)
    {
        const bool image = slotKind == HS_IPC_IMAGE;
        const uint32_t limit = image ? m_head->imageBytes : m_head->fileBytes;
        if (n > limit) {
            return HS_IPC_ERR_SIZE;
        }
        if (!m_kernel.lock(5000)) {
            return HS_IPC_ERR_IO;
        }
        uint8_t* base = image ? m_images : m_files;
        const uint32_t stride = image ? m_imageStride : m_fileStride;
        const uint32_t count = image ? m_head->imageCount : m_head->fileCount;
        uint32_t found = count;
        for (uint32_t i = 0; i < count; ++i) {
            SlotHead* candidate = (SlotHead*)(base + (size_t)i * stride);
            if (candidate->state == kSlotFree) {
                found = i;
                break;
            }
        }
        if (found == count) {
            m_kernel.unlock();
            return HS_IPC_ERR_FULL;
        }
        SlotHead* slot = (SlotHead*)(base + (size_t)found * stride);
        slot->state = kSlotWriting;
        slot->bytes = n;
        slot->seq += 1;
        if (n && pixels) {
            memcpy((uint8_t*)slot + sizeof(SlotHead), pixels, n);
        }
        slot->state = kSlotReady;
        m_kernel.unlock();
        SetEvent(m_slotEvent);
        *index = found;
        return HS_IPC_OK;
    }

    int recvFrame(std::vector<uint8_t>* frame, int timeoutMs)
    {
        const int ready = take(frame);
        if (ready == HS_IPC_OK) {
            return HS_IPC_OK;
        }
        HANDLE events[2] = { m_signalEvent, m_slotEvent };
        const DWORD wait = WaitForMultipleObjects(2, events, FALSE, timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs);
        if (wait == WAIT_TIMEOUT) {
            return take(frame) == HS_IPC_OK ? HS_IPC_OK : HS_IPC_TIMEOUT;
        }
        if (wait != WAIT_OBJECT_0 && wait != WAIT_OBJECT_0 + 1) {
            return HS_IPC_ERR_IO;
        }
        return take(frame) == HS_IPC_OK ? HS_IPC_OK : HS_IPC_TIMEOUT;
    }

    int copySlot(int slotKind, uint32_t index, std::vector<uint8_t>* pixels)
    {
        if (!m_kernel.lock(5000)) {
            return HS_IPC_ERR_IO;
        }
        SlotHead* slot = slotAt(slotKind, index);
        if (!slot || slot->state != kSlotReady) {
            m_kernel.unlock();
            return HS_IPC_ERR_PROTOCOL;
        }
        pixels->assign((uint8_t*)slot + sizeof(SlotHead), (uint8_t*)slot + sizeof(SlotHead) + slot->bytes);
        m_kernel.unlock();
        return HS_IPC_OK;
    }

    void releaseSlot(int slotKind, uint32_t index)
    {
        if (!m_kernel.lock(5000)) {
            return;
        }
        SlotHead* slot = slotAt(slotKind, index);
        if (slot) {
            slot->state = kSlotFree;
        }
        m_kernel.unlock();
    }

private:
    SlotHead* slotAt(int slotKind, uint32_t index)
    {
        const bool image = slotKind == HS_IPC_IMAGE;
        const uint32_t count = image ? m_head->imageCount : m_head->fileCount;
        if (index >= count) {
            return 0;
        }
        uint8_t* base = image ? m_images : m_files;
        const uint32_t stride = image ? m_imageStride : m_fileStride;
        return (SlotHead*)(base + (size_t)index * stride);
    }

    int take(std::vector<uint8_t>* frame)
    {
        if (!m_kernel.lock(5000)) {
            return HS_IPC_ERR_IO;
        }
        if (m_head->readPos != m_head->writePos) {
            uint8_t* slot = m_items + (size_t)(m_head->readPos % m_head->signalCapacity) * m_itemBytes;
            ItemHead item;
            memcpy(&item, slot, sizeof(item));
            if (item.bytes > m_head->signalSlotBytes) {
                m_head->readPos += 1;
                m_kernel.unlock();
                return HS_IPC_ERR_PROTOCOL;
            }
            frame->assign(slot + sizeof(ItemHead), slot + sizeof(ItemHead) + item.bytes);
            m_head->readPos += 1;
            m_kernel.unlock();
            return HS_IPC_OK;
        }
        if (m_head->statsSeq != m_deliveredStats && m_head->statsBytes > 0
            && m_head->statsBytes <= kRingSlotBytes) {
            frame->assign(m_stats, m_stats + m_head->statsBytes);
            m_deliveredStats = m_head->statsSeq;
            m_kernel.unlock();
            return HS_IPC_OK;
        }
        m_kernel.unlock();
        return HS_IPC_TIMEOUT;
    }

    Kernel m_kernel;
    ShmHead* m_head;
    uint8_t* m_stats;
    uint8_t* m_items;
    uint8_t* m_images;
    uint8_t* m_files;
    uint32_t m_itemBytes;
    uint32_t m_imageStride;
    uint32_t m_fileStride;
    HANDLE m_signalEvent;
    HANDLE m_slotEvent;
    uint32_t m_deliveredStats;
};

Transport* createShm()
{
    return new ShmTransport();
}

}
