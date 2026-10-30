/**
 * @file AgentSession.cpp
 * @brief 被控端收发
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentSession.h"
#include "AgentService.h"
#include "RemoteCrash.h"
#include "RemoteLog.h"
#include "SasSender.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <windows.h>

namespace Vision {

int AgentSession::captureMsFromFps(int maxFps)
{
    if (maxFps < RemoteConst::kMaxFpsMin) {
        maxFps = RemoteConst::kMaxFpsMin;
    }
    if (maxFps > RemoteConst::kMaxFpsMax) {
        maxFps = RemoteConst::kMaxFpsMax;
    }
    return 1000 / maxFps;
}

AgentSession::AgentSession()
    : m_run(false)
    , m_pipelineWorkers(0)
    , m_preferCodec(0)
    , m_port(0)
    , m_desktop(false)
    , m_monitor(0)
    , m_originX(0)
    , m_originY(0)
    , m_captureMs(RemoteConst::kCaptureMs)
    , m_jpegQuality(RemoteConst::kJpegQuality)
    , m_longEdge(RemoteConst::kLongEdge)
    , m_subMain(true)
    , m_subThumb(false)
    , m_shareGen(0)
    , m_shareSeen(0)
    , m_mainLatched(false)
    , m_baseCaptureMs(RemoteConst::kCaptureMs)
    , m_baseQuality(RemoteConst::kJpegQuality)
    , m_baseEdge(RemoteConst::kLongEdge)
    , m_rttBase(0)
    , m_lastAdapt(0)
    , m_sentCodec(1)
    , m_streamW(0)
    , m_streamH(0)
    , m_cursorHash(0)
    , m_lastCursorShapeMs(0)
    , m_dirSeq(0)
    , m_fileUiSeq(0)
    , m_fileUiGot(0)
    , m_fileUiBytes(0)
    , m_forceRecord(false)
    , m_httpView(false)
    , m_previewW(0)
    , m_previewH(0)
    , m_httpSeq(0)
    , m_inbound(0)
    , m_relayOn(false)
    , m_relayPort(19200)
    , m_production(false)
    , m_stationBlock(false)
    , m_thumbSeq(0)
    , m_thumbSent(0)
    , m_lastThumbMs(0)
    , m_lastPreviewMs(0)
    , m_cursorDirty(false)
    , m_privacy(false)
    , m_clipImageHash(0)
    , m_clipSeq(0)
    , m_filePull(false)
    , m_fileId(0)
    , m_fileOff(0)
    , m_fileAcked(0)
    , m_fileNext(0)
    , m_fileSize(0)
    , m_file(0)
    , m_fileStop(false)
    , m_operating(false)
    , m_operated(false)
    , m_needHello(false)
    , m_pushed(false)
    , m_suspended(false)
    , m_wantBye(false)
    , m_releaseInput(false)
    , m_lastRx(0)
    , m_connectedAt(0)
    , m_operateAt(0)
    , m_pictureSeq(0)
    , m_sentSeq(0)
    , m_rawSeq(0)
    , m_rawTaken(0)
    , m_confirm(ConfirmNever)
    , m_lockOnEnd(false)
    , m_allow(PermView | PermOperate | PermClipboard | PermFile | PermPower | PermPrivacy)
    , m_pendingActive(false)
    , m_pendingSeq(0)
    , m_pendingDeadline(0)
    , m_deviceId(readDeviceId())
    , m_system(readSystemName())
    , m_pendingPerms(PermView)
{
    RemoteCrash::checkpoint("AgentSession.ctor");
    m_auth.refreshCode();
    std::string err;
    if (!m_identity.ensure("controlled", machineName(), &err)) {
        RemoteCrash::log(("identity.ensure fail: " + err).c_str());
    } else {
        RemoteCrash::checkpoint("AgentSession.identity_ok");
    }
    setFileRoots(std::vector<std::wstring>());
}

AgentSession::~AgentSession()
{
    stop();
}

uint64_t AgentSession::nowMs() const
{
    return GetTickCount64();
}

std::string AgentSession::machineName() const
{
    wchar_t name[256];
    DWORD count = 256;
    if (!GetComputerNameW(name, &count)) {
        return "pc";
    }
    const std::string out = toUtf8(std::wstring(name, count));
    return out.empty() ? std::string("pc") : out;
}

std::string AgentSession::readDeviceId()
{
    std::wstring guid;
    HKEY key = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS) {
        wchar_t value[128];
        DWORD bytes = sizeof(value);
        DWORD type = 0;
        if (RegQueryValueExW(key, L"MachineGuid", 0, &type, reinterpret_cast<BYTE*>(value), &bytes) == ERROR_SUCCESS && type == REG_SZ) {
            guid.assign(value, bytes / sizeof(wchar_t));
            while (!guid.empty() && guid.back() == L'\0') {
                guid.pop_back();
            }
        }
        RegCloseKey(key);
    }
    if (guid.empty()) {
        wchar_t name[256];
        DWORD count = 256;
        if (GetComputerNameW(name, &count)) {
            guid.assign(name, count);
        }
    }
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < guid.size(); ++i) {
        hash ^= static_cast<uint64_t>(guid[i]);
        hash *= 1099511628211ULL;
    }
    const unsigned int n = static_cast<unsigned int>(hash % 1000000000ULL);
    char text[16];
    std::snprintf(text, sizeof(text), "%03u %03u %03u", n / 1000000U, (n / 1000U) % 1000U, n % 1000U);
    return text;
}

std::string AgentSession::readSystemName()
{
    HKEY key = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return "Windows";
    }
    auto readText = [key](const wchar_t* name) {
        wchar_t value[256];
        DWORD bytes = sizeof(value);
        DWORD type = 0;
        if (RegQueryValueExW(key, name, 0, &type, reinterpret_cast<BYTE*>(value), &bytes) != ERROR_SUCCESS || type != REG_SZ) {
            return std::wstring();
        }
        std::wstring out(value, bytes / sizeof(wchar_t));
        while (!out.empty() && out.back() == L'\0') {
            out.pop_back();
        }
        return out;
    };
    std::wstring product = readText(L"ProductName");
    std::wstring version = readText(L"DisplayVersion");
    if (version.empty()) {
        version = readText(L"ReleaseId");
    }
    const std::wstring build = readText(L"CurrentBuildNumber");
    RegCloseKey(key);
    if (!build.empty() && _wtoi(build.c_str()) >= 22000) {
        const size_t at = product.find(L"Windows 10");
        if (at != std::wstring::npos) {
            product.replace(at, 10, L"Windows 11");
        }
    }
    std::wstring text = product.empty() ? std::wstring(L"Windows") : product;
    if (!version.empty()) {
        text += L" " + version;
    }
    return toUtf8(text);
}

std::string AgentSession::toUtf8(const std::wstring& text)
{
    if (text.empty()) {
        return std::string();
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, 0, 0, 0, 0);
    if (bytes <= 1) {
        return std::string();
    }
    std::string out(static_cast<size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &out[0], bytes, 0, 0);
    const size_t limit = static_cast<size_t>(RemoteConst::kTextMax);
    if (out.size() > limit) {
        size_t end = limit;
        while (end > 0 && (static_cast<unsigned char>(out[end]) & 0xC0) == 0x80) {
            --end;
        }
        out.resize(end);
    }
    return out;
}

std::string AgentSession::utf8Full(const std::wstring& text)
{
    if (text.empty()) {
        return std::string();
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, 0, 0, 0, 0);
    if (bytes <= 1) {
        return std::string();
    }
    std::string out(static_cast<size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &out[0], bytes, 0, 0);
    return out;
}

const std::string& AgentSession::deviceId() const
{
    return m_deviceId;
}

const std::string& AgentSession::systemName() const
{
    return m_system;
}

bool AgentSession::start(const std::string& host, int port, const std::wstring& title, const std::wstring& processName,
    bool desktop, int monitor, std::string* err)
{
    if (host.empty() || port <= 0 || port > 65535) {
        if (err) {
            *err = "中控台地址或端口无效";
        }
        RemoteCrash::alert("AgentSession.start", err ? err->c_str() : "bad host/port", true);
        return false;
    }
    bool ok = false;
    const bool ran = RemoteCrash::run("AgentSession.start", [&]() {
    stop();
    m_host = host;
    m_port = port;
    m_desktop = desktop;
    m_monitor = monitor;
    if (desktop) {
        m_source = "桌面";
    } else if (!processName.empty()) {
        m_source = toUtf8(processName);
    } else if (!title.empty()) {
        m_source = toUtf8(title);
    } else {
        m_source = "进程画面";
    }
    m_title = title;
    m_process = processName;
    m_originX = 0;
    m_originY = 0;
    m_run = true;
    m_operating = false;
    m_operated = false;
    m_needHello = false;
    m_suspended = false;
    m_wantBye = false;
    m_releaseInput = false;
    m_pendingActive = false;
    m_controller.clear();
    m_connectedAt = 0;
    m_status = "未连接";
    m_mainLatched = false;
    m_subMain = true;
    m_h264.requestKey();
    if (!desktop) {
        m_watch.start(title, processName);
    }
    publishBeacon();
    std::string beaconErr;
    m_beacon.listen(&beaconErr);
    {
        std::lock_guard<std::mutex> rawLock(m_rawMu);
        m_rawSeq = 0;
        m_rawTaken = 0;
        m_raw = RawFrame();
    }
    m_captureThread = std::thread(&AgentSession::captureMain, this);
    m_encodeThreads.clear();
    // JPEG/Qt 编码非重入，并行只开一条编码线程；再多只会互相拖成百毫秒
    const int workers = m_pipelineWorkers > 0 ? 1 : 0;
    for (int i = 0; i < workers; ++i) {
        m_encodeThreads.push_back(std::thread(&AgentSession::encodeMain, this));
    }
    m_read = std::thread(&AgentSession::readMain, this);
    m_write = std::thread(&AgentSession::writeMain, this);
    ok = true;
    });
    return ran && ok;
}

void AgentSession::stop()
{
    m_direct.stop();
    stopWatches();
    bool bye = false;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_run = false;
        bye = m_wantBye;
        m_wantBye = false;
    }
    m_rawCv.notify_all();
    // 还有没发出的 Bye：等发包线程退出后独占连接尽力发出，再关连接
    if (bye) {
        if (m_write.joinable()) {
            m_write.join();
        }
        std::string err;
        if (m_tcp.isOpen()) {
            m_tcp.write("", Messages::packEmpty(Frame::TypeBye, &err), 500, &err);
        }
    }
    m_tcp.close();
    if (m_read.joinable()) {
        m_read.join();
    }
    if (m_write.joinable()) {
        m_write.join();
    }
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
    for (size_t i = 0; i < m_encodeThreads.size(); ++i) {
        if (m_encodeThreads[i].joinable()) {
            m_encodeThreads[i].join();
        }
    }
    m_encodeThreads.clear();
    // 收包线程已退出，这里独占 m_input
    m_input.releaseAll();
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_releaseInput = false;
    }
    m_watch.stop();
    m_beacon.close();
}

void AgentSession::setPipelineWorkers(int workers)
{
    if (workers < 0) {
        workers = 0;
    }
    if (workers > 1) {
        workers = 1;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    m_pipelineWorkers = workers;
}

int AgentSession::pipelineWorkers() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_pipelineWorkers;
}

void AgentSession::setPreferCodec(int codec)
{
    if (codec < 0 || codec > 3) {
        codec = 0;
    }
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        if (m_preferCodec == codec) {
            return;
        }
        m_preferCodec = codec;
        m_mainLatched = false;
        m_shareGen += 1;
        changed = true;
    }
    if (changed) {
        m_h264.closeEncoder();
        m_h264.requestKey();
        RemoteLog::writef(RemoteLog::Info, "画面", "本地编码切换 codec=%d", codec);
    }
}

int AgentSession::preferCodec() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_preferCodec;
}

AgentSession::FileUi AgentSession::fileUi() const
{
    FileUi out;
    std::lock_guard<std::mutex> lock(m_mu);
    out.online = m_tcp.isOpen() && m_connectedAt != 0;
    out.dirList = m_dirList;
    out.dirSeq = m_dirSeq;
    out.fileSeq = m_fileUiSeq;
    out.fileGot = m_fileUiGot;
    out.fileBytes = m_fileUiBytes;
    out.fileNote = m_fileUiNote;
    return out;
}

void AgentSession::requestRemoteDir(const std::string& path)
{
    std::string err;
    enqueue(Messages::packListDir(path, &err));
}

void AgentSession::requestRemoteMkDir(uint32_t id, const std::string& remotePath)
{
    std::string err;
    enqueue(Messages::packMkDir(id, remotePath, &err));
}

void AgentSession::publishRaw(CommBytes& bgra, int width, int height, int originX, int originY, bool desktop,
    bool operable, bool forcePublish, uint32_t shareGen, int jpegQuality, int longEdge, int captureMs,
    bool subMain, bool subThumb, uint64_t started, uint64_t grabMs)
{
    {
        std::lock_guard<std::mutex> lock(m_rawMu);
        m_raw.bgra.swap(bgra);
        m_raw.width = width;
        m_raw.height = height;
        m_raw.originX = originX;
        m_raw.originY = originY;
        m_raw.desktop = desktop;
        m_raw.operable = operable;
        m_raw.forcePublish = forcePublish;
        m_raw.shareGen = shareGen;
        m_raw.jpegQuality = jpegQuality;
        m_raw.longEdge = longEdge;
        m_raw.captureMs = captureMs;
        m_raw.subMain = subMain;
        m_raw.subThumb = subThumb;
        m_raw.started = started;
        m_raw.grabMs = grabMs;
        m_rawSeq += 1;
    }
    m_rawCv.notify_one();
}

std::string AgentSession::status() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_status;
}

AgentSession::Info AgentSession::info() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    Info out;
    out.status = m_status;
    out.connected = m_connectedAt != 0;
    out.suspended = m_suspended;
    out.operating = m_operating;
    out.localPause = m_status == "本地暂停";
    out.privacy = m_privacy;
    out.controller = m_controller;
    out.host = m_host;
    out.port = m_port;
    const uint64_t now = nowMs();
    out.connectedMs = m_connectedAt != 0 ? now - m_connectedAt : 0;
    out.operateMs = m_operating && m_operateAt != 0 ? now - m_operateAt : 0;
    return out;
}

CommBytes AgentSession::preview(uint32_t* imageW, uint32_t* imageH) const
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (imageW) {
        *imageW = m_picture.imageW;
    }
    if (imageH) {
        *imageH = m_previewH != 0 ? m_previewH : m_picture.imageH;
    }
    if (imageW && m_previewW != 0) {
        *imageW = m_previewW;
    }
    if (!m_previewJpeg.empty()) {
        return m_previewJpeg;
    }
    if (m_picture.codec == 1) {
        return m_picture.jpeg;
    }
    return CommBytes();
}

void AgentSession::keepPreview(const PictureMsg& jpegShot)
{
    if (jpegShot.jpeg.empty() || jpegShot.codec == 2) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    m_previewJpeg = jpegShot.jpeg;
    m_previewW = jpegShot.imageW;
    m_previewH = jpegShot.imageH;
}

void AgentSession::refreshLocalPreview(CommBytes& bgra, int width, int height, uint64_t nowMs)
{
    if (bgra.empty() || width <= 0 || height <= 0) {
        return;
    }
    // 预览只给被控端小窗看，不必跟主码流同帧率；每帧再压一份 JPEG 会明显拖慢编码
    if (m_lastPreviewMs != 0 && nowMs - m_lastPreviewMs < 250) {
        return;
    }
    PictureMsg local = m_capture.fromBgra(bgra, width, height, width, height, true, 40, 480);
    local.codec = 1;
    keepPreview(local);
    m_lastPreviewMs = nowMs;
}

void AgentSession::setPolicy(int confirm, const std::vector<std::string>& trusted, bool lockOnEnd)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_confirm = confirm;
    m_trusted = trusted;
    m_lockOnEnd = lockOnEnd;
}

void AgentSession::setAllow(uint32_t mask)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_allow = mask | PermView;
    if ((m_allow & PermOperate) == 0 && m_operating) {
        m_operating = false;
        m_releaseInput = true;
        m_status = "只看";
    }
    if ((m_allow & PermPrivacy) == 0) {
        m_privacy = false;
    }
}

bool AgentSession::allows(uint32_t bit) const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return (m_allow & bit) != 0;
}

uint32_t AgentSession::allowMask() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_allow | PermView;
}

void AgentSession::setWhitelist(const std::vector<std::string>& allow)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_whitelist = allow;
}

std::string AgentSession::setPassword(const std::string& password)
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_auth.setPassword(password);
}

void AgentSession::loadPasswordHash(const std::string& stored)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_auth.loadPasswordHash(stored);
}

std::string AgentSession::currentCode()
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_auth.currentCode();
}

std::string AgentSession::refreshCode()
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_auth.refreshCode();
}

bool AgentSession::hasPassword() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_auth.hasPassword();
}

std::string AgentSession::fingerprint() const
{
    if (m_identity.ready()) {
        return m_identity.fingerprint();
    }
    return RemoteAuth::fingerprintOf(m_deviceId);
}

std::string AgentSession::peerFingerprint() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_peerFingerprint;
}

bool AgentSession::tlsReady() const
{
    return m_tcp.tlsReady();
}

bool AgentSession::needConfirm() const
{
    if (m_production) {
        return true;
    }
    if (m_confirm == ConfirmAlways) {
        return true;
    }
    if (m_confirm == ConfirmUntrusted) {
        if (!m_peerFingerprint.empty()
            && std::find(m_trusted.begin(), m_trusted.end(), m_peerFingerprint) != m_trusted.end()) {
            return false;
        }
        return m_controller.empty() || std::find(m_trusted.begin(), m_trusted.end(), m_controller) == m_trusted.end();
    }
    return false;
}

AgentSession::Pending AgentSession::pending() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    Pending out;
    out.active = m_pendingActive;
    out.seq = m_pendingSeq;
    out.controller = m_controller;
    out.host = m_host;
    const uint64_t now = nowMs();
    out.secondsLeft = m_pendingActive && m_pendingDeadline > now ? static_cast<int>((m_pendingDeadline - now + 999) / 1000) : 0;
    return out;
}

void AgentSession::answerAuth(uint64_t seq, bool allow)
{
    AuthResultMsg ack;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        if (!m_pendingActive || seq != m_pendingSeq) {
            return;
        }
        m_pendingActive = false;
        if (allow && !m_operating) {
            ack.code = 0;
            ack.perms = m_pendingPerms | PermView;
            m_operating = (ack.perms & PermOperate) != 0;
            if (m_operating) {
                m_operated = true;
                m_operateAt = nowMs();
                m_status = "操作中";
            }
            m_auth.onSuccess();
        } else {
            ack.code = 3;
            ack.perms = PermView;
        }
    }
    std::string err;
    enqueue(Messages::packAuthResult(ack, &err));
}

void AgentSession::pauseOperate()
{
    {
        std::lock_guard<std::mutex> lock(m_mu);
        if (!m_operating) {
            return;
        }
        m_operating = false;
        m_releaseInput = true;
        m_status = "只看";
    }
    std::string err;
    OperateMsg op;
    op.action = Frame::OperateRelease;
    enqueue(Messages::packOperate(op, &err));
}

void AgentSession::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_suspended = true;
    m_wantBye = true;
    m_releaseInput = true;
    m_status = "已断开";
}

void AgentSession::resume()
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_suspended) {
        m_suspended = false;
        m_status = "未连接";
        // 没有连接可告别时撤销待发的 Bye，否则重连后第一件事就是把新会话断开
        if (!m_tcp.isOpen()) {
            m_wantBye = false;
        }
    }
}

void AgentSession::endSession(bool remote)
{
    if (remote && m_operated && m_lockOnEnd) {
        LockWorkStation();
    }
    m_operating = false;
    m_operated = false;
    m_releaseInput = true;
    m_pendingActive = false;
    m_connectedAt = 0;
    m_controller.clear();
    m_frame.clear();
    m_privacy = false;
    m_fileStop = true;
}

void AgentSession::enqueue(const CommBytes& frame)
{
    if (frame.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_out.size() >= 64) {
        m_out.erase(m_out.begin());
    }
    m_out.push_back(frame);
}

void AgentSession::publishBeacon()
{
    HelloMsg info;
    info.name = machineName();
    info.deviceId = m_deviceId;
    info.system = m_system;
    info.monitors = static_cast<uint8_t>(WindowCapture::monitorCount());
    {
        std::lock_guard<std::mutex> lock(m_mu);
        info.source = m_source;
        info.share = m_desktop ? 1 : 0;
        info.monitor = static_cast<uint8_t>(m_monitor < 0 ? 0 : m_monitor);
        info.title = toUtf8(m_title);
        info.process = toUtf8(m_process);
    }
    m_beacon.setInfo(info);
}

void AgentSession::applyControl(const ConfigMsg& cfg)
{
    const bool desktop = cfg.share == 1;
    const std::wstring title = toWide(cfg.title);
    const std::wstring process = toWide(cfg.process);
    bool shareChanged = false;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        shareChanged = (m_desktop != desktop) || (m_monitor != cfg.monitor) || (m_title != title)
            || (m_process != process);
        m_desktop = desktop;
        m_monitor = cfg.monitor;
        m_title = title;
        m_process = process;
        if (cfg.maxFps > 0) {
            m_baseCaptureMs = captureMsFromFps(cfg.maxFps);
            m_captureMs = m_baseCaptureMs;
        }
        if (cfg.jpegQuality > 0) {
            m_jpegQuality = cfg.jpegQuality > 100 ? 100 : cfg.jpegQuality;
        }
        if (cfg.longEdge > 0) {
            m_longEdge = cfg.longEdge < 320 ? 320 : cfg.longEdge;
        }
        bool codecChanged = false;
        if (cfg.codec >= 1 && cfg.codec <= 3 && m_preferCodec != static_cast<int>(cfg.codec)) {
            m_preferCodec = cfg.codec;
            m_mainLatched = false;
            codecChanged = true;
        }
        if (desktop) {
            m_source = "桌面";
        } else if (!process.empty()) {
            m_source = toUtf8(process);
        } else if (!title.empty()) {
            m_source = toUtf8(title);
        } else {
            m_source = "进程画面";
        }
        if (shareChanged || codecChanged) {
            m_needHello = true;
            m_shareGen += 1;
        }
        if (shareChanged) {
            m_pushed = true;
            m_operating = false;
            m_releaseInput = true;
            m_status = "中控台已设置";
        }
    }
    if (cfg.codec >= 1 && cfg.codec <= 3) {
        m_h264.closeEncoder();
        m_h264.requestKey();
    }
    if (!shareChanged) {
        return;
    }
    if (desktop) {
        m_watch.stop();
    } else {
        m_watch.start(title, process);
    }
    publishBeacon();
}

void AgentSession::applySubscribe(const SubscribeMsg& msg)
{
    bool codecChanged = false;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        const bool on = msg.enable != 0;
        if (msg.stream == 1) {
            m_subMain = on;
            if (msg.maxFps > 0) {
                m_baseCaptureMs = captureMsFromFps(msg.maxFps);
                m_captureMs = m_baseCaptureMs;
            }
            if (msg.jpegQuality > 0) {
                m_jpegQuality = msg.jpegQuality > 100 ? 100 : msg.jpegQuality;
                m_baseQuality = m_jpegQuality;
            }
            if (msg.longEdge > 0) {
                m_longEdge = msg.longEdge < 320 ? 320 : msg.longEdge;
                m_baseEdge = m_longEdge;
            }
            if (msg.codec >= 1 && msg.codec <= 3 && m_preferCodec != static_cast<int>(msg.codec)) {
                m_preferCodec = msg.codec;
                m_mainLatched = false;
                m_shareGen += 1;
                codecChanged = true;
            }
        } else if (msg.stream == 2) {
            m_subThumb = on;
        }
    }
    if (codecChanged) {
        m_h264.closeEncoder();
        m_h264.requestKey();
        RemoteLog::writef(RemoteLog::Info, "画面", "中控台指定编码 codec=%u", static_cast<unsigned>(msg.codec));
    }
}

void AgentSession::applyNetStats(const NetStatsMsg& msg)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const uint64_t now = nowMs();
    const int backlog = m_pictureSeq > m_sentSeq ? static_cast<int>(m_pictureSeq - m_sentSeq) : 0;
    bool bad = msg.congested != 0 || backlog > 4;
    if (m_rttBase <= 0 && msg.rttMs > 0) {
        m_rttBase = msg.rttMs;
    }
    if (!bad && m_rttBase > 0 && msg.rttMs > 800 && msg.rttMs > m_rttBase + m_rttBase / 2) {
        bad = true;
    } else if (!bad && msg.rttMs > 0) {
        m_rttBase = (m_rttBase * 3 + msg.rttMs) / 4;
    }
    stepAdapt(bad, now);
}

void AgentSession::stepAdapt(bool bad, uint64_t now)
{
    if (m_lastAdapt != 0 && now - m_lastAdapt < 2000) {
        return;
    }
    const int beforeEdge = m_longEdge;
    const int beforeQ = m_jpegQuality;
    const int beforeMs = m_captureMs;
    const int fpsFloor = 100;
    if (bad) {
        if (m_longEdge > 960) {
            m_longEdge = m_longEdge * 4 / 5;
            if (m_longEdge < 960) {
                m_longEdge = 960;
            }
        } else if (m_jpegQuality > 45) {
            m_jpegQuality = m_jpegQuality * 4 / 5;
            if (m_jpegQuality < 45) {
                m_jpegQuality = 45;
            }
        } else if (m_captureMs < fpsFloor) {
            int slower = m_captureMs + 15;
            if (slower > fpsFloor) {
                slower = fpsFloor;
            }
            m_captureMs = slower;
        }
        m_lastAdapt = now;
        if (beforeEdge != m_longEdge || beforeQ != m_jpegQuality || beforeMs != m_captureMs) {
            RemoteLog::writef(RemoteLog::Warn, "画面",
                "自适应降档 边%d→%d Q%d→%d 间隔%d→%dms",
                beforeEdge, m_longEdge, beforeQ, m_jpegQuality, beforeMs, m_captureMs);
        }
        return;
    }
    if (m_captureMs > m_baseCaptureMs) {
        m_captureMs = m_captureMs / 2;
        if (m_captureMs < m_baseCaptureMs) {
            m_captureMs = m_baseCaptureMs;
        }
    } else if (m_longEdge < m_baseEdge) {
        int step = m_baseEdge / 10;
        if (step < 1) {
            step = 1;
        }
        m_longEdge += step;
        if (m_longEdge > m_baseEdge) {
            m_longEdge = m_baseEdge;
        }
    } else if (m_jpegQuality < m_baseQuality) {
        int step = m_baseQuality / 5;
        if (step < 1) {
            step = 1;
        }
        m_jpegQuality += step;
        if (m_jpegQuality > m_baseQuality) {
            m_jpegQuality = m_baseQuality;
        }
    }
    m_lastAdapt = now;
    if (beforeEdge != m_longEdge || beforeQ != m_jpegQuality || beforeMs != m_captureMs) {
        RemoteLog::writef(RemoteLog::Info, "画面",
            "自适应恢复 边%d→%d Q%d→%d 间隔%d→%dms",
            beforeEdge, m_longEdge, beforeQ, m_jpegQuality, beforeMs, m_captureMs);
    }
}

void AgentSession::setFileRoots(const std::vector<std::wstring>& roots)
{
    std::vector<std::wstring> next = roots;
    if (next.empty()) {
        if (GetDriveTypeW(L"D:\\") != DRIVE_NO_ROOT_DIR && GetDriveTypeW(L"D:\\") != DRIVE_UNKNOWN) {
            next.push_back(L"D:\\");
        } else {
            wchar_t windir[MAX_PATH];
            if (GetWindowsDirectoryW(windir, MAX_PATH) > 0 && windir[1] == L':') {
                next.push_back(std::wstring(1, windir[0]) + L":\\");
            } else {
                next.push_back(L"C:\\");
            }
        }
    }
    std::lock_guard<std::mutex> lock(m_mu);
    m_roots.swap(next);
}

bool AgentSession::setDirect(bool on, int port, std::string* err)
{
    if (!on) {
        m_direct.stop();
        return true;
    }
    return m_direct.start(this, port > 0 ? port : 19102, err);
}

void AgentSession::setRelay(bool on, const std::string& host, int port, const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_relayOn = on && !host.empty();
    m_relayHost = host;
    m_relayPort = port > 0 ? port : 19200;
    m_relayToken = token;
}

void AgentSession::setProduction(bool on)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_production = on;
}

bool AgentSession::offerInbound(uintptr_t sock, const std::string& remote)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_tcp.isOpen() || m_inbound != 0) {
        return false;
    }
    m_inbound = sock;
    m_inboundRemote = remote;
    return true;
}

bool AgentSession::takeInbound()
{
    uintptr_t sock = 0;
    std::string remote;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        sock = m_inbound;
        remote = m_inboundRemote;
        m_inbound = 0;
        m_inboundRemote.clear();
    }
    if (sock == 0) {
        return false;
    }
    std::string err;
    if (!m_tcp.attach(sock, remote, &err)) {
        return false;
    }
    return true;
}

bool AgentSession::openViaRelay(std::string* err)
{
    bool on = false;
    std::string host;
    int port = 19200;
    std::string token;
    std::string id;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        on = m_relayOn;
        host = m_relayHost;
        port = m_relayPort;
        token = m_relayToken;
        id = m_deviceId;
    }
    if (!on) {
        return false;
    }
    for (size_t i = 0; i < id.size();) {
        if (id[i] == ' ') {
            id.erase(i, 1);
        } else {
            ++i;
        }
    }
    if (!m_tcp.open(host, port, 3000, err)) {
        return false;
    }
    const std::string line = "REG " + id + " " + token + "\n";
    CommBytes bytes(line.begin(), line.end());
    if (!m_tcp.write("", bytes, 3000, err)) {
        m_tcp.close();
        return false;
    }
    std::string got;
    for (int i = 0; i < 64; ++i) {
        const CommBytes one = m_tcp.read("", 1, 5000, err);
        if (one.empty()) {
            m_tcp.close();
            return false;
        }
        got.push_back(static_cast<char>(one[0]));
        if (one[0] == '\n') {
            break;
        }
    }
    if (got.find("GO") == std::string::npos) {
        // WAIT 表示已登记，等中控台来配对。继续读到 GO 或失败。
        if (got.find("WAIT") != std::string::npos) {
            got.clear();
            for (int i = 0; i < 64; ++i) {
                const CommBytes one = m_tcp.read("", 1, 30000, err);
                if (one.empty()) {
                    m_tcp.close();
                    return false;
                }
                got.push_back(static_cast<char>(one[0]));
                if (one[0] == '\n') {
                    break;
                }
            }
        }
    }
    if (got.find("GO") == std::string::npos) {
        m_tcp.close();
        if (err) {
            *err = "中继拒绝";
        }
        return false;
    }
    return true;
}

void AgentSession::refreshStation()
{
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(0, path, MAX_PATH) == 0) {
        return;
    }
    std::wstring file = path;
    const size_t slash = file.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        file.resize(slash);
    }
    file += L"\\station.json";
    HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        std::lock_guard<std::mutex> lock(m_mu);
        m_stationBlock = false;
        m_stationText.clear();
        return;
    }
    char buf[1024];
    DWORD got = 0;
    ReadFile(handle, buf, sizeof(buf) - 1, &got, 0);
    CloseHandle(handle);
    buf[got] = 0;
    const std::string text(buf, buf + got);
    const bool running = text.find("\"running\":true") != std::string::npos || text.find("\"running\": true") != std::string::npos;
    const bool block = text.find("\"block\":true") != std::string::npos || text.find("\"block\": true") != std::string::npos;
    const bool ng = text.find("\"ng\":") != std::string::npos && text.find("\"ng\":0") == std::string::npos && text.find("\"ng\": 0") == std::string::npos;
    std::string shown = running ? "工站运行" : "工站空闲";
    if (ng) {
        shown += " NG";
    }
    std::lock_guard<std::mutex> lock(m_mu);
    if (running) {
        m_production = true;
    }
    m_stationBlock = block;
    m_stationText = shown;
}

void AgentSession::setHttpView(bool on)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_httpView = on;
    if (!on) {
        m_httpJpeg.clear();
    }
}

bool AgentSession::copyHttpJpeg(CommBytes* jpeg, uint64_t* seq) const
{
    if (!jpeg) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_httpView || m_httpJpeg.empty()) {
        return false;
    }
    *jpeg = m_httpJpeg;
    if (seq) {
        *seq = m_httpSeq;
    }
    return true;
}

void AgentSession::publishHttp(const CommBytes& jpeg)
{
    if (jpeg.empty() || jpeg.size() > 2 * 1024 * 1024) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_httpView) {
        return;
    }
    m_httpJpeg = jpeg;
    m_httpSeq += 1;
}

void AgentSession::publishHttpBgra(CommBytes& bgra, int width, int height)
{
    bool on = false;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        on = m_httpView;
    }
    if (!on) {
        return;
    }
    PictureMsg shot = m_capture.fromBgra(bgra, width, height, width, height, true, 45, 960);
    publishHttp(shot.jpeg);
}

void AgentSession::setForceRecord(bool on)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_forceRecord == on) {
        return;
    }
    m_forceRecord = on;
    m_needHello = true;
}

bool AgentSession::encodePicture(const CommBytes& bgra, int width, int height, int quality, int longEdge, int fps, PictureMsg* shot)
{
    if (!shot || bgra.empty()) {
        return false;
    }
    std::vector<unsigned char> annex;
    int outW = 0;
    int outH = 0;
    uint8_t key = 0;
    const int bitrate = (quality < 1 ? 60 : quality) * 40000;
    if (!m_h264.encode(bgra, width, height, longEdge, fps, bitrate, &annex, &outW, &outH, &key)) {
        if (m_h264.isBroken()) {
            static bool s_brokenLog = false;
            if (!s_brokenLog) {
                s_brokenLog = true;
                RemoteLog::writef(RemoteLog::Warn, "画面", "H264不可用 · %s", m_h264.lastError());
            }
        }
        return false;
    }
    static bool s_okLog = false;
    if (!s_okLog) {
        s_okLog = true;
        RemoteLog::writef(RemoteLog::Info, "画面", "H264已启用 · %s %dx%d", m_h264.lastError(), outW, outH);
    }
    shot->visible = 1;
    shot->operable = 1;
    shot->clientW = static_cast<uint32_t>(width);
    shot->clientH = static_cast<uint32_t>(height);
    shot->imageW = static_cast<uint32_t>(outW);
    shot->imageH = static_cast<uint32_t>(outH);
    shot->codec = 2;
    shot->key = key;
    shot->jpeg.swap(annex);
    return true;
}

bool AgentSession::encodeRaw(const CommBytes& bgra, int width, int height, int longEdge, PictureMsg* shot)
{
    if (!shot || bgra.empty() || width < 2 || height < 2) {
        return false;
    }
    // 原图带宽大，长边默认再压一档，避免单帧爆炸
    int edge = longEdge > 0 ? longEdge : 1280;
    if (edge > 1600) {
        edge = 1600;
    }
    std::vector<unsigned char> scaled;
    int outW = 0;
    int outH = 0;
    if (!m_h264.scaleBgra(bgra.data(), width, height, edge, &scaled, &outW, &outH) || scaled.empty()) {
        return false;
    }
    shot->visible = 1;
    shot->operable = 1;
    shot->clientW = static_cast<uint32_t>(width);
    shot->clientH = static_cast<uint32_t>(height);
    shot->imageW = static_cast<uint32_t>(outW);
    shot->imageH = static_cast<uint32_t>(outH);
    shot->codec = 3;
    shot->key = 1;
    shot->jpeg.assign(scaled.begin(), scaled.end());
    return true;
}

bool AgentSession::encodeByPrefer(const CommBytes& bgra, int width, int height, int quality, int longEdge, int fps,
    PictureMsg* shot, const char** pathName)
{
    if (!shot) {
        return false;
    }
    int prefer = 0;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        prefer = m_preferCodec;
    }
    if (prefer == 3) {
        if (!encodeRaw(bgra, width, height, longEdge, shot)) {
            return false;
        }
        if (pathName) {
            *pathName = "原图";
        }
        keepPreview(m_capture.fromBgra(const_cast<CommBytes&>(bgra), width, height, width, height, true, 50, 480));
        return true;
    }
    if (prefer == 1) {
        *shot = m_capture.fromBgra(const_cast<CommBytes&>(bgra), width, height, width, height, true, quality, longEdge);
        shot->codec = 1;
        keepPreview(*shot);
        if (pathName) {
            *pathName = "JPEG";
        }
        return !shot->jpeg.empty();
    }
    // 2=强制 H.264，0=自动优先 H.264
    if (encodePicture(bgra, width, height, quality, longEdge, fps, shot)) {
        if (pathName) {
            *pathName = "H264";
        }
        return true;
    }
    if (prefer == 2 && !m_h264.isBroken()) {
        // 预热中：本帧跳过，避免夹 JPEG 把中控台解码器打乱
        if (pathName) {
            *pathName = "H264预热";
        }
        return false;
    }
    *shot = m_capture.fromBgra(const_cast<CommBytes&>(bgra), width, height, width, height, true, quality, longEdge);
    shot->codec = 1;
    keepPreview(*shot);
    if (pathName) {
        *pathName = prefer == 2 ? "JPEG(H264失败)" : "JPEG";
    }
    return !shot->jpeg.empty();
}

CommBytes AgentSession::captureCursorShape()
{
    const uint64_t now = nowMs();
    if (m_lastCursorShapeMs != 0 && now - m_lastCursorShapeMs < 100) {
        return CommBytes();
    }
    m_lastCursorShapeMs = now;
    CURSORINFO info;
    info.cbSize = sizeof(info);
    if (!GetCursorInfo(&info) || !info.hCursor) {
        return CommBytes();
    }
    ICONINFO icon;
    if (!GetIconInfo(info.hCursor, &icon)) {
        return CommBytes();
    }
    BITMAP bmp;
    const HBITMAP color = icon.hbmColor ? icon.hbmColor : icon.hbmMask;
    if (!color || GetObject(color, sizeof(bmp), &bmp) == 0 || bmp.bmWidth <= 0 || bmp.bmHeight <= 0) {
        if (icon.hbmColor) {
            DeleteObject(icon.hbmColor);
        }
        if (icon.hbmMask) {
            DeleteObject(icon.hbmMask);
        }
        return CommBytes();
    }
    int w = bmp.bmWidth;
    int h = icon.hbmColor ? bmp.bmHeight : bmp.bmHeight / 2;
    if (w > 64) {
        w = 64;
    }
    if (h > 64) {
        h = 64;
    }
    if (h < 1) {
        h = 1;
    }
    HDC screen = GetDC(0);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi;
    std::memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = 0;
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, 0, 0);
    HGDIOBJ old = SelectObject(mem, dib);
    DrawIconEx(mem, 0, 0, info.hCursor, w, h, 0, 0, DI_NORMAL);
    SelectObject(mem, old);
    uint64_t hash = 1469598103934665603ull;
    const unsigned char* bytes = static_cast<const unsigned char*>(bits);
    const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    for (size_t i = 0; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    hash ^= static_cast<uint64_t>(icon.xHotspot) << 32;
    hash ^= static_cast<uint64_t>(icon.yHotspot);
    CommBytes packet;
    if (hash != m_cursorHash && bits) {
        m_cursorHash = hash;
        CursorShapeMsg shape;
        const int hotScaleX = bmp.bmWidth > 0 ? w : 1;
        const int hotScaleY = (icon.hbmColor ? bmp.bmHeight : h) > 0 ? h : 1;
        shape.hotX = static_cast<uint16_t>(icon.xHotspot * hotScaleX / (bmp.bmWidth > 0 ? bmp.bmWidth : 1));
        shape.hotY = static_cast<uint16_t>(icon.yHotspot * hotScaleY / ((icon.hbmColor ? bmp.bmHeight : h) > 0 ? (icon.hbmColor ? bmp.bmHeight : h) : 1));
        shape.width = static_cast<uint16_t>(w);
        shape.height = static_cast<uint16_t>(h);
        if (m_capture.pngFromBgra(bytes, w, h, &shape.png)) {
            std::string err;
            packet = Messages::packCursorShape(shape, &err);
        }
    }
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(0, screen);
    if (icon.hbmColor) {
        DeleteObject(icon.hbmColor);
    }
    if (icon.hbmMask) {
        DeleteObject(icon.hbmMask);
    }
    return packet;
}

bool AgentSession::takePushed(bool* desktop, int* monitor, std::wstring* title, std::wstring* process)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_pushed) {
        return false;
    }
    m_pushed = false;
    if (desktop) {
        *desktop = m_desktop;
    }
    if (monitor) {
        *monitor = m_monitor;
    }
    if (title) {
        *title = m_title;
    }
    if (process) {
        *process = m_process;
    }
    return true;
}

std::wstring AgentSession::toWide(const std::string& text)
{
    if (text.empty()) {
        return std::wstring();
    }
    const int chars = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, 0, 0);
    if (chars <= 1) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(chars - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &out[0], chars);
    return out;
}

void AgentSession::commitEncoded(PictureMsg& shot, PictureMsg* thumb, bool haveThumb, const CursorPosMsg* cursor,
    bool haveCursor, int originX, int originY, bool desktop, bool forcePublish, uint32_t shareGen,
    bool subMain, int captureMs, uint64_t started, const char* pathName, uint64_t grabMs,
    uint64_t encodeMs, uint64_t previewMs, uint64_t thumbMs)
{
    (void)forcePublish;
    const int fps = captureMs > 0 ? 1000 / captureMs : 15;
    std::lock_guard<std::mutex> lock(m_mu);
    if (desktop) {
        m_originX = originX;
        m_originY = originY;
    }
    if (haveCursor && cursor) {
        if (cursor->x != m_cursor.x || cursor->y != m_cursor.y || cursor->visible != m_cursor.visible) {
            m_cursor = *cursor;
            m_cursorDirty = true;
        }
    }
    if (subMain && !shot.jpeg.empty()) {
        m_picture = shot;
        m_pictureSeq += 1;
        m_mainLatched = true;
        m_shareSeen = shareGen;
        if (shot.codec != m_sentCodec || shot.imageW != m_streamW || shot.imageH != m_streamH) {
            m_sentCodec = shot.codec == 0 ? 1 : shot.codec;
            m_streamW = shot.imageW;
            m_streamH = shot.imageH;
            StreamInfoMsg info;
            info.stream = 1;
            info.codec = m_sentCodec;
            info.width = static_cast<uint16_t>(shot.imageW > 65535 ? 65535 : shot.imageW);
            info.height = static_cast<uint16_t>(shot.imageH > 65535 ? 65535 : shot.imageH);
            info.fps = static_cast<uint8_t>(fps > 255 ? 255 : fps);
            std::string infoErr;
            const CommBytes packet = Messages::packStreamInfo(info, &infoErr);
            if (!packet.empty()) {
                if (m_out.size() >= 64) {
                    m_out.erase(m_out.begin());
                }
                m_out.push_back(packet);
            }
        }
        // 流水线下积压 1–2 帧是正常的（发不过来就丢旧帧），不要当成拥塞去降帧率
        if (m_pictureSeq > m_sentSeq + 8) {
            stepAdapt(true, started);
        }
        RemoteLog::writef(RemoteLog::Info, "画面",
            "抓%llums 预览%llums 编%llums(%s) 拇指%llums 目标间隔%dms %ux%u Q%d 边%d %zuB 流水线%d",
            static_cast<unsigned long long>(grabMs),
            static_cast<unsigned long long>(previewMs),
            static_cast<unsigned long long>(encodeMs),
            pathName ? pathName : "-",
            static_cast<unsigned long long>(thumbMs),
            captureMs,
            shot.imageW,
            shot.imageH,
            m_jpegQuality,
            m_longEdge,
            shot.jpeg.size(),
            m_pipelineWorkers);
    }
    if (haveThumb && thumb && !thumb->jpeg.empty()) {
        m_thumb = *thumb;
        m_thumbSeq += 1;
        m_lastThumbMs = started;
    }
    if (m_suspended) {
        m_status = "已断开";
    } else if (!m_tcp.isOpen()) {
        m_status = shot.visible ? "未连接" : (desktop ? "没有这块显示器" : "未找到窗口");
    } else if (subMain && shot.jpeg.empty()) {
        m_status = "没有抓到画面";
    } else if (m_status == "没有抓到画面" || m_status == "没有这块显示器" || m_status == "未找到窗口") {
        m_status = m_operating ? "操作中" : "只看";
    }
}

void AgentSession::encodeMain()
{
    try {
    while (true) {
        RawFrame raw;
        uint64_t seq = 0;
        {
            std::unique_lock<std::mutex> lock(m_rawMu);
            m_rawCv.wait(lock, [this]() {
                return !m_run || m_rawSeq != m_rawTaken;
            });
            if (!m_run && m_rawSeq == m_rawTaken) {
                return;
            }
            seq = m_rawSeq;
            m_rawTaken = m_rawSeq;
            raw.bgra.swap(m_raw.bgra);
            raw.width = m_raw.width;
            raw.height = m_raw.height;
            raw.originX = m_raw.originX;
            raw.originY = m_raw.originY;
            raw.desktop = m_raw.desktop;
            raw.operable = m_raw.operable;
            raw.forcePublish = m_raw.forcePublish;
            raw.shareGen = m_raw.shareGen;
            raw.jpegQuality = m_raw.jpegQuality;
            raw.longEdge = m_raw.longEdge;
            raw.captureMs = m_raw.captureMs;
            raw.subMain = m_raw.subMain;
            raw.subThumb = m_raw.subThumb;
            raw.started = m_raw.started;
            raw.grabMs = m_raw.grabMs;
        }
        (void)seq;
        if (raw.bgra.empty() || raw.width <= 0 || raw.height <= 0) {
            continue;
        }
        PictureMsg shot;
        PictureMsg thumb;
        const int fps = raw.captureMs > 0 ? 1000 / raw.captureMs : 15;
        const uint64_t e0 = RemoteLog::tickMs();
        const char* pathName = "JPEG";
        if (!encodeByPrefer(raw.bgra, raw.width, raw.height, raw.jpegQuality, raw.longEdge, fps, &shot, &pathName)) {
            continue;
        }
        if (!raw.desktop) {
            shot.operable = raw.operable ? 1 : 0;
            if (pathName && pathName[0] == 'H') {
                pathName = "窗H264";
            } else if (pathName && std::strcmp(pathName, "JPEG") == 0) {
                pathName = "窗JPEG";
            } else if (pathName && std::strcmp(pathName, "原图") == 0) {
                pathName = "窗原图";
            }
        } else {
            shot.operable = 1;
            shot.visible = 1;
        }
        const uint64_t encodeMs = RemoteLog::tickMs() - e0;
        uint64_t thumbMs = 0;
        bool haveThumb = false;
        if (raw.subThumb && (m_lastThumbMs == 0 || raw.started - m_lastThumbMs >= 500)) {
            const uint64_t t0 = RemoteLog::tickMs();
            thumb = m_capture.fromBgra(raw.bgra, raw.width, raw.height, raw.width, raw.height, true,
                RemoteConst::kJpegQuality, 480);
            thumbMs = RemoteLog::tickMs() - t0;
            haveThumb = !thumb.jpeg.empty();
        }
        commitEncoded(shot, &thumb, haveThumb, 0, false, raw.originX, raw.originY, raw.desktop,
            raw.forcePublish, raw.shareGen, raw.subMain, raw.captureMs, raw.started, pathName,
            raw.grabMs, encodeMs, 0, thumbMs);
    }
    } catch (const std::exception& e) {
        RemoteCrash::alert("AgentSession.encodeMain", e.what(), true);
    } catch (...) {
        RemoteCrash::alert("AgentSession.encodeMain", "编码线程未知异常", true);
    }
}

void AgentSession::captureMain()
{
    try {
    while (true) {
        bool desktop = false;
        int monitor = 0;
        uint32_t shareGen = 0;
        int jpegQuality = RemoteConst::kJpegQuality;
        int longEdge = RemoteConst::kLongEdge;
        int captureMs = RemoteConst::kCaptureMs;
        bool subMain = true;
        bool subThumb = false;
        int pipelineWorkers = 0;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (!m_run) {
                return;
            }
            desktop = m_desktop;
            monitor = m_monitor;
            jpegQuality = m_jpegQuality;
            longEdge = m_longEdge;
            captureMs = m_captureMs;
            subMain = m_subMain;
            subThumb = m_subThumb;
            shareGen = m_shareGen;
            pipelineWorkers = m_pipelineWorkers;
        }
        // 主码流刚被会话打开，或共享源刚从进程切回桌面时，静止画面也要补一帧关键帧
        if (!subMain) {
            m_mainLatched = false;
        }
        const bool forcePublish = subMain && (shareGen != m_shareSeen || !m_mainLatched);
        if (forcePublish) {
            m_h264.requestKey();
        }
        // 抓屏在锁外做；整个桌面恒可操作，并记下显示器原点供输入换算
        const uint64_t started = nowMs();
        const uint64_t tick0 = RemoteLog::tickMs();
        PictureMsg shot;
        bool frameChanged = true;
        uint64_t grabMs = 0;
        uint64_t encodeMs = 0;
        uint64_t previewMs = 0;
        uint64_t thumbMs = 0;
        const char* pathName = "-";
        if (desktop) {
            int ox = 0;
            int oy = 0;
            CommBytes bgra;
            int bw = 0;
            int bh = 0;
            bool dxgiOk = false;
            if (monitor != 255) {
                const uint64_t g0 = RemoteLog::tickMs();
                dxgiOk = m_dxgi.grab(monitor, &bgra, &bw, &bh, &ox, &oy, &frameChanged);
                grabMs = RemoteLog::tickMs() - g0;
            } else {
                m_dxgi.reset();
            }
            const int fps = captureMs > 0 ? 1000 / captureMs : 15;
            if (dxgiOk && !frameChanged && !(forcePublish && !bgra.empty())) {
                shot.visible = 1;
                shot.operable = 1;
                pathName = "静帧";
            } else if (dxgiOk && frameChanged && m_capture.blank(bgra, bw, bh)) {
                const uint64_t e0 = RemoteLog::tickMs();
                shot = m_capture.grabDesktop(monitor, true, &ox, &oy, jpegQuality, longEdge);
                encodeMs = RemoteLog::tickMs() - e0;
                shot.codec = 1;
                pathName = "GDI空白";
                keepPreview(shot);
                frameChanged = !shot.jpeg.empty();
                if (!shot.jpeg.empty()) {
                    publishHttp(shot.jpeg);
                }
            } else if (dxgiOk && !bgra.empty()) {
                frameChanged = true;
                publishHttpBgra(bgra, bw, bh);
                {
                    const uint64_t p0 = RemoteLog::tickMs();
                    refreshLocalPreview(bgra, bw, bh, started);
                    previewMs = RemoteLog::tickMs() - p0;
                }
                if (pipelineWorkers > 0) {
                    pathName = "流水线";
                    publishRaw(bgra, bw, bh, ox, oy, true, true, forcePublish, shareGen,
                        jpegQuality, longEdge, captureMs, subMain, subThumb, started, grabMs);
                } else {
                    const uint64_t e0 = RemoteLog::tickMs();
                    if (!encodeByPrefer(bgra, bw, bh, jpegQuality, longEdge, fps, &shot, &pathName)) {
                        pathName = 0;
                    }
                    encodeMs = RemoteLog::tickMs() - e0;
                }
            } else {
                const uint64_t e0 = RemoteLog::tickMs();
                shot = m_capture.grabDesktop(monitor, true, &ox, &oy, jpegQuality, longEdge);
                encodeMs = RemoteLog::tickMs() - e0;
                shot.codec = 1;
                pathName = "GDI";
                keepPreview(shot);
                frameChanged = !shot.jpeg.empty();
                if (shot.codec == 1) {
                    publishHttp(shot.jpeg);
                }
            }
            const CommBytes shape = frameChanged || dxgiOk ? captureCursorShape() : CommBytes();
            if (!shape.empty()) {
                enqueue(shape);
            }
            POINT pt;
            const bool havePt = GetCursorPos(&pt) != FALSE;
            CursorPosMsg cursor;
            {
                std::lock_guard<std::mutex> lock(m_mu);
                const uint32_t clientW = shot.clientW > 0 ? shot.clientW : m_picture.clientW;
                const uint32_t clientH = shot.clientH > 0 ? shot.clientH : m_picture.clientH;
                const uint32_t imageW = shot.imageW > 0 ? shot.imageW : m_picture.imageW;
                const uint32_t imageH = shot.imageH > 0 ? shot.imageH : m_picture.imageH;
                if (havePt && clientW > 0 && imageW > 0 && clientH > 0 && imageH > 0) {
                    const int ix = (pt.x - ox) * static_cast<int>(imageW) / static_cast<int>(clientW);
                    const int iy = (pt.y - oy) * static_cast<int>(imageH) / static_cast<int>(clientH);
                    cursor.visible = (ix >= 0 && iy >= 0 && ix < static_cast<int>(imageW) && iy < static_cast<int>(imageH)) ? 1 : 0;
                    cursor.x = static_cast<uint16_t>(ix < 0 ? 0 : ix);
                    cursor.y = static_cast<uint16_t>(iy < 0 ? 0 : iy);
                }
                m_originX = ox;
                m_originY = oy;
                if (cursor.x != m_cursor.x || cursor.y != m_cursor.y || cursor.visible != m_cursor.visible) {
                    m_cursor = cursor;
                    m_cursorDirty = true;
                }
            }
            // 流水线模式主画面由编码线程提交；内联模式在这里提交
            if (pipelineWorkers <= 0 && frameChanged && pathName && std::strcmp(pathName, "静帧") != 0
                && std::strcmp(pathName, "流水线") != 0) {
                const bool wantThumb = subThumb && (m_lastThumbMs == 0 || started - m_lastThumbMs >= 500);
                PictureMsg thumb;
                const uint64_t t0 = RemoteLog::tickMs();
                if (wantThumb && !bgra.empty()) {
                    thumb = m_capture.fromBgra(bgra, bw, bh, bw, bh, true, RemoteConst::kJpegQuality, 480);
                } else if (wantThumb && !shot.jpeg.empty() && longEdge <= 480) {
                    thumb = shot;
                }
                thumbMs = RemoteLog::tickMs() - t0;
                commitEncoded(shot, &thumb, wantThumb && !thumb.jpeg.empty(), &cursor, true, ox, oy, true,
                    forcePublish, shareGen, subMain, captureMs, started, pathName, grabMs, encodeMs, previewMs, thumbMs);
            } else if (!frameChanged && pathName && std::strcmp(pathName, "静帧") == 0) {
                static uint64_t s_lastSkipLog = 0;
                const uint64_t nowTick = RemoteLog::tickMs();
                if (s_lastSkipLog == 0 || nowTick - s_lastSkipLog >= 2000) {
                    s_lastSkipLog = nowTick;
                    RemoteLog::writef(RemoteLog::Info, "画面",
                        "画面未变，跳过编码 · 抓%llums 间隔%dms",
                        static_cast<unsigned long long>(grabMs), captureMs);
                }
            }
        } else {
            // 进程画面：目标权限比本进程高时标记不可操作
            const HWND hwnd = m_watch.current();
            const bool can = m_input.operable(hwnd);
            CommBytes windowBgra;
            int ww = 0;
            int wh = 0;
            const int fps = captureMs > 0 ? 1000 / captureMs : 15;
            {
                const uint64_t g0 = RemoteLog::tickMs();
                m_capture.grabBgra(hwnd, &windowBgra, &ww, &wh);
                grabMs = RemoteLog::tickMs() - g0;
            }
            if (!windowBgra.empty()) {
                publishHttpBgra(windowBgra, ww, wh);
                const uint64_t p0 = RemoteLog::tickMs();
                refreshLocalPreview(windowBgra, ww, wh, started);
                previewMs = RemoteLog::tickMs() - p0;
            }
            if (pipelineWorkers > 0 && !windowBgra.empty()) {
                pathName = "流水线";
                publishRaw(windowBgra, ww, wh, 0, 0, false, can, forcePublish, shareGen,
                    jpegQuality, longEdge, captureMs, subMain, subThumb, started, grabMs);
            } else {
                const uint64_t e0 = RemoteLog::tickMs();
                bool encoded = false;
                if (!windowBgra.empty()) {
                    encoded = encodeByPrefer(windowBgra, ww, wh, jpegQuality, longEdge, fps, &shot, &pathName);
                    if (encoded) {
                        shot.operable = can ? 1 : 0;
                        if (pathName && pathName[0] == 'H') {
                            pathName = "窗H264";
                        } else if (pathName && std::strcmp(pathName, "JPEG") == 0) {
                            pathName = "窗JPEG";
                        } else if (pathName && std::strcmp(pathName, "原图") == 0) {
                            pathName = "窗原图";
                        }
                    }
                }
                if (!encoded) {
                    shot = m_capture.grab(hwnd, can, jpegQuality, longEdge);
                    shot.codec = 1;
                    keepPreview(shot);
                    publishHttp(shot.jpeg);
                    pathName = "窗JPEG";
                    encoded = !shot.jpeg.empty();
                }
                encodeMs = RemoteLog::tickMs() - e0;
                const CommBytes shape = captureCursorShape();
                if (!shape.empty()) {
                    enqueue(shape);
                }
                if (encoded) {
                    commitEncoded(shot, 0, false, 0, false, 0, 0, false, forcePublish, shareGen, subMain,
                        captureMs, started, pathName, grabMs, encodeMs, previewMs, 0);
                }
            }
            if (pipelineWorkers > 0) {
                const CommBytes shape = captureCursorShape();
                if (!shape.empty()) {
                    enqueue(shape);
                }
            }
        }
        if (captureMs < 1) {
            captureMs = RemoteConst::kCaptureMs;
        }
        // 帧率管整个周期：已经超时就不再 Sleep
        const uint64_t spentWall = nowMs() - started;
        if (spentWall < static_cast<uint64_t>(captureMs)) {
            Sleep(static_cast<DWORD>(captureMs - static_cast<int>(spentWall)));
        }
        (void)tick0;
        (void)encodeMs;
        (void)previewMs;
        (void)thumbMs;
    }
    } catch (const std::exception& e) {
        RemoteCrash::alert("AgentSession.captureMain", e.what(), true);
    } catch (...) {
        RemoteCrash::alert("AgentSession.captureMain", "抓屏线程未知异常", true);
    }
}

void AgentSession::readMain()
{
    try {
    while (true) {
        bool suspended = false;
        bool release = false;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            if (!m_run) {
                return;
            }
            suspended = m_suspended;
            // 发包线程写失败会直接关连接；重连前先结束旧会话，保证 m_connectedAt 只在新会话登记后才非 0
            if (m_connectedAt != 0 && !m_tcp.isOpen()) {
                endSession(!m_suspended);
                m_status = m_suspended ? "已断开" : "未连接";
            }
            release = m_releaseInput;
            m_releaseInput = false;
            // 现场确认超时：撤销申请并回 code 4。已持锁，直接入队不走 enqueue
            if (m_pendingActive && nowMs() > m_pendingDeadline) {
                m_pendingActive = false;
                std::string err;
                AuthResultMsg ack;
                ack.code = 4;
                ack.perms = PermView;
                const CommBytes bytes = Messages::packAuthResult(ack, &err);
                if (m_out.size() >= 64) {
                    m_out.erase(m_out.begin());
                }
                m_out.push_back(bytes);
            }
        }
        // 失去操作权（任何线程置的标志）后由本线程松开已注入的键鼠，m_input 只在本线程用
        if (release) {
            m_input.releaseAll();
        }
        drainViewerInputs();
        // 未连接：手动断开时空等；否则尝试反连，连上后重置会话并让发包线程先发 Hello，失败隔 1 秒再试
        if (!m_tcp.isOpen()) {
            if (suspended) {
                Sleep(200);
                continue;
            }
            std::string err;
            {
                std::lock_guard<std::mutex> lock(m_mu);
                if (!m_whitelist.empty()) {
                    bool allowed = false;
                    for (size_t w = 0; w < m_whitelist.size(); ++w) {
                        if (m_whitelist[w] == m_host || m_whitelist[w] == "*") {
                            allowed = true;
                            break;
                        }
                    }
                    if (!allowed) {
                        m_status = "白名单拒绝";
                        Sleep(2000);
                        continue;
                    }
                }
            }
            if (takeInbound()) {
                err.clear();
            } else if (!m_tcp.open(m_host, m_port, 2000, &err) && !openViaRelay(&err)) {
                Sleep(1000);
                continue;
            }
            RemoteCrash::checkpoint("AgentSession.tcp_open");
            std::string peerFp;
            if (m_identity.ready()) {
                if (!m_tcp.startTls(m_identity, &peerFp, &err)) {
                    RemoteCrash::log(("TLS fail: " + err).c_str());
                    m_tcp.close();
                    std::lock_guard<std::mutex> lock(m_mu);
                    m_status = "TLS 失败";
                    Sleep(1000);
                    continue;
                }
                RemoteCrash::checkpoint("AgentSession.tls_ok");
            } else {
                RemoteCrash::log("AgentSession: identity not ready, plain TCP");
            }
            std::lock_guard<std::mutex> lock(m_mu);
            m_peerFingerprint = peerFp;
            // 连接建立期间可能已 stop：stop 的 close 早于连接登记，这里自己关掉
            if (!m_run) {
                m_tcp.close();
                return;
            }
            // 期间现场点了断开：不开会话；有待发 Bye 时留给发包线程发出后关，否则直接关
            if (m_suspended) {
                if (!m_wantBye) {
                    m_tcp.close();
                }
                m_lastRx = nowMs();
                continue;
            }
            // 队列里是上一条会话的答复（AuthAck、Release、HeartbeatAck），对新会话无意义
            m_out.clear();
            m_needHello = true;
            m_operating = false;
            m_operated = false;
            m_pendingActive = false;
            m_controller.clear();
            m_lastRx = nowMs();
            m_connectedAt = nowMs();
            m_frame.clear();
            m_status = "只看";
            continue;
        }
        // 收一次（最多等 200 ms）。连接已关（read 出错、写失败或发完 Bye）时结束会话，手动断开不算对方结束
        std::string err;
        const CommBytes chunk = m_tcp.read("", 256 * 1024, 200, &err);
        if (!m_tcp.isOpen()) {
            std::lock_guard<std::mutex> lock(m_mu);
            endSession(!m_suspended);
            m_status = m_suspended ? "已断开" : "未连接";
            continue;
        }
        // 没收到数据：超过 kOfflineMs 没有心跳类报文就判断线
        if (chunk.empty()) {
            std::lock_guard<std::mutex> lock(m_mu);
            if (m_lastRx != 0 && nowMs() - m_lastRx > static_cast<uint64_t>(RemoteConst::kOfflineMs)) {
                m_tcp.close();
                endSession(true);
                m_status = "未连接";
            }
            continue;
        }
        // 拆帧；帧头非法说明流已错位，只能断开
        std::vector<Frame::Packet> packets;
        if (!m_frame.push(chunk, &packets, &err)) {
            m_tcp.close();
            std::lock_guard<std::mutex> lock(m_mu);
            endSession(true);
            continue;
        }
        // 逐帧按类型处理。HelloAck 与心跳类刷新 m_lastRx
        for (size_t i = 0; i < packets.size(); ++i) {
            const Frame::Packet& packet = packets[i];
            if (packet.channel == Frame::ChInput && packet.type == Frame::TypeSpecial) {
                SpecialMsg special;
                if (Messages::unpackSpecial(packet.payload, &special) && special.kind == SpecialMsg::KindCad) {
                    AgentService::requestCad();
                }
                continue;
            }
            if (packet.channel == Frame::ChInput
                && (packet.type == Frame::TypeMouse || packet.type == Frame::TypeKey)) {
                InputMsg input;
                if (!Messages::unpackInput(packet.payload, &input)) {
                    continue;
                }
                HWND hwnd = 0;
                PictureMsg picture;
                bool operating = false;
                bool desktop = false;
                int originX = 0;
                int originY = 0;
                {
                    std::lock_guard<std::mutex> lock(m_mu);
                    operating = m_operating && m_grantFp.empty();
                    picture = m_picture;
                    hwnd = m_watch.current();
                    desktop = m_desktop;
                    originX = m_originX;
                    originY = m_originY;
                }
                RECT screen;
                screen.left = originX;
                screen.top = originY;
                screen.right = originX + static_cast<int>(picture.clientW);
                screen.bottom = originY + static_cast<int>(picture.clientH);
                const bool allowed = desktop ? m_input.allowRemoteRect(screen) : m_input.allowRemote(hwnd);
                if (!operating || !picture.operable || !allowed) {
                    if (operating) {
                        m_input.releaseAll();
                    }
                    if (operating && !allowed) {
                        std::lock_guard<std::mutex> lock(m_mu);
                        m_status = "本地暂停";
                    }
                    continue;
                }
                if (desktop) {
                    m_input.postScreen(originX, originY, input, picture.clientW, picture.clientH, picture.imageW, picture.imageH);
                } else {
                    m_input.post(hwnd, input, picture.clientW, picture.clientH, picture.imageW, picture.imageH);
                }
                std::lock_guard<std::mutex> lock(m_mu);
                if (m_status == "本地暂停") {
                    m_status = "操作中";
                }
                continue;
            }
            if (takePhase4(packet)) {
                continue;
            }
            if (packet.channel != Frame::ChControl) {
                continue;
            }
            if (packet.type == Frame::TypeHelloAck) {
                HelloAckMsg ack;
                const bool ok = Messages::unpackHelloAck(packet.payload, &ack);
                std::lock_guard<std::mutex> lock(m_mu);
                m_lastRx = nowMs();
                if (ok) {
                    m_controller = ack.name;
                    if (!ack.fingerprint.empty()) {
                        m_peerFingerprint = ack.fingerprint;
                    }
                }
            } else if (packet.type == Frame::TypeHeartbeatAck) {
                std::lock_guard<std::mutex> lock(m_mu);
                m_lastRx = nowMs();
            } else if (packet.type == Frame::TypeHeartbeat) {
                HeartbeatMsg beat;
                if (!Messages::unpackHeartbeat(packet.payload, &beat)) {
                    continue;
                }
                HeartbeatAckMsg ack;
                ack.peerMs = beat.ms;
                ack.selfMs = nowMs();
                enqueue(Messages::packHeartbeatAck(ack, &err));
                std::lock_guard<std::mutex> lock(m_mu);
                m_lastRx = nowMs();
            } else if (packet.type == Frame::TypeAuthRequest) {
                AuthRequestMsg auth;
                AuthResultMsg ack;
                ack.code = 2;
                ack.perms = PermView;
                bool reply = true;
                if (Messages::unpackAuthRequest(packet.payload, &auth)) {
                    std::lock_guard<std::mutex> lock(m_mu);
                    uint32_t lockSec = 0;
                    if (m_auth.isLocked(&lockSec)) {
                        ack.code = 6;
                        ack.lockSec = lockSec;
                    } else if (m_operating || m_pendingActive) {
                        ack.code = 2;
                    } else {
                        bool authed = false;
                        if (auth.method == AuthMethodCode) {
                            authed = m_auth.checkCode(auth.token);
                        } else if (auth.method == AuthMethodPassword) {
                            authed = m_auth.checkPassword(auth.token);
                        } else if (auth.method == AuthMethodTrusted) {
                            authed = (!m_peerFingerprint.empty()
                                && std::find(m_trusted.begin(), m_trusted.end(), m_peerFingerprint) != m_trusted.end())
                                || (!m_controller.empty()
                                    && std::find(m_trusted.begin(), m_trusted.end(), m_controller) != m_trusted.end());
                            if (!authed && !m_auth.hasPassword() && m_confirm == ConfirmNever) {
                                authed = true;
                            }
                            if (!authed && auth.token.size() == 6) {
                                authed = m_auth.checkCode(auth.token);
                            }
                        }
                        if (!authed) {
                            uint32_t left = 0;
                            if (m_auth.onFail(&left)) {
                                ack.code = 6;
                                ack.lockSec = left;
                            } else {
                                ack.code = 5;
                                ack.triesLeft = m_auth.triesLeft();
                            }
                        } else if (m_stationBlock && (auth.perms & PermOperate) != 0) {
                            ack.code = 2;
                            ack.perms = PermView;
                        } else {
                            m_pendingPerms = (auth.perms | PermView) & m_allow;
                            if (needConfirm()) {
                                m_pendingActive = true;
                                m_pendingSeq += 1;
                                m_pendingDeadline = nowMs() + kConfirmMs;
                                reply = false;
                            } else {
                                ack.code = 0;
                                ack.perms = m_pendingPerms;
                                m_grantFp.clear();
                                m_operating = (ack.perms & PermOperate) != 0;
                                if (m_operating) {
                                    m_operated = true;
                                    m_operateAt = nowMs();
                                    m_status = "操作中";
                                }
                                m_auth.onSuccess();
                            }
                        }
                    }
                }
                if (reply) {
                    enqueue(Messages::packAuthResult(ack, &err));
                }
            } else if (packet.type == Frame::TypeConfig) {
                ConfigMsg cfg;
                if (!Messages::unpackConfig(packet.payload, &cfg)) {
                    continue;
                }
                applyControl(cfg);
            } else if (packet.type == Frame::TypeSubscribe) {
                SubscribeMsg sub;
                if (Messages::unpackSubscribe(packet.payload, &sub)) {
                    applySubscribe(sub);
                }
            } else if (packet.type == Frame::TypeNetStats) {
                NetStatsMsg stats;
                if (Messages::unpackNetStats(packet.payload, &stats)) {
                    applyNetStats(stats);
                }
            } else if (packet.type == Frame::TypeOperate) {
                OperateMsg op;
                if (!Messages::unpackOperate(packet.payload, &op)) {
                    continue;
                }
                if (op.action == Frame::OperateRelease) {
                    std::lock_guard<std::mutex> lock(m_mu);
                    m_operating = false;
                    m_grantFp.clear();
                    m_releaseInput = true;
                    m_status = "只看";
                } else if (op.action == Frame::OperateHandoff && !op.who.empty()) {
                    handoffTo(op.who);
                }
            } else if (packet.type == Frame::TypeBye) {
                m_tcp.close();
                std::lock_guard<std::mutex> lock(m_mu);
                endSession(true);
                m_status = "未连接";
            }
        }
    }
    } catch (const std::exception& e) {
        RemoteCrash::alert("AgentSession.readMain", e.what(), true);
    } catch (...) {
        RemoteCrash::alert("AgentSession.readMain", "收包线程未知异常", true);
    }
}

void AgentSession::writeMain()
{
    try {
    uint64_t nextBeat = 0;
    while (true) {
        bool run = false;
        bool needHello = false;
        bool wantBye = false;
        bool open = false;
        bool ready = false;
        PictureMsg picture;
        uint64_t seq = 0;
        uint64_t sent = 0;
        PictureMsg thumb;
        uint64_t thumbSeq = 0;
        uint64_t thumbSent = 0;
        CursorPosMsg cursor;
        bool cursorDirty = false;
        std::vector<CommBytes> batch;
        bool desktop = false;
        int monitor = 0;
        std::string source;
        std::string title;
        std::string process;
        bool forceRecord = false;
        // 锁内判断连接并取快照，网络发送都在锁外。没连上时 Bye、Hello 标志和待发队列原样留着：
        // Bye 等连上或 stop 时发；Hello 与队列由收包线程登记新会话时重置。
        // ready 要求收包线程已登记本次会话（m_connectedAt 与 m_needHello 同时置位），保证新连接上先发 Hello
        {
            std::lock_guard<std::mutex> lock(m_mu);
            run = m_run;
            open = m_tcp.isOpen();
            ready = open && m_connectedAt != 0;
            if (open) {
                wantBye = m_wantBye;
                m_wantBye = false;
            }
            if (ready && !wantBye) {
                needHello = m_needHello;
                m_needHello = false;
                seq = m_pictureSeq;
                sent = m_sentSeq;
                // 只有真有新帧才拷主画面/缩略图，避免空转时每圈复制几十 KB
                if (seq != sent) {
                    picture = m_picture;
                }
                thumbSeq = m_thumbSeq;
                thumbSent = m_thumbSent;
                if (thumbSeq != thumbSent) {
                    thumb = m_thumb;
                }
                cursor = m_cursor;
                cursorDirty = m_cursorDirty;
                m_cursorDirty = false;
                desktop = m_desktop;
                forceRecord = m_forceRecord;
                monitor = m_monitor;
                source = m_source;
                title = toUtf8(m_title);
                process = toUtf8(m_process);
                batch.swap(m_out);
            }
        }
        if (!run) {
            return;
        }
        if (!open) {
            Sleep(50);
            continue;
        }
        // 现场要求断开：发 Bye 后关连接，队列留给下次登记会话时清掉
        std::string err;
        if (wantBye) {
            m_tcp.write("", Messages::packEmpty(Frame::TypeBye, &err), 1000, &err);
            m_tcp.close();
            continue;
        }
        if (!ready) {
            Sleep(20);
            continue;
        }
        // 新连接或共享范围变了：先发 Hello 自报身份与共享源
        const uint64_t now = nowMs();
        if (needHello) {
            HelloMsg hello;
            hello.name = machineName();
            hello.source = source;
            hello.share = desktop ? 1 : 0;
            hello.visible = picture.visible;
            hello.fingerprint = fingerprint();
            hello.forceRecord = forceRecord ? 1 : 0;
            hello.monitor = static_cast<uint8_t>(monitor < 0 ? 0 : monitor);
            hello.title = title;
            hello.process = process;
            hello.deviceId = m_deviceId;
            hello.monitors = static_cast<uint8_t>(WindowCapture::monitorCount());
            hello.system = m_system;
            // 没发出去就恢复标志；取快照后共享范围又变时标志已被重新置位，不会被这里清掉
            if (!m_tcp.write("", Messages::packHello(hello, &err), 1000, &err)) {
                std::lock_guard<std::mutex> lock(m_mu);
                m_needHello = true;
                continue;
            }
        }
        // 每 kHeartbeatMs 一次心跳；画面有新帧才发，不重发旧帧；最后发队列里的答复类报文
        if (now >= nextBeat) {
            HeartbeatMsg beat;
            beat.ms = now;
            m_tcp.write("", Messages::packHeartbeat(beat, &err), 1000, &err);
            nextBeat = now + RemoteConst::kHeartbeatMs;
        }
        bool sentPicture = false;
        if (seq != sent) {
            const uint64_t t0 = RemoteLog::tickMs();
            const CommBytes packet = Messages::packPicture(picture, Frame::ChVideo, &err);
            const uint64_t packMs = RemoteLog::tickMs() - t0;
            const uint64_t t1 = RemoteLog::tickMs();
            const bool ok = m_tcp.write("", packet, 1000, &err);
            const uint64_t writeMs = RemoteLog::tickMs() - t1;
            int backlog = 0;
            {
                std::lock_guard<std::mutex> lock(m_mu);
                m_sentSeq = seq;
                backlog = m_pictureSeq > m_sentSeq ? static_cast<int>(m_pictureSeq - m_sentSeq) : 0;
            }
            RemoteLog::writef(ok ? RemoteLog::Info : RemoteLog::Warn, "发送",
                "主画面 seq=%llu 打包%llums 写出%llums %zuB codec=%u %ux%u 积压%d %s",
                static_cast<unsigned long long>(seq),
                static_cast<unsigned long long>(packMs),
                static_cast<unsigned long long>(writeMs),
                packet.size(),
                static_cast<unsigned>(picture.codec),
                picture.imageW,
                picture.imageH,
                backlog,
                ok ? "ok" : err.c_str());
            sentPicture = true;
        }
        if (thumbSeq != thumbSent && !thumb.jpeg.empty()) {
            m_tcp.write("", Messages::packPicture(thumb, Frame::ChThumb, &err), 1000, &err);
            std::lock_guard<std::mutex> lock(m_mu);
            m_thumbSent = thumbSeq;
        }
        if (cursorDirty) {
            m_tcp.write("", Messages::packCursorPos(cursor, &err), 1000, &err);
        }
        std::vector<CommBytes> phase4;
        pumpPhase4(&phase4);
        for (size_t n = 0; n < phase4.size(); ++n) {
            m_tcp.write("", phase4[n], 1000, &err);
        }
        for (size_t i = 0; i < batch.size(); ++i) {
            m_tcp.write("", batch[i], 1000, &err);
        }
        // 还有待发画面就立刻再转；空转才睡，避免人为把帧率卡死
        bool pending = false;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            pending = m_pictureSeq != m_sentSeq || !m_out.empty();
        }
        if (pending || sentPicture) {
            Sleep(0);
        } else {
            Sleep(10);
        }
    }
    } catch (const std::exception& e) {
        RemoteCrash::alert("AgentSession.writeMain", e.what(), true);
    } catch (...) {
        RemoteCrash::alert("AgentSession.writeMain", "发包线程未知异常", true);
    }
}

}
