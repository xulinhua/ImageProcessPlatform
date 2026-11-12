/**
 * @file AgentApp.cpp
 * @brief 会话：Hello、心跳、订阅后的 JPEG、键鼠
 */

#include "AgentApp.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace Vision {

namespace {

uint64_t nowMs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000ull + static_cast<uint64_t>(ts.tv_nsec / 1000000);
}

std::string hostName()
{
    char name[64];
    if (gethostname(name, sizeof(name)) != 0) {
        return "ubuntu";
    }
    name[sizeof(name) - 1] = 0;
    return name;
}

}

std::string AgentApp::exeOf(int pid) const
{
    char link[64];
    std::snprintf(link, sizeof(link), "/proc/%d/exe", pid);
    char path[512];
    const ssize_t n = ::readlink(link, path, sizeof(path) - 1);
    if (n <= 0) {
        return std::string();
    }
    path[n] = 0;
    return path;
}

void AgentApp::stopPrevious()
{
    const std::string self = exeOf(static_cast<int>(::getpid()));
    if (self.empty()) {
        return;
    }
    DIR* dir = ::opendir("/proc");
    if (!dir) {
        return;
    }
    std::vector<int> pids;
    while (dirent* entry = ::readdir(dir)) {
        bool digits = entry->d_name[0] != 0;
        for (const char* p = entry->d_name; *p; ++p) {
            if (*p < '0' || *p > '9') {
                digits = false;
                break;
            }
        }
        if (!digits) {
            continue;
        }
        const int pid = std::atoi(entry->d_name);
        if (pid <= 0 || pid == static_cast<int>(::getpid())) {
            continue;
        }
        if (exeOf(pid) == self || exeOf(pid) == self + " (deleted)") {
            pids.push_back(pid);
        }
    }
    ::closedir(dir);
    if (pids.empty()) {
        return;
    }
    for (size_t i = 0; i < pids.size(); ++i) {
        std::fprintf(stderr, "结束已有被控端 %d\n", pids[i]);
        ::kill(pids[i], SIGTERM);
    }
    for (int spin = 0; spin < 20; ++spin) {
        bool alive = false;
        for (size_t i = 0; i < pids.size(); ++i) {
            if (::kill(pids[i], 0) == 0) {
                alive = true;
            }
        }
        if (!alive) {
            return;
        }
        ::usleep(100000);
    }
    for (size_t i = 0; i < pids.size(); ++i) {
        if (::kill(pids[i], 0) == 0) {
            ::kill(pids[i], SIGKILL);
        }
    }
    ::usleep(100000);
}

