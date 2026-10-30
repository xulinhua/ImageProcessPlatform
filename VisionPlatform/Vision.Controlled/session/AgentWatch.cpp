/**
 * @file AgentWatch.cpp
 * @brief 被控端再连一台中控台，只看，操作权靠交接
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentSession.h"
#include "WindowCapture.h"

#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace Vision {

class WatchLink {
public:
    WatchLink(AgentSession* session, const std::string& host, int port)
        : m_session(session)
        , m_host(host)
        , m_port(port)
        , m_stop(false)
        , m_hadOperate(false)
        , m_sent(0)
    {
        m_thread = std::thread(&WatchLink::run, this);
    }

    ~WatchLink()
    {
        stop();
    }

    void stop()
    {
        m_stop = true;
        m_tcp.close();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    bool same(const std::string& host, int port) const
    {
        return m_host == host && m_port == port;
    }

    std::string line() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_name + "\t" + m_fp;
    }

private:
    void run()
    {
        std::string err;
        while (!m_stop) {
            if (!m_tcp.isOpen()) {
                if (!m_tcp.open(m_host, m_port, 3000, &err)) {
                    Sleep(1000);
                    continue;
                }
                std::string fp;
                if (!m_session->openViewerTls(&m_tcp, &fp, &err)) {
                    m_tcp.close();
                    Sleep(1000);
                    continue;
                }
                HelloMsg hello;
                if (m_session->fillHello(&hello)) {
                    m_tcp.write("", Messages::packHello(hello, &err), 1000, &err);
                }
                m_frame.clear();
            }
            const CommBytes chunk = m_tcp.read("", 256 * 1024, 30, &err);
            if (!err.empty()) {
                m_tcp.close();
                continue;
            }
            if (!chunk.empty()) {
                std::vector<Frame::Packet> packets;
                if (!m_frame.push(chunk, &packets, &err)) {
                    m_tcp.close();
                    continue;
                }
                for (size_t i = 0; i < packets.size(); ++i) {
                    take(packets[i]);
                }
            }
            PictureMsg picture = m_session->pictureCopy();
            const uint64_t seq = m_session->pictureSeqCopy();
            const bool operate = m_session->viewerMayOperate(fingerprint());
            if (m_hadOperate && !operate) {
                OperateMsg release;
                release.action = Frame::OperateRelease;
                m_tcp.write("", Messages::packOperate(release, &err), 1000, &err);
                std::lock_guard<std::mutex> lock(m_mu);
                m_hadOperate = false;
            }
            if (seq != m_sent && !picture.jpeg.empty() && picture.imageW > 0) {
                m_tcp.write("", Messages::packPicture(picture, Frame::ChVideo, &err), 1000, &err);
                m_sent = seq;
            }
            CursorPosMsg cursor = m_session->cursorCopy();
            m_tcp.write("", Messages::packCursorPos(cursor, &err), 1000, &err);
        }
    }

    void take(const Frame::Packet& packet)
    {
        std::string err;
        if (packet.type == Frame::TypeHelloAck && packet.channel == Frame::ChControl) {
            HelloAckMsg ack;
            if (Messages::unpackHelloAck(packet.payload, &ack)) {
                std::lock_guard<std::mutex> lock(m_mu);
                m_name = ack.name;
                if (!ack.fingerprint.empty()) {
                    m_fp = ack.fingerprint;
                }
            }
            return;
        }
        if (packet.type == Frame::TypeHeartbeat && packet.channel == Frame::ChControl) {
            HeartbeatMsg beat;
            if (Messages::unpackHeartbeat(packet.payload, &beat)) {
                HeartbeatAckMsg ack;
                ack.peerMs = beat.ms;
                ack.selfMs = GetTickCount64();
                m_tcp.write("", Messages::packHeartbeatAck(ack, &err), 1000, &err);
            }
            return;
        }
        if (packet.type == Frame::TypeAuthRequest && packet.channel == Frame::ChControl) {
            AuthResultMsg ack;
            ack.code = 0;
            ack.perms = PermView;
            if (m_session->viewerMayOperate(fingerprint())) {
                ack.perms = m_session->allowMask();
                std::lock_guard<std::mutex> lock(m_mu);
                m_hadOperate = true;
            }
            m_tcp.write("", Messages::packAuthResult(ack, &err), 1000, &err);
            return;
        }
        if (packet.channel == Frame::ChInput && (packet.type == Frame::TypeMouse || packet.type == Frame::TypeKey)) {
            InputMsg input;
            if (Messages::unpackInput(packet.payload, &input)) {
                m_session->submitViewerInput(fingerprint(), input);
            }
        }
    }

    std::string fingerprint() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_fp;
    }

    AgentSession* m_session;
    std::string m_host;
    int m_port;
    RemoteTcpClient m_tcp;
    Frame m_frame;
    std::thread m_thread;
    std::atomic<bool> m_stop;
    bool m_hadOperate;
    uint64_t m_sent;
    mutable std::mutex m_mu;
    std::string m_name;
    std::string m_fp;
};

std::string AgentSession::sha256File(const std::wstring& path) const
{
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (!file) {
        return std::string();
    }
    BCRYPT_ALG_HANDLE alg = 0;
    BCRYPT_HASH_HANDLE hash = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, 0, 0) != 0
        || BCryptCreateHash(alg, &hash, 0, 0, 0, 0, 0) != 0) {
        if (alg) {
            BCryptCloseAlgorithmProvider(alg, 0);
        }
        fclose(file);
        return std::string();
    }
    unsigned char buf[65536];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), file)) > 0) {
        BCryptHashData(hash, buf, static_cast<ULONG>(n), 0);
    }
    fclose(file);
    unsigned char dig[32];
    BCryptFinishHash(hash, dig, sizeof(dig), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    static const char* hex = "0123456789abcdef";
    std::string out(64, '0');
    for (int i = 0; i < 32; ++i) {
        out[static_cast<size_t>(i) * 2] = hex[dig[i] >> 4];
        out[static_cast<size_t>(i) * 2 + 1] = hex[dig[i] & 0x0f];
    }
    return out;
}

void AgentSession::stopWatches()
{
    std::vector<WatchLink*> links;
    {
        std::lock_guard<std::mutex> lock(m_watchMu);
        links.swap(m_watches);
    }
    for (size_t i = 0; i < links.size(); ++i) {
        links[i]->stop();
        delete links[i];
    }
}

void AgentSession::inviteViewer(const std::string& host, int port)
{
    if (host.empty() || port <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_watchMu);
    if (m_watches.size() >= 3) {
        return;
    }
    for (size_t i = 0; i < m_watches.size(); ++i) {
        if (m_watches[i]->same(host, port)) {
            return;
        }
    }
    m_watches.push_back(new WatchLink(this, host, port));
}

void AgentSession::handoffTo(const std::string& fingerprint)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_grantFp = fingerprint;
    m_operating = false;
    m_releaseInput = true;
    m_status = "已交接";
}

bool AgentSession::viewerMayOperate(const std::string& fingerprint) const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return !fingerprint.empty() && fingerprint == m_grantFp;
}

std::string AgentSession::viewerReport() const
{
    std::lock_guard<std::mutex> lock(m_watchMu);
    std::string out;
    for (size_t i = 0; i < m_watches.size(); ++i) {
        const std::string row = m_watches[i]->line();
        const size_t tab = row.find('\t');
        const std::string fp = tab == std::string::npos ? std::string() : row.substr(tab + 1);
        out += row;
        out += viewerMayOperate(fp) ? "\t1\n" : "\t0\n";
    }
    return out;
}

void AgentSession::submitViewerInput(const std::string& fingerprint, const InputMsg& input)
{
    std::lock_guard<std::mutex> lock(m_watchMu);
    if (m_viewerInput.size() < 64) {
        m_viewerInput.push_back(std::make_pair(fingerprint, input));
    }
}

void AgentSession::drainViewerInputs()
{
    std::vector<std::pair<std::string, InputMsg> > batch;
    {
        std::lock_guard<std::mutex> lock(m_watchMu);
        batch.swap(m_viewerInput);
    }
    for (size_t i = 0; i < batch.size(); ++i) {
        applyViewerInput(batch[i].first, batch[i].second);
    }
}

void AgentSession::applyViewerInput(const std::string& fingerprint, const InputMsg& input)
{
    if (!viewerMayOperate(fingerprint)) {
        return;
    }
    const PictureMsg picture = pictureCopy();
    HWND hwnd = 0;
    bool desktop = false;
    int originX = 0;
    int originY = 0;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        hwnd = m_watch.current();
        desktop = m_desktop;
        originX = m_originX;
        originY = m_originY;
    }
    if (!picture.operable) {
        return;
    }
    if (desktop) {
        m_input.postScreen(originX, originY, input, picture.clientW, picture.clientH, picture.imageW, picture.imageH);
    } else {
        m_input.post(hwnd, input, picture.clientW, picture.clientH, picture.imageW, picture.imageH);
    }
}

PictureMsg AgentSession::pictureCopy() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_picture;
}

uint64_t AgentSession::pictureSeqCopy() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_pictureSeq;
}

CursorPosMsg AgentSession::cursorCopy() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_cursor;
}

bool AgentSession::fillHello(HelloMsg* hello) const
{
    if (!hello) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    hello->name = machineName();
    hello->source = m_source;
    hello->share = m_desktop ? 1 : 0;
    hello->visible = m_picture.visible;
    hello->fingerprint = m_identity.fingerprint();
    hello->monitor = static_cast<uint8_t>(m_monitor < 0 ? 0 : m_monitor);
    hello->title = toUtf8(m_title);
    hello->process = toUtf8(m_process);
    hello->deviceId = m_deviceId;
    hello->monitors = static_cast<uint8_t>(WindowCapture::monitorCount());
    hello->system = m_system;
    hello->forceRecord = m_forceRecord ? 1 : 0;
    return true;
}

bool AgentSession::openViewerTls(RemoteTcpClient* tcp, std::string* peerFp, std::string* err)
{
    if (!tcp || !m_identity.ready()) {
        return false;
    }
    return tcp->startTls(m_identity, peerFp, err);
}

}
