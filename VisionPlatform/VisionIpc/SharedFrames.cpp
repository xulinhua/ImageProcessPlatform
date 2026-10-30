/**
 * @file SharedFrames.cpp
 * @brief 共享内存映射、命令事件和画面槽
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SharedFrames.h"
#include "SharedRegion.h"
#include "NamedEvent.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstring>

namespace Vision {

namespace {

const uint32_t kMagic = 0x43495056; ///< 字节 VPIC
const uint32_t kVersion = 1;
const uint32_t kMinSlot = 64 * 1024;

// 固定前缀，后面才是 JPEG。三进程按同一布局读写
struct SlotPod {
    uint32_t frameSeq;
    uint32_t jpegSize;
    uint32_t ok;
    int32_t cycleMs;
    int32_t total;
    int32_t pass;
    int32_t fail;
    uint32_t offline;
    char stationId[40];
    char nodeId[40];
    char summary[80];
};

struct StationPod {
    char stationId[64];
    char nodeId[64];
    uint32_t seq;  ///< 奇数表示正在写，偶数表示可以读
    uint32_t slot; ///< 已发布的副本下标
    uint32_t offline;
};

struct HeadPod {
    uint32_t magic;
    uint32_t version;
    uint32_t slotBytes;
    uint32_t command;
    uint32_t commandSeq;
    uint32_t commandArg;
    uint32_t subscribe;
    uint32_t role;
    uint32_t fault;
    uint64_t stationBeatMs;
    uint64_t linkBeatMs;
    char nodeId[64];
    char faultText[128];
    uint32_t localCount;
    uint32_t remoteCount;
    StationPod local[SharedFrames::kMaxLocal];
    StationPod remote[SharedFrames::kMaxRemote];
};

}

struct SharedFrames::Transport {
    Ipc::SharedRegion region;
    Ipc::NamedEvent command;
    Ipc::NamedEvent frame;
};

struct SharedFrames::Head {
    HeadPod pod;
};

struct SharedFrames::Slot {
    SlotPod pod;
};

SharedFrames& SharedFrames::instance()
{
    static SharedFrames frames;
    return frames;
}

SharedFrames::SharedFrames()
    : m_view(0)
    , m_head(0)
    , m_transport(0)
    , m_slotBytes(0)
{
}

SharedFrames::~SharedFrames()
{
    delete m_transport;
    m_transport = 0;
    m_view = 0;
    m_head = 0;
}

void SharedFrames::copyText(char* dst, int cap, const std::string& src) const
{
    if (!dst || cap <= 0) {
        return;
    }
    const int n = static_cast<int>(src.size());
    const int keep = n < cap - 1 ? n : cap - 1;
    if (keep > 0) {
        std::memcpy(dst, src.data(), static_cast<size_t>(keep));
    }
    dst[keep] = 0;
}

uint64_t SharedFrames::nowMs() const
{
    return GetTickCount64();
}

uint32_t SharedFrames::layoutBytes(uint32_t slotBytes) const
{
    const uint64_t slots = static_cast<uint64_t>(kMaxLocal * kLocalCopies + kMaxRemote * kRemoteCopies);
    const uint64_t bytes = static_cast<uint64_t>(sizeof(HeadPod)) + slots * slotBytes;
    return bytes > 0xFFFFFFFFull ? 0 : static_cast<uint32_t>(bytes);
}

bool SharedFrames::failMap(Transport* transport, std::string* err, const char* reason) const
{
    delete transport;
    if (err && reason) {
        *err = reason;
    }
    return false;
}

bool SharedFrames::map(uint32_t slotBytes, std::string* err)
{
    if (m_view) {
        return true;
    }
    if (slotBytes < kMinSlot) {
        slotBytes = kMinSlot;
    }
    const uint32_t want = layoutBytes(slotBytes);
    if (want == 0) {
        return failMap(0, err, "画面槽过大");
    }
    Transport* t = new Transport();
    // 已有对象比本进程要的小时整块映射会失败；任何进程建的都不小于最小布局，按它再开一次读头部
    std::string wantErr;
    if (!t->region.open(L"Vision.Ipc", want, &wantErr)) {
        if (!t->region.open(L"Vision.Ipc", layoutBytes(kMinSlot), err)) {
            return failMap(t, err, 0);
        }
        if (t->region.created()) {
            return failMap(t, err, "共享内存在打开过程中被释放，请重试");
        }
    }
    Head* head = reinterpret_cast<Head*>(t->region.data());
    if (t->region.created() || head->pod.magic != kMagic || head->pod.version != kVersion) {
        // 新建或头部无效：按本次映射能容下的槽大小写头部
        if (layoutBytes(slotBytes) > t->region.size()) {
            slotBytes = kMinSlot;
        }
        std::memset(t->region.data(), 0, t->region.size());
        head->pod.magic = kMagic;
        head->pod.version = kVersion;
        head->pod.slotBytes = slotBytes;
    } else {
        // 布局以建立者写入头部的槽大小为准，本进程传入的值只在新建时生效
        const uint32_t recorded = head->pod.slotBytes;
        const uint32_t need = recorded < kMinSlot ? 0 : layoutBytes(recorded);
        if (need == 0) {
            return failMap(t, err, "共享内存头部记录的槽大小无效");
        }
        if (need > t->region.size()) {
            t->region.close();
            if (!t->region.open(L"Vision.Ipc", need, err)) {
                return failMap(t, err, 0);
            }
            head = reinterpret_cast<Head*>(t->region.data());
            if (t->region.created() || head->pod.magic != kMagic || head->pod.version != kVersion
                || head->pod.slotBytes != recorded) {
                return failMap(t, err, "共享内存在打开过程中被重建，请重试");
            }
        }
        slotBytes = recorded;
    }
    if (!t->command.open(t->region.global(), L"Vision.Command")) {
        return failMap(t, err, "打开命令事件失败");
    }
    if (!t->frame.open(t->region.global(), L"Vision.FrameReady")) {
        return failMap(t, err, "打开新帧事件失败");
    }
    m_transport = t;
    m_view = t->region.data();
    m_head = head;
    m_slotBytes = slotBytes;
    return true;
}

bool SharedFrames::mapped() const
{
    return m_head != 0;
}

void SharedFrames::writeCommand(IpcCommand cmd, uint32_t arg)
{
    if (!m_head) {
        return;
    }
    m_head->pod.command = static_cast<uint32_t>(cmd);
    m_head->pod.commandArg = arg;
    m_head->pod.commandSeq += 1;
    if (cmd == IpcCmdSetSubscribe) {
        m_head->pod.subscribe = arg ? 1u : 0u;
    }
    m_transport->command.signal();
}

IpcCommand SharedFrames::waitCommand(int timeoutMs, uint32_t* arg, uint32_t* seq)
{
    if (!m_transport || !m_transport->command.wait(timeoutMs) || !m_head) {
        return IpcCmdNone;
    }
    if (arg) {
        *arg = m_head->pod.commandArg;
    }
    if (seq) {
        *seq = m_head->pod.commandSeq;
    }
    return static_cast<IpcCommand>(m_head->pod.command);
}

void SharedFrames::setSubscribe(bool on)
{
    writeCommand(IpcCmdSetSubscribe, on ? 1u : 0u);
}

bool SharedFrames::subscribe() const
{
    return m_head && m_head->pod.subscribe != 0;
}

void SharedFrames::beatStation()
{
    if (m_head) {
        m_head->pod.stationBeatMs = nowMs();
    }
}

void SharedFrames::beatLink()
{
    if (m_head) {
        m_head->pod.linkBeatMs = nowMs();
    }
}

int SharedFrames::stationBeatAgeMs() const
{
    if (!m_head || m_head->pod.stationBeatMs == 0) {
        return -1;
    }
    const uint64_t age = nowMs() - m_head->pod.stationBeatMs;
    return age > 0x7fffffff ? 0x7fffffff : static_cast<int>(age);
}

int SharedFrames::linkBeatAgeMs() const
{
    if (!m_head || m_head->pod.linkBeatMs == 0) {
        return -1;
    }
    const uint64_t age = nowMs() - m_head->pod.linkBeatMs;
    return age > 0x7fffffff ? 0x7fffffff : static_cast<int>(age);
}

void SharedFrames::setFault(IpcFault fault, const std::string& reason)
{
    if (!m_head) {
        return;
    }
    m_head->pod.fault = static_cast<uint32_t>(fault);
    copyText(m_head->pod.faultText, static_cast<int>(sizeof(m_head->pod.faultText)), reason);
}

IpcFault SharedFrames::fault() const
{
    if (!m_head) {
        return IpcFaultDown;
    }
    return static_cast<IpcFault>(m_head->pod.fault);
}

std::string SharedFrames::faultText() const
{
    if (!m_head) {
        return std::string();
    }
    return std::string(m_head->pod.faultText);
}

void SharedFrames::setRole(NodeRole role)
{
    if (m_head) {
        m_head->pod.role = static_cast<uint32_t>(role);
    }
}

NodeRole SharedFrames::role() const
{
    if (!m_head) {
        return NodeRoleStandalone;
    }
    return static_cast<NodeRole>(m_head->pod.role);
}

int SharedFrames::bindLocal(const std::string& stationId, const std::string& nodeId)
{
    if (!m_head) {
        return -1;
    }
    for (uint32_t i = 0; i < m_head->pod.localCount && i < kMaxLocal; ++i) {
        if (stationId == m_head->pod.local[i].stationId) {
            return static_cast<int>(i);
        }
    }
    if (m_head->pod.localCount >= kMaxLocal) {
        return -1;
    }
    const uint32_t i = m_head->pod.localCount++;
    copyText(m_head->pod.local[i].stationId, 64, stationId);
    copyText(m_head->pod.local[i].nodeId, 64, nodeId);
    m_head->pod.local[i].seq = 0;
    m_head->pod.local[i].slot = 0;
    return static_cast<int>(i);
}

int SharedFrames::bindRemote(const std::string& stationId, const std::string& nodeId)
{
    if (!m_head) {
        return -1;
    }
    for (uint32_t i = 0; i < m_head->pod.remoteCount && i < kMaxRemote; ++i) {
        if (stationId == m_head->pod.remote[i].stationId) {
            return static_cast<int>(i);
        }
    }
    if (m_head->pod.remoteCount >= kMaxRemote) {
        return -1;
    }
    const uint32_t i = m_head->pod.remoteCount++;
    copyText(m_head->pod.remote[i].stationId, 64, stationId);
    copyText(m_head->pod.remote[i].nodeId, 64, nodeId);
    m_head->pod.remote[i].seq = 0;
    m_head->pod.remote[i].slot = 0;
    return static_cast<int>(i);
}

SharedFrames::Slot* SharedFrames::slotAt(bool local, int index, int copy) const
{
    if (!m_view || index < 0 || copy < 0) {
        return 0;
    }
    const int base = local ? index * kLocalCopies + copy
                           : kMaxLocal * kLocalCopies + index * kRemoteCopies + copy;
    unsigned char* p = reinterpret_cast<unsigned char*>(m_view) + sizeof(HeadPod)
        + static_cast<size_t>(base) * m_slotBytes;
    return reinterpret_cast<Slot*>(p);
}

bool SharedFrames::publishAt(bool local, int index, int copies, uint32_t frameSeq, bool ok,
                            int cycleMs, int total, int pass, int fail, bool offline,
                            const std::string& summary, const std::vector<unsigned char>& jpeg)
{
    if (!m_head || index < 0 || index >= (local ? kMaxLocal : kMaxRemote)) {
        return false;
    }
    const uint32_t prefix = static_cast<uint32_t>(sizeof(SlotPod));
    if (prefix >= m_slotBytes || jpeg.size() > m_slotBytes - prefix) {
        return false;
    }
    StationPod* st = local ? &m_head->pod.local[index] : &m_head->pod.remote[index];
    const int next = (static_cast<int>(st->slot) + 1) % copies;
    st->seq = (st->seq + 1) | 1u;
    Slot* slot = slotAt(local, index, next);
    if (!slot) {
        st->seq = (st->seq + 1) & ~1u;
        return false;
    }
    slot->pod.frameSeq = frameSeq;
    slot->pod.jpegSize = static_cast<uint32_t>(jpeg.size());
    slot->pod.ok = ok ? 1u : 0u;
    slot->pod.cycleMs = cycleMs;
    slot->pod.total = total;
    slot->pod.pass = pass;
    slot->pod.fail = fail;
    slot->pod.offline = offline ? 1u : 0u;
    copyText(slot->pod.stationId, 40, st->stationId);
    copyText(slot->pod.nodeId, 40, st->nodeId);
    copyText(slot->pod.summary, 80, summary);
    if (!jpeg.empty()) {
        std::memcpy(reinterpret_cast<unsigned char*>(slot) + prefix, jpeg.data(), jpeg.size());
    }
    st->slot = static_cast<uint32_t>(next);
    st->seq = (st->seq + 1) & ~1u;
    st->offline = offline ? 1u : 0u;
    return true;
}

bool SharedFrames::publishLocal(int index, uint32_t frameSeq, bool ok, int cycleMs,
                               int total, int pass, int fail, const std::string& summary,
                               const std::vector<unsigned char>& jpeg)
{
    return publishAt(true, index, kLocalCopies, frameSeq, ok, cycleMs, total, pass, fail, false, summary, jpeg);
}

bool SharedFrames::publishRemote(int index, uint32_t frameSeq, bool ok, int cycleMs,
                                int total, int pass, int fail, bool offline,
                                const std::string& summary,
                                const std::vector<unsigned char>& jpeg)
{
    return publishAt(false, index, kRemoteCopies, frameSeq, ok, cycleMs, total, pass, fail, offline, summary, jpeg);
}

bool SharedFrames::readAt(bool local, int index, int copies, IpcPreview* out) const
{
    if (!out || !m_head || index < 0) {
        return false;
    }
    const StationPod* st = local ? &m_head->pod.local[index] : &m_head->pod.remote[index];
    if ((st->seq & 1u) != 0) {
        return false;
    }
    const int copy = static_cast<int>(st->slot) % copies;
    const uint32_t seen = st->seq;
    const Slot* slot = slotAt(local, index, copy);
    if (!slot || slot->pod.jpegSize > m_slotBytes) {
        return false;
    }
    if (st->seq != seen || (st->seq & 1u) != 0) {
        return false;
    }
    out->stationId = slot->pod.stationId;
    out->nodeId = slot->pod.nodeId;
    out->summary = slot->pod.summary;
    out->ok = slot->pod.ok != 0;
    out->offline = slot->pod.offline != 0;
    out->remote = !local;
    out->cycleMs = slot->pod.cycleMs;
    out->total = slot->pod.total;
    out->pass = slot->pod.pass;
    out->fail = slot->pod.fail;
    out->frameSeq = slot->pod.frameSeq;
    out->jpeg.resize(slot->pod.jpegSize);
    if (slot->pod.jpegSize > 0) {
        std::memcpy(out->jpeg.data(), reinterpret_cast<const unsigned char*>(slot) + sizeof(SlotPod), slot->pod.jpegSize);
    }
    return true;
}

bool SharedFrames::readLocal(int index, IpcPreview* out) const
{
    return readAt(true, index, kLocalCopies, out);
}

bool SharedFrames::readRemote(int index, IpcPreview* out) const
{
    return readAt(false, index, kRemoteCopies, out);
}

int SharedFrames::localCount() const
{
    return m_head ? static_cast<int>(m_head->pod.localCount) : 0;
}

int SharedFrames::remoteCount() const
{
    return m_head ? static_cast<int>(m_head->pod.remoteCount) : 0;
}

std::string SharedFrames::localStationId(int index) const
{
    if (!m_head || index < 0 || index >= static_cast<int>(m_head->pod.localCount)) {
        return std::string();
    }
    return std::string(m_head->pod.local[index].stationId);
}

std::string SharedFrames::remoteStationId(int index) const
{
    if (!m_head || index < 0 || index >= static_cast<int>(m_head->pod.remoteCount)) {
        return std::string();
    }
    return std::string(m_head->pod.remote[index].stationId);
}

void SharedFrames::signalFrame()
{
    if (m_transport) {
        m_transport->frame.signal();
    }
}

bool SharedFrames::waitFrame(int timeoutMs)
{
    return m_transport && m_transport->frame.wait(timeoutMs);
}

}
