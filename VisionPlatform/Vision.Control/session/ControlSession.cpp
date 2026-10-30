/**
 * @file ControlSession.cpp
 * @brief 中控台对多台被控端的收发
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ControlSession.h"
#include "LinkDial.h"
#include "RemoteCrash.h"
#include "RemoteLog.h"

#include <windows.h>

namespace Vision {

ControlSession::ControlSession()
    : m_run(false)
    , m_listening(false)
    , m_adaptive(true)
{
    RemoteCrash::checkpoint("ControlSession.ctor");
    std::string err;
    if (!m_identity.ensure("control", "RemoteMaster", &err)) {
        RemoteCrash::log(("identity.ensure fail: " + err).c_str());
    } else {
        RemoteCrash::checkpoint("ControlSession.identity_ok");
    }
}

ControlSession::~ControlSession()
{
    stop();
}

uint64_t ControlSession::nowMs() const
{
    return GetTickCount64();
}

void ControlSession::setSelfName(const std::string& name)
{
    std::string cut = name;
    const size_t limit = static_cast<size_t>(RemoteConst::kTextMax);
    if (cut.size() > limit) {
        size_t end = limit;
        while (end > 0 && (static_cast<unsigned char>(cut[end]) & 0xC0) == 0x80) {
            --end;
        }
        cut.resize(end);
    }
    std::lock_guard<std::mutex> lock(m_mu);
    m_selfName = cut;
}

void ControlSession::setAdaptive(bool on)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_adaptive = on;
}

bool ControlSession::start(const std::string& host, int port, std::string* err)
{
    RemoteCrash::checkpoint("ControlSession.start");
    if (host.empty() || port <= 0 || port > 65535) {
        if (err) {
            *err = "监听地址或端口无效";
        }
        RemoteCrash::alert("ControlSession.start", err ? err->c_str() : "bad listen", true);
        return false;
    }
    bool ok = false;
    const bool ran = RemoteCrash::run("ControlSession.start", [&]() {
        stop();
        if (!m_identity.ready()) {
            m_identity.ensure("control", m_selfName.empty() ? "RemoteMaster" : m_selfName, err);
        }
        if (m_identity.ready()) {
            m_tcp.setTlsIdentity(&m_identity);
            RemoteCrash::checkpoint("ControlSession.tls_identity_set");
        } else {
            m_tcp.setTlsIdentity(nullptr);
            RemoteCrash::log("ControlSession.start: TLS identity not ready, plain TCP");
            RemoteCrash::alert("ControlSession.start", "身份还没准备好，先走明文 TCP 连接", true);
        }
        if (!m_tcp.open(host, port, err)) {
            RemoteCrash::log(err && !err->empty() ? ("listen fail: " + *err).c_str() : "listen fail");
            ok = false;
            return;
        }
        RemoteCrash::checkpoint("ControlSession.listen_ok");
        {
            std::lock_guard<std::mutex> lock(m_mu);
            m_run = true;
            m_listening = true;
            m_machines.clear();
        }
        m_read = std::thread(&ControlSession::readMain, this);
        m_write = std::thread(&ControlSession::writeMain, this);
        ok = true;
    });
    return ran && ok;
}

void ControlSession::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_run = false;
    }
    m_tcp.close();
    if (m_read.joinable()) {
        m_read.join();
    }
    if (m_write.joinable()) {
        m_write.join();
    }
    std::lock_guard<std::mutex> lock(m_mu);
    m_listening = false;
    for (size_t i = 0; i < m_machines.size(); ++i) {
        m_machines[i].tile.online = false;
        m_machines[i].tile.operating = false;
    }
}

bool ControlSession::listening() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_listening;
}

std::vector<ControlSession::Tile> ControlSession::tiles() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    std::vector<Tile> out;
    for (size_t i = 0; i < m_machines.size(); ++i) {
        out.push_back(m_machines[i].tile);
    }
    return out;
}

void ControlSession::enqueue(Machine* machine, const CommBytes& bytes)
{
    if (bytes.empty() || !machine) {
        return;
    }
    if (machine->out.size() >= 32) {
        machine->out.erase(machine->out.begin());
    }
    machine->out.push_back(bytes);
}

int ControlSession::indexOfPeer(const std::string& peerId)
{
    for (size_t i = 0; i < m_machines.size(); ++i) {
        if (m_machines[i].tile.peerId == peerId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string ControlSession::hostOf(const std::string& remote)
{
    const size_t colon = remote.rfind(':');
    if (colon == std::string::npos || colon == 0) {
        return remote;
    }
    return remote.substr(0, colon);
}

void ControlSession::adoptHello(const std::string& peerId, const HelloMsg& hello)
{
    const int self = indexOfPeer(peerId);
    if (self < 0) {
        return;
    }
    Machine& mine = m_machines[static_cast<size_t>(self)];
    // 拼绑定键。没有设备 ID 时与 DeviceStore::keyFor 的地址分支一致
    const std::string key = hello.share == 1
        ? std::string("d:") + mine.tile.address
        : std::string("w:") + mine.tile.address + "\n" + hello.source;
    // 同一绑定键的旧连接：重连时先断开，再删掉旧条目
    std::vector<std::string> drop;
    for (size_t i = 0; i < m_machines.size(); ++i) {
        if (static_cast<int>(i) == self) {
            continue;
        }
        if (m_machines[i].tile.bindKey == key && !key.empty()) {
            if (!m_machines[i].tile.peerId.empty()) {
                drop.push_back(m_machines[i].tile.peerId);
            }
        }
    }
    for (size_t i = 0; i < drop.size(); ++i) {
        m_tcp.closePeer(drop[i]);
    }
    for (size_t i = m_machines.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (m_machines[index].tile.peerId == peerId) {
            continue;
        }
        if (m_machines[index].tile.bindKey == key) {
            m_machines.erase(m_machines.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
    // 删除被挤掉的元素后重新定位本条，再写入设备信息
    const int again = indexOfPeer(peerId);
    if (again < 0) {
        return;
    }
    Machine& current = m_machines[static_cast<size_t>(again)];
    current.tile.bindKey = key;
    current.tile.name = hello.name;
    current.tile.source = hello.source;
    current.tile.share = hello.share;
    current.tile.monitor = hello.monitor;
    current.tile.title = hello.title;
    current.tile.process = hello.process;
    current.tile.deviceId = hello.deviceId;
    current.tile.monitors = hello.monitors;
    current.tile.system = hello.system;
    current.tile.forceRecord = hello.forceRecord;
    current.tile.operating = false;
    std::string err;
    HelloAckMsg ack;
    ack.code = 0;
    ack.name = m_selfName;
    ack.fingerprint = m_identity.fingerprint();
    enqueue(&current, Messages::packHelloAck(ack, &err));
}

void ControlSession::requestOperate(const std::string& peerId, uint8_t method, const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0 || !m_machines[static_cast<size_t>(index)].tile.online) {
        return;
    }
    Machine& machine = m_machines[static_cast<size_t>(index)];
    machine.auth.method = method;
    machine.auth.token = token;
    machine.auth.perms = PermDefaultRequest;
    machine.wantAuth = true;
    machine.tile.hint = "正在申请操作…";
}

void ControlSession::pushConfig(const std::string& peerId, const ConfigMsg& cfg)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0 || !m_machines[static_cast<size_t>(index)].tile.online) {
        return;
    }
    Machine& machine = m_machines[static_cast<size_t>(index)];
    machine.config = cfg;
    machine.wantConfig = true;
}

void ControlSession::subscribe(const std::string& peerId, const SubscribeMsg& msg)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0 || !m_machines[static_cast<size_t>(index)].tile.online) {
        return;
    }
    Machine& machine = m_machines[static_cast<size_t>(index)];
    std::string err;
    enqueue(&machine, Messages::packSubscribe(msg, &err));
}

void ControlSession::releaseOperate(const std::string& peerId)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0) {
        return;
    }
    Machine& machine = m_machines[static_cast<size_t>(index)];
    machine.wantRelease = true;
    machine.tile.operating = false;
}

void ControlSession::sendInput(const std::string& peerId, const InputMsg& msg)
{
    std::string err;
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0 || !m_machines[static_cast<size_t>(index)].tile.operating) {
        return;
    }
    enqueue(&m_machines[static_cast<size_t>(index)], Messages::packInput(msg, &err));
}

void ControlSession::sendSpecial(const std::string& peerId, uint8_t kind)
{
    std::string err;
    SpecialMsg msg;
    msg.kind = kind;
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0 || !m_machines[static_cast<size_t>(index)].tile.operating) {
        return;
    }
    enqueue(&m_machines[static_cast<size_t>(index)], Messages::packSpecial(msg, &err));
}

std::string ControlSession::fingerprint() const
{
    return m_identity.fingerprint();
}

std::string ControlSession::peerFingerprint(const std::string& peerId) const
{
    return m_tcp.peerFingerprint(peerId);
}

void ControlSession::readMain()
{
    try {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (!m_run) {
                return;
            }
        }
        // 按当前连接表补新条目；已断开的标离线，还没有绑定键的直接删掉
        const std::vector<CommPeer> peers = m_tcp.peers();
        {
            std::lock_guard<std::mutex> lock(m_mu);
            for (size_t i = 0; i < peers.size(); ++i) {
                if (indexOfPeer(peers[i].id) < 0) {
                    Machine machine;
                    machine.tile.peerId = peers[i].id;
                    machine.tile.address = hostOf(peers[i].remote);
                    machine.tile.name = peers[i].remote;
                    machine.tile.online = true;
                    machine.tile.onlineSince = nowMs();
                    machine.lastRx = nowMs();
                    m_machines.push_back(machine);
                }
            }
            for (size_t i = m_machines.size(); i > 0; --i) {
                Machine& machine = m_machines[i - 1];
                if (machine.tile.peerId.empty()) {
                    continue;
                }
                bool live = false;
                for (size_t n = 0; n < peers.size(); ++n) {
                    if (peers[n].id == machine.tile.peerId) {
                        live = true;
                        break;
                    }
                }
                if (live) {
                    continue;
                }
                machine.tile.online = false;
                machine.tile.operating = false;
                machine.tile.peerId.clear();
                machine.frame.clear();
                if (machine.tile.bindKey.empty()) {
                    m_machines.erase(m_machines.begin() + static_cast<std::ptrdiff_t>(i - 1));
                }
            }
        }
        // 每条连接读一块。出错、超时无数据或拆帧失败就断开，条目留着
        for (size_t p = 0; p < peers.size(); ++p) {
            const std::string peer = peers[p].id;
            std::string err;
            const CommBytes chunk = m_tcp.read(peer, 256 * 1024, 20, &err);
            std::lock_guard<std::mutex> lock(m_mu);
            const int index = indexOfPeer(peer);
            if (index < 0) {
                continue;
            }
            Machine& machine = m_machines[static_cast<size_t>(index)];
            if (!err.empty()) {
                m_tcp.closePeer(peer);
                machine.tile.online = false;
                machine.tile.operating = false;
                machine.tile.peerId.clear();
                machine.frame.clear();
                continue;
            }
            if (chunk.empty()) {
                if (machine.tile.online && machine.lastRx != 0
                    && nowMs() - machine.lastRx > static_cast<uint64_t>(RemoteConst::kOfflineMs)) {
                    m_tcp.closePeer(peer);
                    machine.tile.online = false;
                    machine.tile.operating = false;
                    machine.tile.peerId.clear();
                    machine.frame.clear();
                }
                continue;
            }
            std::vector<Frame::Packet> packets;
            if (!machine.frame.push(chunk, &packets, &err)) {
                m_tcp.closePeer(peer);
                machine.tile.online = false;
                machine.tile.operating = false;
                machine.tile.peerId.clear();
                continue;
            }
            // 逐包处理。Hello 可能删掉别的条目，所以每包重新定位
            for (size_t i = 0; i < packets.size(); ++i) {
                const int slot = indexOfPeer(peer);
                if (slot < 0) {
                    break;
                }
                Machine& machine = m_machines[static_cast<size_t>(slot)];
                const Frame::Packet& packet = packets[i];
                machine.lastRx = nowMs();
                machine.tile.online = true;
                if (takePhase4(&machine, packet)) {
                    continue;
                }
                if (packet.channel == Frame::ChCursor) {
                    if (packet.type == Frame::TypeCursorShape) {
                        CursorShapeMsg shape;
                        if (Messages::unpackCursorShape(packet.payload, &shape)) {
                            machine.tile.cursorShape.swap(shape.png);
                            machine.tile.cursorHotX = shape.hotX;
                            machine.tile.cursorHotY = shape.hotY;
                            machine.tile.cursorShapeW = shape.width;
                            machine.tile.cursorShapeH = shape.height;
                            machine.tile.cursorShapeSeq += 1;
                        }
                    } else {
                        CursorPosMsg cursor;
                        if (Messages::unpackCursorPos(packet.payload, &cursor)) {
                            machine.tile.cursorX = cursor.x;
                            machine.tile.cursorY = cursor.y;
                            machine.tile.cursorOn = cursor.visible;
                        }
                    }
                    continue;
                }
                if ((packet.channel == Frame::ChVideo || packet.channel == Frame::ChThumb)
                    && packet.type == Frame::TypeVideoFrame) {
                    const uint64_t t0 = RemoteLog::tickMs();
                    machine.tile.bytesRx += packet.payload.size();
                    PictureMsg picture;
                    if (!Messages::unpackPicture(packet.payload, &picture)) {
                        continue;
                    }
                    // 原图 BGRA 经分片重组后可到十余 MB；JPEG/H.264 仍按更紧的上限过滤
                    const uint8_t picCodec = picture.codec == 0 ? 1 : picture.codec;
                    const size_t hardCap = picCodec == 3 ? 12u * 1024u * 1024u
                        : (picCodec == 2 ? 2u * 1024u * 1024u : Frame::kMaxPayload);
                    if (picture.imageW > 8192 || picture.imageH > 8192 || picture.jpeg.size() > hardCap) {
                        continue;
                    }
                    machine.tile.operable = picture.operable != 0;
                    if (packet.channel == Frame::ChVideo) {
                        machine.tile.imageW = picture.imageW;
                        machine.tile.imageH = picture.imageH;
                        machine.tile.codec = picCodec;
                    } else if (machine.tile.jpeg.empty()) {
                        machine.tile.imageW = picture.imageW;
                        machine.tile.imageH = picture.imageH;
                    }
                    CommBytes* slot = packet.channel == Frame::ChThumb ? &machine.tile.thumb : &machine.tile.jpeg;
                    const size_t cap = picCodec == 3 ? 12u * 1024u * 1024u
                        : (picCodec == 2 ? 2u * 1024u * 1024u : 512u * 1024u);
                    if (picture.jpeg.size() <= cap) {
                        slot->swap(picture.jpeg);
                    } else {
                        slot->clear();
                    }
                    if (packet.channel == Frame::ChThumb) {
                        machine.tile.thumbSeq += 1;
                    } else {
                        machine.tile.pictureSeq += 1;
                        const uint64_t unpackMs = RemoteLog::tickMs() - t0;
                        RemoteLog::writef(RemoteLog::Info, "接收",
                            "主画面 peer=%s seq=%llu 解包%llums %zuB codec=%u %ux%u",
                            machine.tile.peerId.c_str(),
                            static_cast<unsigned long long>(machine.tile.pictureSeq),
                            static_cast<unsigned long long>(unpackMs),
                            slot->size(),
                            static_cast<unsigned>(machine.tile.codec),
                            machine.tile.imageW,
                            machine.tile.imageH);
                    }
                } else if (packet.channel != Frame::ChControl) {
                    continue;
                } else if (packet.type == Frame::TypeHello) {
                    HelloMsg hello;
                    if (!Messages::unpackHello(packet.payload, &hello)) {
                        continue;
                    }
                    adoptHello(peer, hello);
                } else if (packet.type == Frame::TypeHeartbeat) {
                    HeartbeatMsg beat;
                    if (!Messages::unpackHeartbeat(packet.payload, &beat)) {
                        continue;
                    }
                    HeartbeatAckMsg ack;
                    ack.peerMs = beat.ms;
                    ack.selfMs = nowMs();
                    enqueue(&machine, Messages::packHeartbeatAck(ack, &err));
                } else if (packet.type == Frame::TypeHeartbeatAck) {
                    HeartbeatAckMsg ack;
                    if (Messages::unpackHeartbeatAck(packet.payload, &ack) && ack.peerMs != 0 && nowMs() >= ack.peerMs) {
                        machine.tile.rttMs = static_cast<int>(nowMs() - ack.peerMs);
                    }
                } else if (packet.type == Frame::TypeAuthResult) {
                    AuthResultMsg ack;
                    const bool ok = Messages::unpackAuthResult(packet.payload, &ack);
                    if (ok && ack.code == 0) {
                        machine.tile.operating = (ack.perms & PermOperate) != 0;
                        machine.tile.hint.clear();
                    } else {
                        machine.tile.operating = false;
                        if (ok && ack.code == 3) {
                            machine.tile.hint = "对方已拒绝本次操作";
                        } else if (ok && ack.code == 4) {
                            machine.tile.hint = "确认超时，已按拒绝处理";
                        } else if (ok && ack.code == 5) {
                            machine.tile.hint = "认证失败";
                        } else if (ok && ack.code == 6) {
                            machine.tile.hint = "连续认证失败，已锁定";
                        } else {
                            machine.tile.hint = "当前无法操作";
                        }
                    }
                } else if (packet.type == Frame::TypeOperate) {
                    OperateMsg op;
                    if (Messages::unpackOperate(packet.payload, &op) && op.action == Frame::OperateRelease) {
                        if (machine.tile.operating) {
                            machine.tile.hint = "对方已释放操作";
                        }
                        machine.tile.operating = false;
                    }
                } else if (packet.type == Frame::TypeBye) {
                    machine.tile.online = false;
                    machine.tile.operating = false;
                }
            }
        }
        Sleep(10);
    }
    } catch (const std::exception& e) {
        RemoteCrash::alert("ControlSession.readMain", e.what(), true);
    } catch (...) {
        RemoteCrash::alert("ControlSession.readMain", "收包线程发生未捕获异常", true);
    }
}

void ControlSession::writeMain()
{
    try {
    std::vector<uint64_t> nextBeat;
    while (true) {
        std::vector<std::string> peers;
        std::vector<std::vector<CommBytes> > batches;
        std::vector<bool> auths;
        std::vector<AuthRequestMsg> authMsgs;
        std::vector<bool> releases;
        std::vector<bool> configs;
        std::vector<ConfigMsg> configMsgs;
        std::vector<bool> subs;
        std::vector<SubscribeMsg> subMsgs;
        std::vector<int> rtts;
        bool adaptive = true;
        bool run = false;
        // 锁里取走待发标志和队列，锁外再写套接字
        {
            std::lock_guard<std::mutex> lock(m_mu);
            run = m_run;
            if (nextBeat.size() != m_machines.size()) {
                nextBeat.assign(m_machines.size(), 0);
            }
            for (size_t i = 0; i < m_machines.size(); ++i) {
                peers.push_back(m_machines[i].tile.peerId);
                auths.push_back(m_machines[i].wantAuth);
                authMsgs.push_back(m_machines[i].auth);
                releases.push_back(m_machines[i].wantRelease);
                configs.push_back(m_machines[i].wantConfig);
                configMsgs.push_back(m_machines[i].config);
                subs.push_back(m_machines[i].wantSub);
                subMsgs.push_back(m_machines[i].subscribe);
                rtts.push_back(m_machines[i].tile.rttMs);
                adaptive = m_adaptive;
                m_machines[i].wantAuth = false;
                m_machines[i].wantRelease = false;
                m_machines[i].wantConfig = false;
                m_machines[i].wantSub = false;
                batches.push_back(m_machines[i].out);
                m_machines[i].out.clear();
            }
        }
        if (!run) {
            return;
        }
        std::string err;
        const uint64_t now = nowMs();
        // 按条目发心跳、操作申请、释放、配置和订阅，再发队列里的包
        for (size_t i = 0; i < peers.size(); ++i) {
            if (peers[i].empty()) {
                continue;
            }
            if (now >= nextBeat[i]) {
                HeartbeatMsg beat;
                beat.ms = now;
                m_tcp.write(peers[i], Messages::packHeartbeat(beat, &err), 1000, &err);
                if (i < rtts.size() && rtts[i] >= 0) {
                    NetStatsMsg stats;
                    stats.rttMs = static_cast<uint16_t>(rtts[i] > 65535 ? 65535 : rtts[i]);
                    stats.congested = (adaptive && rtts[i] >= 800) ? 1 : 0;
                    m_tcp.write(peers[i], Messages::packNetStats(stats, &err), 1000, &err);
                }
                nextBeat[i] = now + RemoteConst::kHeartbeatMs;
            }
            if (auths[i]) {
                AuthRequestMsg req = authMsgs[i];
                if (req.method == 0) {
                    req.method = AuthMethodTrusted;
                    req.perms = PermDefaultRequest;
                }
                m_tcp.write(peers[i], Messages::packAuthRequest(req, &err), 1000, &err);
            }
            if (releases[i]) {
                OperateMsg op;
                op.action = Frame::OperateRelease;
                m_tcp.write(peers[i], Messages::packOperate(op, &err), 1000, &err);
            }
            if (configs[i]) {
                m_tcp.write(peers[i], Messages::packConfig(configMsgs[i], &err), 1000, &err);
            }
            if (subs[i]) {
                m_tcp.write(peers[i], Messages::packSubscribe(subMsgs[i], &err), 1000, &err);
            }
            pumpUpload(peers[i]);
            for (size_t n = 0; n < batches[i].size(); ++n) {
                m_tcp.write(peers[i], batches[i][n], 1000, &err);
            }
        }
        Sleep(20);
    }
    } catch (const std::exception& e) {
        RemoteCrash::alert("ControlSession.writeMain", e.what(), true);
    } catch (...) {
        RemoteCrash::alert("ControlSession.writeMain", "发包线程发生未捕获异常", true);
    }
}

bool ControlSession::dialDevice(const std::string& address, const std::string& deviceId,
    const std::string& relayHost, int relayPort, const std::string& token, std::string* err)
{
    LinkDial dial;
    return dial.reach(&m_tcp, address, 19102, relayHost, relayPort, token, deviceId, err);
}

}
