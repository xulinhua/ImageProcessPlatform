/**
 * @file SharedFrames.cpp
 * @brief 共享内存映射、命令事件和画面槽
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SharedFrames.h"

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

// 拷进定长字符数组，保证有结尾 0
void copyText(char* dst, int cap, const std::string& src)
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

// 服务和登录用户都能开。Global 失败时退回 Local，只在同一会话里可见
void* createMapping(uint32_t bytes, bool* global)
{
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;
    void* handle = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, bytes, L"Global\\Vision.Ipc");
    if (handle) {
        *global = true;
        return handle;
    }
    handle = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, bytes, L"Local\\Vision.Ipc");
    *global = false;
    return handle;
}

void* createEvent(bool global, const wchar_t* globalName, const wchar_t* localName)
{
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;
    return CreateEventW(&sa, FALSE, FALSE, global ? globalName : localName);
}

uint64_t nowMs()
{
    return GetTickCount64();
}

}

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
    , m_mapping(0)
    , m_command(0)
    , m_frame(0)
    , m_slotBytes(0)
{
}

SharedFrames::~SharedFrames()
{
    if (m_view) {
        UnmapViewOfFile(m_view);
        m_view = 0;
    }
    if (m_mapping) {
        CloseHandle(m_mapping);
        m_mapping = 0;
    }
    if (m_command) {
        CloseHandle(m_command);
        m_command = 0;
    }
    if (m_frame) {
        CloseHandle(m_frame);
        m_frame = 0;
    }
    m_head = 0;
}

bool SharedFrames::map(uint32_t slotBytes, std::string* err)
{
    if (m_view) {
        return true;
    }
    if (slotBytes < kMinSlot) {
        slotBytes = kMinSlot;
    }
    const uint32_t slots = static_cast<uint32_t>(kMaxLocal * kLocalCopies + kMaxRemote * kRemoteCopies);
    const uint32_t bytes = static_cast<uint32_t>(sizeof(HeadPod)) + slots * slotBytes;
    bool global = false;
    m_mapping = createMapping(bytes, &global);
    if (!m_mapping) {
        if (err) {
            *err = "创建共享内存失败";
        }
        return false;
    }
    m_view = MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (!m_view) {
        CloseHandle(m_mapping);
        m_mapping = 0;
        if (err) {
            *err = "映射共享内存失败";
        }
        return false;
    }
    m_head = reinterpret_cast<Head*>(m_view);
    m_slotBytes = slotBytes;
    const bool created = GetLastError() != ERROR_ALREADY_EXISTS;
    if (created || m_head->pod.magic != kMagic || m_head->pod.version != kVersion) {
        std::memset(m_view, 0, bytes);
        m_head->pod.magic = kMagic;
        m_head->pod.version = kVersion;
        m_head->pod.slotBytes = slotBytes;
    }
    m_command = createEvent(global, L"Global\\Vision.Command", L"Local\\Vision.Command");
    m_frame = createEvent(global, L"Global\\Vision.FrameReady", L"Local\\Vision.FrameReady");
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
    if (m_command) {
        SetEvent(m_command);
    }
}

IpcCommand SharedFrames::waitCommand(int timeoutMs, uint32_t* arg, uint32_t* seq)
{
    if (!m_command) {
        return IpcCmdNone;
    }
    const DWORD w = WaitForSingleObject(m_command, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
    if (w != WAIT_OBJECT_0 || !m_head) {
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
    if (m_frame) {
        SetEvent(m_frame);
    }
}

bool SharedFrames::waitFrame(int timeoutMs)
{
    if (!m_frame) {
        return false;
    }
    return WaitForSingleObject(m_frame, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs)) == WAIT_OBJECT_0;
}

}
