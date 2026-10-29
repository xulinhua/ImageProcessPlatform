/**
 * @file LinkService.cpp
 * @brief 节点 TCP 循环。套接字只通过 VisionComm 打开
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LinkService.h"
#include "AppPaths.h"
#include "CommTypes.h"
#include "SharedFrames.h"
#include "TextEncoding.h"
#include "VisionCfgMgr.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstring>

namespace Vision {

namespace {

void appendStr(std::vector<unsigned char>* buf, const std::string& s)
{
    const uint16_t n = static_cast<uint16_t>(s.size() > 65535 ? 65535 : s.size());
    buf->push_back(static_cast<unsigned char>(n & 0xff));
    buf->push_back(static_cast<unsigned char>((n >> 8) & 0xff));
    buf->insert(buf->end(), s.begin(), s.begin() + n);
}

bool takeStr(const unsigned char*& p, const unsigned char* end, std::string* out)
{
    if (end - p < 2) {
        return false;
    }
    const uint16_t n = static_cast<uint16_t>(p[0] | (p[1] << 8));
    p += 2;
    if (end - p < n) {
        return false;
    }
    out->assign(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p) + n);
    p += n;
    return true;
}

unsigned long long nowMs()
{
    return GetTickCount64();
}

}

LinkService::LinkService()
    : m_channel(ObjectId::fromString("b4e91a10-7c22-4f18-9a01-a0b1c2d3e4c1"))
    , m_server(false)
    , m_helloSent(false)
{
}

LinkService::~LinkService()
{
    CommMgr::instance().closeChannel(m_channel);
}

std::vector<unsigned char> LinkService::makeHello() const
{
    std::vector<unsigned char> buf;
    appendStr(&buf, m_node.nodeId);
    appendStr(&buf, m_node.nodeName);
    appendStr(&buf, m_deploy.solutionId);
    for (std::map<std::string, std::string>::const_iterator it = m_deploy.owners.begin();
         it != m_deploy.owners.end(); ++it) {
        if (it->second == m_node.nodeId) {
            appendStr(&buf, it->first);
        }
    }
    return buf;
}

bool LinkService::acceptHello(const std::vector<unsigned char>& payload, Peer* peer, std::string* reason) const
{
    if (!peer) {
        return false;
    }
    const unsigned char* p = payload.data();
    const unsigned char* end = p + payload.size();
    if (!takeStr(p, end, &peer->nodeId) || !takeStr(p, end, &peer->nodeName) || !takeStr(p, end, &peer->solutionId)) {
        if (reason) {
            *reason = "Hello 不完整";
        }
        return false;
    }
    if (!m_deploy.solutionId.empty() && peer->solutionId != m_deploy.solutionId) {
        if (reason) {
            *reason = "方案标识不一致";
        }
        return false;
    }
    if (m_node.role != NodeRoleServer) {
        if (reason) {
            *reason = "对端不是服务端";
        }
        return false;
    }
    for (std::map<std::string, Peer>::const_iterator it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (it->second.nodeId == peer->nodeId) {
            if (reason) {
                *reason = "这个 nodeId 已经在线";
            }
            return false;
        }
    }
    while (p < end) {
        std::string stationId;
        if (!takeStr(p, end, &stationId)) {
            break;
        }
        for (std::map<std::string, Peer>::const_iterator it = m_peers.begin(); it != m_peers.end(); ++it) {
            for (size_t i = 0; i < it->second.stations.size(); ++i) {
                if (it->second.stations[i] == stationId) {
                    if (reason) {
                        *reason = "工站被两个节点同时认领";
                    }
                    return false;
                }
            }
        }
        peer->stations.push_back(stationId);
    }
    peer->lastBeatMs = nowMs();
    return true;
}

void LinkService::onFrame(const CommFrame::Frame& frame)
{
    SharedFrames& frames = SharedFrames::instance();
    if (frame.type == CommFrame::TypeHello) {
        Peer peer;
        std::string reason;
        std::vector<unsigned char> ack;
        if (!acceptHello(frame.payload, &peer, &reason)) {
            appendStr(&ack, std::string("reject:") + reason);
            m_frames.writeFrame(m_channel, frame.peerId, CommFrame::TypeHelloAck, ack, 0);
            frames.setFault(IpcFaultRejected, reason);
            return;
        }
        m_peers[frame.peerId] = peer;
        appendStr(&ack, "ok");
        m_frames.writeFrame(m_channel, frame.peerId, CommFrame::TypeHelloAck, ack, 0);
        frames.setFault(IpcFaultNone, std::string());
        return;
    }
    std::map<std::string, Peer>::iterator it = m_peers.find(frame.peerId);
    if (it == m_peers.end() && m_server) {
        return;
    }
    if (frame.type == CommFrame::TypeHeartbeat) {
        if (it != m_peers.end()) {
            it->second.lastBeatMs = nowMs();
        }
        m_frames.writeFrame(m_channel, frame.peerId, CommFrame::TypeHeartbeatAck, frame.payload, 0);
        return;
    }
    if (frame.type == CommFrame::TypeHeartbeatAck) {
        if (it != m_peers.end()) {
            it->second.lastBeatMs = nowMs();
        }
        frames.setFault(IpcFaultNone, std::string());
        return;
    }
    if (frame.type == CommFrame::TypeHelloAck) {
        std::string text;
        const unsigned char* p = frame.payload.data();
        const unsigned char* end = p + frame.payload.size();
        takeStr(p, end, &text);
        if (text.compare(0, 7, "reject:") == 0) {
            frames.setFault(IpcFaultRejected, text.substr(7));
        } else {
            frames.setFault(IpcFaultNone, std::string());
        }
        return;
    }
    if (frame.type == CommFrame::TypeSubscribe && it != m_peers.end()) {
        it->second.subscribed = !frame.payload.empty() && frame.payload[0] != 0;
        return;
    }
    if (frame.type == CommFrame::TypeSnapshot && frames.subscribe()) {
        const unsigned char* p = frame.payload.data();
        const unsigned char* end = p + frame.payload.size();
        std::string stationId;
        std::string nodeId;
        if (!takeStr(p, end, &stationId) || !takeStr(p, end, &nodeId) || end - p < 21) {
            return;
        }
        uint32_t seq = 0;
        std::memcpy(&seq, p, 4);
        p += 4;
        const bool ok = *p++ != 0;
        int32_t cycle = 0;
        int32_t total = 0;
        int32_t pass = 0;
        int32_t fail = 0;
        std::memcpy(&cycle, p, 4);
        p += 4;
        std::memcpy(&total, p, 4);
        p += 4;
        std::memcpy(&pass, p, 4);
        p += 4;
        std::memcpy(&fail, p, 4);
        p += 4;
        std::string summary;
        takeStr(p, end, &summary);
        uint32_t jpegLen = 0;
        if (end - p < 4) {
            return;
        }
        std::memcpy(&jpegLen, p, 4);
        p += 4;
        if (jpegLen > static_cast<uint32_t>(end - p)) {
            return;
        }
        std::vector<unsigned char> jpeg(p, p + jpegLen);
        const int index = frames.bindRemote(stationId, nodeId);
        frames.publishRemote(index, seq, ok, cycle, total, pass, fail, false, summary, jpeg);
    }
}

void LinkService::publishLatest()
{
    if (!m_server && m_peers.empty()) {
        return;
    }
    SharedFrames& frames = SharedFrames::instance();
    const int n = frames.localCount();
    for (int i = 0; i < n; ++i) {
        IpcPreview preview;
        if (!frames.readLocal(i, &preview) || preview.jpeg.empty()) {
            continue;
        }
        std::vector<unsigned char> payload;
        appendStr(&payload, preview.stationId);
        appendStr(&payload, preview.nodeId);
        const uint32_t seq = preview.frameSeq;
        const int32_t cycle = preview.cycleMs;
        const int32_t total = preview.total;
        const int32_t pass = preview.pass;
        const int32_t fail = preview.fail;
        payload.insert(payload.end(), reinterpret_cast<const unsigned char*>(&seq),
                       reinterpret_cast<const unsigned char*>(&seq) + 4);
        payload.push_back(preview.ok ? 1 : 0);
        payload.insert(payload.end(), reinterpret_cast<const unsigned char*>(&cycle),
                       reinterpret_cast<const unsigned char*>(&cycle) + 4);
        payload.insert(payload.end(), reinterpret_cast<const unsigned char*>(&total),
                       reinterpret_cast<const unsigned char*>(&total) + 4);
        payload.insert(payload.end(), reinterpret_cast<const unsigned char*>(&pass),
                       reinterpret_cast<const unsigned char*>(&pass) + 4);
        payload.insert(payload.end(), reinterpret_cast<const unsigned char*>(&fail),
                       reinterpret_cast<const unsigned char*>(&fail) + 4);
        appendStr(&payload, preview.summary);
        const uint32_t jpegLen = static_cast<uint32_t>(preview.jpeg.size());
        payload.insert(payload.end(), reinterpret_cast<const unsigned char*>(&jpegLen),
                       reinterpret_cast<const unsigned char*>(&jpegLen) + 4);
        payload.insert(payload.end(), preview.jpeg.begin(), preview.jpeg.end());
        if (m_server) {
            for (std::map<std::string, Peer>::iterator it = m_peers.begin(); it != m_peers.end(); ++it) {
                if (!it->second.subscribed) {
                    continue;
                }
                bool own = false;
                for (size_t s = 0; s < it->second.stations.size(); ++s) {
                    if (it->second.stations[s] == preview.stationId) {
                        own = true;
                    }
                }
                if (!own) {
                    m_frames.writeFrame(m_channel, it->first, CommFrame::TypeSnapshot, payload, 0);
                }
            }
        } else {
            m_frames.writeFrame(m_channel, std::string(), CommFrame::TypeSnapshot, payload, 0);
        }
    }
}

void LinkService::ensureHello()
{
    if (!CommMgr::instance().isOpen(m_channel)) {
        m_helloSent = false;
        m_peers.clear();
        return;
    }
    if (m_helloSent) {
        return;
    }
    std::string err;
    if (!m_frames.writeFrame(m_channel, std::string(), CommFrame::TypeHello, makeHello(), &err)) {
        return;
    }
    m_peers[std::string()].lastBeatMs = nowMs();
    m_helloSent = true;
}

void LinkService::dropStale()
{
    const unsigned long long now = nowMs();
    SharedFrames& frames = SharedFrames::instance();
    for (std::map<std::string, Peer>::iterator it = m_peers.begin(); it != m_peers.end();) {
        if (it->second.lastBeatMs != 0 && now - it->second.lastBeatMs > 3000) {
            for (size_t i = 0; i < it->second.stations.size(); ++i) {
                const int index = frames.bindRemote(it->second.stations[i], it->second.nodeId);
                IpcPreview last;
                frames.readRemote(index, &last);
                frames.publishRemote(index, last.frameSeq, last.ok, last.cycleMs, last.total, last.pass, last.fail,
                                    true, last.summary, last.jpeg);
            }
            frames.setFault(IpcFaultLink, (it->second.nodeId.empty() ? std::string("链路") : it->second.nodeId) + " 超过 3 秒没有心跳");
            CommMgr::instance().closePeer(m_channel, it->first);
            m_frames.clearPeer(it->first);
            if (!m_server) {
                m_helloSent = false;
            }
            it = m_peers.erase(it);
        } else {
            ++it;
        }
    }
}

int LinkService::run()
{
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        std::wstring ws(path, n);
        const size_t slash = ws.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            AppPaths::setExeDir(wideToUtf8(ws.substr(0, slash).c_str()));
        }
    }
    AppPaths::ensureDirs();
    std::string err;
    if (!m_node.load(&err) || !m_node.linkEnabled || m_node.role == NodeRoleStandalone) {
        return 0;
    }
    const AppSettings settings = VisionCfgMgr::loadSettings();
    if (!settings.lastSolutionPath.empty()) {
        m_deploy.loadBeside(settings.lastSolutionPath, &err);
    }
    SharedFrames& frames = SharedFrames::instance();
    if (!frames.map(m_node.previewSlotBytes, &err)) {
        return 1;
    }
    frames.setRole(m_node.role);
    CommChannel channel;
    channel.id = m_channel;
    channel.name = "node-link";
    channel.enabled = true;
    m_server = m_node.role == NodeRoleServer;
    if (m_server) {
        channel.type = CommTcpServer;
        channel.params.insert("port", m_node.listenPort);
        channel.params.insert("maxClients", 32);
    } else {
        channel.type = CommTcpClient;
        channel.params.insert("host", m_node.serverHost);
        channel.params.insert("port", m_node.serverPort);
        channel.params.insert("autoReconnect", true);
    }
    if (!CommMgr::instance().openChannel(channel, &err)) {
        frames.setFault(IpcFaultLink, err);
        return 1;
    }
    if (!m_server) {
        m_helloSent = false;
    }
    unsigned long long nextBeat = nowMs();
    while (true) {
        frames.beatLink();
        if (!m_server) {
            ensureHello();
        }
        const unsigned long long now = nowMs();
        if (now >= nextBeat) {
            std::vector<unsigned char> beat(8, 0);
            const unsigned long long ms = now;
            std::memcpy(beat.data(), &ms, 8);
            if (m_server) {
                const std::vector<CommPeer> peers = CommMgr::instance().peers(m_channel);
                for (size_t i = 0; i < peers.size(); ++i) {
                    m_frames.writeFrame(m_channel, peers[i].id, CommFrame::TypeHeartbeat, beat, 0);
                }
            } else if (m_helloSent) {
                m_frames.writeFrame(m_channel, std::string(), CommFrame::TypeHeartbeat, beat, 0);
            }
            nextBeat = now + static_cast<unsigned long long>(m_node.heartbeatMs > 0 ? m_node.heartbeatMs : 1000);
        }
        if (m_server) {
            const std::vector<CommPeer> peers = CommMgr::instance().peers(m_channel);
            for (size_t i = 0; i < peers.size(); ++i) {
                CommFrame::Frame frame;
                if (m_frames.readFrame(m_channel, peers[i].id, 0, &frame, 0)) {
                    onFrame(frame);
                }
            }
        } else {
            CommFrame::Frame frame;
            if (m_frames.readFrame(m_channel, std::string(), 0, &frame, 0)) {
                onFrame(frame);
            }
        }
        if (frames.waitFrame(50)) {
            publishLatest();
        }
        if (frames.subscribe()) {
            std::vector<unsigned char> on(1, 1);
            if (!m_server) {
                m_frames.writeFrame(m_channel, std::string(), CommFrame::TypeSubscribe, on, 0);
            }
        }
        dropStale();
    }
}

}