int AgentApp::run(int argc, char** argv)
{
    if (argc >= 2) {
        m_consoleHost = argv[1];
    }
    m_consolePort = 19100;
    if (argc >= 3) {
        m_consolePort = std::atoi(argv[2]);
    }
    m_listenFd = -1;
    m_mainOn = false;
    m_thumbOn = false;
    m_operating = false;
    m_mainEdge = RemoteConst::kLongEdge;
    m_thumbEdge = 480;
    m_quality = RemoteConst::kJpegQuality;
    m_fps = 15;
    m_imageW = 0;
    m_imageH = 0;
    m_nextFrame = 0;
    m_nextBeat = 0;
    m_nextDial = 0;
    m_nextPreview = 0;
    m_nextWindows = 0;
    m_perms = 0;
    m_up.store(false);
    m_sharePending = false;
    m_shareDesktop = true;
    m_codeAt = 0;

    stopPrevious();

    std::string err;
    if (!m_id.loadOrCreate(&err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    if (!m_desk.open(&err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    m_deskLine = m_desk.summary();
    m_beacon.setInfo(helloMsg());
    if (!m_beacon.listen(&err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
    }
    if (!listenOn(19102, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    std::fprintf(stderr, "设备 %s\n指纹 %s\n直连 0.0.0.0:19102\n发现 UDP 19101\n", m_id.deviceId().c_str(), m_id.fingerprint().c_str());
    note("等待中控台");
    touchCode();
    if (!m_consoleHost.empty()) {
        std::fprintf(stderr, "反连 %s:%d\n", m_consoleHost.c_str(), m_consolePort);
    }

    for (;;) {
        // 先等直连。没有中控台地址时多等一会，有地址就尽快去反连
        const bool known = !m_consoleHost.empty() || !m_beacon.consoleHost().empty();
        const int accepted = acceptOne(known ? 200 : 1000);
        if (accepted >= 0) {
            std::fprintf(stderr, "直连进来\n");
            serve(accepted);
            continue;
        }
        std::string host = m_consoleHost;
        int port = m_consolePort;
        if (host.empty()) {
            host = m_beacon.consoleHost();
            port = RemoteConst::kPort;
        }
        if (host.empty() || nowMs() < m_nextDial) {
            touchCode();
            takeShare();
            refreshWindows();
            continue;
        }
        touchCode();
        takeShare();
        refreshWindows();
        const int fd = dial(host, port);
        if (fd < 0) {
            std::fprintf(stderr, "反连 %s:%d 失败\n", host.c_str(), port);
            m_nextDial = nowMs() + 2000;
            continue;
        }
        std::fprintf(stderr, "反连 %s:%d\n", host.c_str(), port);
        serve(fd);
    }
}

bool AgentApp::listenOn(int port, std::string* err)
{
    m_listenFd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenFd < 0) {
        if (err) {
            *err = "创建监听套接字失败";
        }
        return false;
    }
    int reuse = 1;
    setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(m_listenFd, 4) != 0) {
        if (err) {
            *err = "19102 被占用";
        }
        return false;
    }
    return true;
}

int AgentApp::acceptOne(int timeoutMs)
{
    pollfd pfd;
    pfd.fd = m_listenFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, timeoutMs) <= 0) {
        return -1;
    }
    sockaddr_in from;
    socklen_t len = sizeof(from);
    return ::accept(m_listenFd, reinterpret_cast<sockaddr*>(&from), &len);
}

int AgentApp::dial(const std::string& host, int port)
{
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = 0;
    char portText[16];
    std::snprintf(portText, sizeof(portText), "%d", port);
    if (getaddrinfo(host.c_str(), portText, &hints, &result) != 0) {
        return -1;
    }
    int fd = -1;
    for (addrinfo* it = result; it; it = it->ai_next) {
        fd = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) {
            continue;
        }
        timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            break;
        }
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(result);
    return fd;
}

bool AgentApp::sendPacked(const CommBytes& bytes)
{
    if (bytes.empty()) {
        return false;
    }
    std::string err;
    if (!m_link.write(bytes, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return false;
    }
    return true;
}

void AgentApp::setStatusHandler(const std::function<void(const std::string&, const std::string&, const std::string&, const std::string&)>& handler)
{
    m_status = handler;
}

void AgentApp::setPreviewHandler(const std::function<void(const std::string&, int, int)>& handler)
{
    m_preview = handler;
}

HelloMsg AgentApp::helloMsg() const
{
    HelloMsg hello;
    hello.name = hostName();
    if (hello.name.size() > 64) {
        hello.name.resize(64);
    }
    hello.source = m_desk.source();
    hello.share = m_desk.desktop() ? 1 : 0;
    hello.visible = 1;
    hello.monitor = 0;
    hello.title = m_desk.title();
    hello.process = m_desk.process();
    hello.deviceId = m_id.deviceId();
    hello.monitors = 1;
    hello.system = "Ubuntu 22.04";
    hello.fingerprint = m_id.fingerprint();
    return hello;
}

void AgentApp::note(const std::string& state)
{
    if (m_status) {
        m_status(m_id.deviceId(), m_id.fingerprint(), m_deskLine, state);
    }
}

void AgentApp::sendHello()
{
    std::string err;
    sendPacked(Messages::packHello(helloMsg(), &err));
}

void AgentApp::tune(uint16_t fps, uint8_t quality, uint16_t longEdge, bool thumb)
{
    if (fps >= RemoteConst::kMaxFpsMin && fps <= RemoteConst::kMaxFpsMax) {
        m_fps = fps > 30 ? 30 : static_cast<int>(fps);
    }
    if (quality >= 1 && quality <= 100) {
        m_quality = quality;
    }
    if (longEdge == RemoteConst::kNativeEdge || (longEdge >= 320 && longEdge <= 7680)) {
        if (thumb) {
            m_thumbEdge = longEdge;
        } else {
            m_mainEdge = longEdge;
        }
    }
}

void AgentApp::handle(const Frame::Packet& packet)
{
    std::string err;
    if (packet.channel == Frame::ChControl) {
        if (packet.type == Frame::TypeHelloAck) {
            return;
        }
        if (packet.type == Frame::TypeHeartbeat) {
            HeartbeatMsg beat;
            if (!Messages::unpackHeartbeat(packet.payload, &beat)) {
                return;
            }
            HeartbeatAckMsg ack;
            ack.peerMs = beat.ms;
            ack.selfMs = nowMs();
            sendPacked(Messages::packHeartbeatAck(ack, &err));
            return;
        }
        if (packet.type == Frame::TypeSubscribe) {
            SubscribeMsg sub;
            if (!Messages::unpackSubscribe(packet.payload, &sub)) {
                return;
            }
            const bool on = sub.enable != 0;
            if (sub.stream == 1) {
                m_mainOn = on;
                tune(sub.maxFps, sub.jpegQuality, sub.longEdge, false);
            } else {
                m_thumbOn = on;
                tune(sub.maxFps, sub.jpegQuality, sub.longEdge, true);
            }
            m_nextFrame = 0;
            note((m_mainOn || m_thumbOn) ? "已连接 · 正在送画面" : "已连接 · 等待画面订阅");
            return;
        }
        if (packet.type == Frame::TypeConfig) {
            ConfigMsg cfg;
            if (Messages::unpackConfig(packet.payload, &cfg)) {
                applyConfig(cfg);
            }
            return;
        }
        if (packet.type == Frame::TypeAuthRequest) {
            // 这一版不弹确认、不核对验证码。中控台最高权限和普通申请都直接同意。
            AuthRequestMsg req;
            if (!Messages::unpackAuthRequest(packet.payload, &req)) {
                return;
            }
            AuthResultMsg result;
            result.code = 0;
            result.perms = req.perms ? req.perms : static_cast<uint32_t>(PermDefaultRequest);
            m_perms = result.perms;
            m_operating = (result.perms & PermOperate) != 0;
            sendPacked(Messages::packAuthResult(result, &err));
            refreshCode();
            return;
        }
        if (packet.type == Frame::TypeOperate) {
            OperateMsg op;
            if (!Messages::unpackOperate(packet.payload, &op)) {
                return;
            }
            m_operating = op.action != Frame::OperateRelease;
            return;
        }
        if (packet.type == Frame::TypeBye) {
            m_link.close();
        }
        return;
    }
    if (packet.channel == Frame::ChInput && m_operating) {
        InputMsg input;
        if (m_imageW > 0 && m_imageH > 0 && Messages::unpackInput(packet.payload, &input)) {
            m_desk.inject(input, m_imageW, m_imageH);
        }
        return;
    }
    if (packet.channel == Frame::ChChat && packet.type == Frame::TypeChatText) {
        std::string name;
        std::string text;
        uint8_t kind = 0;
        if (Messages::unpackChat(packet.payload, &name, &text, &kind) && m_chat && !text.empty()) {
            m_chat(name.empty() ? "中控台" : name, text, kind == 1 ? 1 : 0);
        }
        return;
    }
    if (packet.channel == Frame::ChFile) {
        const bool allow = m_operating && (m_perms & PermFile) != 0;
        const char* deny = (m_perms & PermFile) == 0 ? "未允许文件" : "需要操作权";
        flushFile(m_files.take(packet, allow, deny));
    }
}

void AgentApp::sendPictures()
{
    touchCode();
    takeShare();
    refreshWindows();
    drainChat();
    flushFile(m_files.pump());
    const uint64_t now = nowMs();
    if (m_mainOn || m_thumbOn) {
        const int gap = 1000 / (m_fps > 0 ? m_fps : 15);
        if (now >= m_nextFrame) {
            CommBytes jpeg;
            int w = 0;
            int h = 0;
            std::string err;
            const int edge = m_mainOn ? m_mainEdge : m_thumbEdge;
            if (m_desk.grabJpeg(edge, m_quality, &jpeg, &w, &h, &err)) {
                m_imageW = w;
                m_imageH = h;
                PictureMsg picture;
                picture.visible = 1;
                picture.clientW = static_cast<uint32_t>(w);
                picture.clientH = static_cast<uint32_t>(h);
                picture.imageW = static_cast<uint32_t>(w);
                picture.imageH = static_cast<uint32_t>(h);
                picture.codec = 1;
                picture.key = 1;
                // 1 表示这块画面可以申请操作。当前有没有人拿着操作权，由认证结果另说。
                picture.operable = 1;
                picture.jpeg.swap(jpeg);
                bool sentMain = false;
                if (m_mainOn) {
                    sentMain = sendPacked(Messages::packPicture(picture, Frame::ChVideo, &err));
                    if (sentMain) {
                        showPreview(picture.jpeg, w, h);
                    }
                }
                if (m_thumbOn) {
                    if (m_mainOn && m_thumbEdge != m_mainEdge) {
                        CommBytes thumb;
                        int tw = 0;
                        int th = 0;
                        if (m_desk.grabJpeg(m_thumbEdge, m_quality, &thumb, &tw, &th, &err)) {
                            picture.clientW = static_cast<uint32_t>(tw);
                            picture.clientH = static_cast<uint32_t>(th);
                            picture.imageW = static_cast<uint32_t>(tw);
                            picture.imageH = static_cast<uint32_t>(th);
                            picture.jpeg.swap(thumb);
                        }
                    }
                    sendPacked(Messages::packPicture(picture, Frame::ChThumb, &err));
                    if (!sentMain) {
                        showPreview(picture.jpeg, static_cast<int>(picture.imageW), static_cast<int>(picture.imageH));
                    }
                }
                int cx = 0;
                int cy = 0;
                if (m_desk.pointer(m_imageW, m_imageH, &cx, &cy)) {
                    CursorPosMsg cursor;
                    cursor.x = static_cast<uint16_t>(cx < 0 ? 0 : cx);
                    cursor.y = static_cast<uint16_t>(cy < 0 ? 0 : cy);
                    cursor.visible = 1;
                    sendPacked(Messages::packCursorPos(cursor, &err));
                }
            }
            m_nextFrame = now + static_cast<uint64_t>(gap);
        }
    }
    if (now >= m_nextBeat) {
        HeartbeatMsg beat;
        beat.ms = now;
        std::string err;
        sendPacked(Messages::packHeartbeat(beat, &err));
        m_nextBeat = now + 1000;
    }
}

void AgentApp::showPreview(const CommBytes& jpeg, int w, int h)
{
    if (!m_preview) {
        return;
    }
    if (jpeg.empty() || w <= 0 || h <= 0) {
        m_preview(std::string(), 0, 0);
        return;
    }
    const uint64_t now = nowMs();
    if (now < m_nextPreview) {
        return;
    }
    m_nextPreview = now + 400;
    m_preview(std::string(jpeg.begin(), jpeg.end()), w, h);
}

void AgentApp::serve(int fd)
{
    std::string err;
    // 握手失败时 TlsLink 已经关掉套接字
    if (!m_link.handshake(fd, m_id.context(), &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return;
    }
    m_frame.clear();
    m_mainOn = false;
    m_thumbOn = false;
    m_operating = false;
    m_perms = 0;
    m_files.reset();
    m_fileNote.clear();
    m_up.store(true);
    m_nextFrame = 0;
    m_nextBeat = 0;
    sendHello();
    std::fprintf(stderr, "已发送 Hello\n");
    note("已连接");
    while (m_link.fd() >= 0) {
        pollfd pfd;
        pfd.fd = m_link.fd();
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll(&pfd, 1, 30);
        CommBytes chunk;
        if (!m_link.read(&chunk, &err)) {
            std::fprintf(stderr, "%s\n", err.c_str());
            break;
        }
        if (!chunk.empty()) {
            std::vector<Frame::Packet> packets;
            if (!m_frame.push(chunk, &packets, &err)) {
                std::fprintf(stderr, "%s\n", err.c_str());
                break;
            }
            for (size_t i = 0; i < packets.size(); ++i) {
                handle(packets[i]);
            }
        }
        sendPictures();
    }
    m_up.store(false);
    m_link.close();
    m_files.reset();
    flushFile(std::vector<CommBytes>());
    showPreview(CommBytes(), 0, 0);
    note("等待中控台");
    std::fprintf(stderr, "会话结束\n");
}

void AgentApp::setWindowHandler(const std::function<void(const std::vector<std::string>&)>& handler)
{
    m_windows = handler;
}

void AgentApp::setShareHandler(const std::function<void(bool, const std::string&, const std::string&)>& handler)
{
    m_shareUi = handler;
}

void AgentApp::setChatHandler(const std::function<void(const std::string&, const std::string&, int)>& handler)
{
    m_chat = handler;
}

void AgentApp::setFileHandler(const std::function<void(const std::string&)>& handler)
{
    m_fileUi = handler;
}

void AgentApp::chooseShare(bool desktop, const std::string& process, const std::string& title)
{
    std::lock_guard<std::mutex> lock(m_shareMu);
    m_sharePending = true;
    m_shareDesktop = desktop;
    m_shareProcess = process;
    m_shareTitle = title;
}

void AgentApp::submitChat(const std::string& text)
{
    if (text.empty()) {
        return;
    }
    if (!m_up.load()) {
        if (m_chat) {
            m_chat("被控端", "还没有连上中控台", 0);
        }
        return;
    }
    std::lock_guard<std::mutex> lock(m_chatMu);
    m_chatOut.push_back(text);
}

void AgentApp::setCodeHandler(const std::function<void(const std::string&)>& handler)
{
    m_codeUi = handler;
}

std::string AgentApp::refreshCode()
{
    unsigned char rnd[4] = { 0, 0, 0, 0 };
    std::ifstream in("/dev/urandom", std::ios::binary);
    if (in) {
        in.read(reinterpret_cast<char*>(rnd), 4);
    }
    const unsigned int n = (static_cast<unsigned int>(rnd[0]) << 24)
        | (static_cast<unsigned int>(rnd[1]) << 16)
        | (static_cast<unsigned int>(rnd[2]) << 8)
        | static_cast<unsigned int>(rnd[3]);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%06u", n % 1000000u);
    {
        std::lock_guard<std::mutex> lock(m_codeMu);
        m_code = buf;
        m_codeAt = nowMs();
    }
    if (m_codeUi) {
        m_codeUi(buf);
    }
    return buf;
}

void AgentApp::touchCode()
{
    bool stale = false;
    {
        std::lock_guard<std::mutex> lock(m_codeMu);
        stale = m_code.empty() || nowMs() - m_codeAt >= 10ull * 60ull * 1000ull;
    }
    if (stale) {
        refreshCode();
    }
}

void AgentApp::takeShare()
{
    bool desktop = true;
    std::string process;
    std::string title;
    {
        std::lock_guard<std::mutex> lock(m_shareMu);
        if (!m_sharePending) {
            return;
        }
        m_sharePending = false;
        desktop = m_shareDesktop;
        process = m_shareProcess;
        title = m_shareTitle;
    }
    const bool changed = m_desk.desktop() != desktop || m_desk.process() != process || m_desk.title() != title;
    if (!changed) {
        return;
    }
    m_desk.setShare(desktop, process, title);
    m_operating = false;
    m_imageW = 0;
    m_imageH = 0;
    m_beacon.setInfo(helloMsg());
    if (m_link.fd() >= 0) {
        sendHello();
    }
}

void AgentApp::refreshWindows()
{
    const uint64_t now = nowMs();
    if (now < m_nextWindows) {
        return;
    }
    m_nextWindows = now + 2000;
    if (!m_windows) {
        return;
    }
    const std::vector<std::pair<std::string, std::string>> rows = m_desk.windows();
    std::string snap;
    std::vector<std::string> lines;
    lines.reserve(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        const std::string line = rows[i].first + "\t" + rows[i].second;
        lines.push_back(line);
        snap += line;
        snap.push_back('\n');
    }
    if (snap == m_windowSnap) {
        return;
    }
    m_windowSnap = snap;
    m_windows(lines);
}

void AgentApp::drainChat()
{
    std::deque<std::string> pending;
    {
        std::lock_guard<std::mutex> lock(m_chatMu);
        pending.swap(m_chatOut);
    }
    if (!m_up.load()) {
        return;
    }
    std::string err;
    const std::string name = hostName();
    for (size_t i = 0; i < pending.size(); ++i) {
        if (sendPacked(Messages::packChat(name, pending[i], &err))) {
            if (m_chat) {
                m_chat(name, pending[i], 0);
            }
        }
    }
}

void AgentApp::flushFile(const std::vector<CommBytes>& frames)
{
    for (size_t i = 0; i < frames.size(); ++i) {
        sendPacked(frames[i]);
    }
    const std::string note = m_files.status();
    if (note == m_fileNote) {
        return;
    }
    m_fileNote = note;
    if (m_fileUi) {
        m_fileUi(note);
    }
}

void AgentApp::applyConfig(const ConfigMsg& cfg)
{
    tune(cfg.maxFps, cfg.jpegQuality, cfg.longEdge, false);
    const bool desktop = cfg.share == 1;
    const bool changed = m_desk.desktop() != desktop || m_desk.process() != cfg.process || m_desk.title() != cfg.title;
    if (!changed) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_shareMu);
        m_sharePending = false;
        m_shareDesktop = desktop;
        m_shareProcess = cfg.process;
        m_shareTitle = cfg.title;
    }
    m_desk.setShare(desktop, cfg.process, cfg.title);
    m_operating = false;
    m_imageW = 0;
    m_imageH = 0;
    m_beacon.setInfo(helloMsg());
    sendHello();
    if (m_shareUi) {
        m_shareUi(desktop, cfg.process, cfg.title);
    }
}

}
