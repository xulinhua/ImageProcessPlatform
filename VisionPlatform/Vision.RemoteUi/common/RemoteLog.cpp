/**
 * @file RemoteLog.cpp
 * @brief 运行日志环、落盘与毫秒计时
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteLog.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <cstring>

#pragma comment(lib, "shell32.lib")

namespace Vision {

RemoteLog& RemoteLog::self()
{
    static RemoteLog g;
    return g;
}

RemoteLog::RemoteLog()
    : m_nextSeq(1)
    , m_file(0)
    , m_freq(0)
    , m_bucketMs(0)
    , m_bucketCount(0)
    , m_lastFlushMs(0)
{
}

void RemoteLog::install(const char* role)
{
    RemoteLog& log = self();
    std::lock_guard<std::mutex> lock(log.m_mu);
    if (log.m_file) {
        return;
    }
    wchar_t root[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA, 0, SHGFP_TYPE_CURRENT, root))) {
        return;
    }
    std::wstring dir = std::wstring(root) + L"\\Vision.Remote";
    CreateDirectoryW(dir.c_str(), 0);
    const char* name = (role && *role) ? role : "remote";
    wchar_t wideRole[64];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wideRole, 64);
    std::wstring path = dir + L"\\" + wideRole + L"-run.log";
    // 二进制追加 + 共享读写：不能用 ccs=UTF-8（窄 fprintf 会 CRT 断言）；供外面边跑边读
    HANDLE handle = CreateFileW(path.c_str(),
        FILE_APPEND_DATA | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        0);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_APPEND | _O_BINARY);
    if (fd < 0) {
        CloseHandle(handle);
        return;
    }
    FILE* fp = _fdopen(fd, "ab");
    if (!fp) {
        _close(fd);
        return;
    }
    log.m_file = fp;
    const int n = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, 0, 0, 0, 0);
    if (n > 1) {
        log.m_path.assign(static_cast<size_t>(n - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &log.m_path[0], n, 0, 0);
    }
    std::fprintf(log.m_file, "==== run log open role=%s ====\r\n", name);
    std::fflush(log.m_file);
}

uint64_t RemoteLog::tickMs()
{
    return self().nowMs();
}

uint64_t RemoteLog::nowMs() const
{
    if (m_freq == 0) {
        LARGE_INTEGER freq = {};
        QueryPerformanceFrequency(&freq);
        m_freq = freq.QuadPart > 0 ? freq.QuadPart : 1;
    }
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart * 1000ull / m_freq);
}

void RemoteLog::stamp(char* buf, size_t cap) const
{
    if (!buf || cap < 13) {
        return;
    }
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    sprintf_s(buf, cap, "%02u:%02u:%02u.%03u",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

bool RemoteLog::throttleInfoLocked(const char* tag)
{
    if (!tag) {
        return false;
    }
    // 画面链路 Info 限流，避免 15 fps 刷爆窗口；Warn/Error 不限
    if (std::strcmp(tag, "画面") != 0
        && std::strcmp(tag, "发送") != 0
        && std::strcmp(tag, "接收") != 0
        && std::strcmp(tag, "解码") != 0) {
        return true;
    }
    const uint64_t now = nowMs();
    if (m_bucketMs == 0 || now - m_bucketMs >= 1000) {
        m_bucketMs = now;
        m_bucketCount = 0;
    }
    if (m_bucketCount >= 12) {
        return false;
    }
    ++m_bucketCount;
    return true;
}

void RemoteLog::pushLocked(Level level, const char* tag, const char* text)
{
    const size_t kCap = 2000;
    Line line;
    std::memset(&line, 0, sizeof(line));
    line.seq = m_nextSeq++;
    line.level = level;
    stamp(line.time, sizeof(line.time));
    if (tag) {
        sprintf_s(line.tag, "%s", tag);
    }
    if (text) {
        sprintf_s(line.text, "%s", text);
    }
    m_lines.push_back(line);
    while (m_lines.size() > kCap) {
        m_lines.pop_front();
    }
    if (m_file) {
        const char* lv = "I";
        if (level == Warn) {
            lv = "W";
        } else if (level == Error) {
            lv = "E";
        }
        std::fprintf(m_file, "%s [%s] [%s] %s\r\n", line.time, lv, line.tag, line.text);
        // 画面链路每行 fflush 会把收包/发包拖到百毫秒级；Warn/Error 立刻刷，Info 约 500ms 一次
        const uint64_t now = nowMs();
        const bool urgent = level != Info;
        if (urgent || m_lastFlushMs == 0 || now - m_lastFlushMs >= 500) {
            std::fflush(m_file);
            m_lastFlushMs = now;
        }
    }
}

void RemoteLog::writef(Level level, const char* tag, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vwritef(level, tag, fmt, args);
    va_end(args);
}

void RemoteLog::vwritef(Level level, const char* tag, const char* fmt, va_list args)
{
    if (!fmt) {
        return;
    }
    char text[768];
    vsnprintf_s(text, sizeof(text), _TRUNCATE, fmt, args);
    RemoteLog& log = self();
    std::lock_guard<std::mutex> lock(log.m_mu);
    if (level == Info && !log.throttleInfoLocked(tag)) {
        return;
    }
    log.pushLocked(level, tag, text);
}

size_t RemoteLog::pull(uint64_t* seq, Line* out, size_t cap)
{
    if (!seq || !out || cap == 0) {
        return 0;
    }
    RemoteLog& log = self();
    std::lock_guard<std::mutex> lock(log.m_mu);
    size_t n = 0;
    for (size_t i = 0; i < log.m_lines.size() && n < cap; ++i) {
        if (log.m_lines[i].seq <= *seq) {
            continue;
        }
        out[n++] = log.m_lines[i];
        *seq = log.m_lines[i].seq;
    }
    return n;
}

void RemoteLog::clear()
{
    RemoteLog& log = self();
    std::lock_guard<std::mutex> lock(log.m_mu);
    log.m_lines.clear();
}

const char* RemoteLog::filePath()
{
    RemoteLog& log = self();
    std::lock_guard<std::mutex> lock(log.m_mu);
    return log.m_path.c_str();
}

}
