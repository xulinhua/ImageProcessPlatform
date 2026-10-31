/**
 * @file RemoteLog.cpp
 * @brief 运行日志环、可选落盘与按间隔切分
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteLog.h"
#include "RemotePaths.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <cstring>

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
    , m_disk(false)
    , m_pictureDisk(false)
    , m_rotateMinutes(60)
    , m_keepDays(7)
{
}

void RemoteLog::install(const char* role)
{
    RemotePaths::ensureLayout();
    RemoteLog& log = self();
    std::lock_guard<std::mutex> lock(log.m_mu);
    log.m_role = (role && *role) ? role : "remote";
    log.m_dirUtf8 = RemotePaths::narrow(RemotePaths::logDirW());
    log.m_path = log.m_dirUtf8;
}

void RemoteLog::applyDisk(bool diskEnabled, bool pictureDisk, int rotateMinutes, int keepDays)
{
    RemotePaths::ensureLayout();
    RemoteLog& log = self();
    std::lock_guard<std::mutex> lock(log.m_mu);
    if (rotateMinutes < 15) {
        rotateMinutes = 15;
    }
    if (rotateMinutes > 24 * 60) {
        rotateMinutes = 24 * 60;
    }
    log.m_disk = diskEnabled;
    log.m_pictureDisk = pictureDisk;
    log.m_rotateMinutes = rotateMinutes;
    log.m_keepDays = keepDays;
    log.m_dirUtf8 = RemotePaths::narrow(RemotePaths::logDirW());
    if (!log.m_disk) {
        log.closeFileLocked();
        log.m_path = log.m_dirUtf8;
        return;
    }
    log.openBucketLocked();
    log.purgeOldLocked();
}

bool RemoteLog::diskEnabled()
{
    RemoteLog& log = self();
    std::lock_guard<std::mutex> lock(log.m_mu);
    return log.m_disk;
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

bool RemoteLog::isPictureTag(const char* tag) const
{
    return tag
        && (std::strcmp(tag, "画面") == 0
            || std::strcmp(tag, "发送") == 0
            || std::strcmp(tag, "接收") == 0
            || std::strcmp(tag, "解码") == 0);
}

bool RemoteLog::throttleInfoLocked(const char* tag)
{
    if (!isPictureTag(tag)) {
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

std::wstring RemoteLog::bucketNameLocked() const
{
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    const int mins = st.wHour * 60 + st.wMinute;
    const int slot = (mins / m_rotateMinutes) * m_rotateMinutes;
    const int hour = slot / 60;
    const int minute = slot % 60;
    wchar_t roleW[64] = {};
    MultiByteToWideChar(CP_UTF8, 0, m_role.c_str(), -1, roleW, 64);
    wchar_t name[160];
    swprintf_s(name, L"%s-run-%04u%02u%02u-%02u%02u.log",
        roleW, st.wYear, st.wMonth, st.wDay,
        static_cast<unsigned>(hour), static_cast<unsigned>(minute));
    return name;
}

void RemoteLog::closeFileLocked()
{
    if (m_file) {
        std::fflush(m_file);
        std::fclose(m_file);
        m_file = 0;
    }
    m_openName.clear();
}

void RemoteLog::openBucketLocked()
{
    const std::wstring name = bucketNameLocked();
    if (m_file && name == m_openName) {
        return;
    }
    closeFileLocked();
    const std::wstring path = RemotePaths::logDirW() + L"\\" + name;
    HANDLE handle = CreateFileW(path.c_str(),
        FILE_APPEND_DATA | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        0);
    if (handle == INVALID_HANDLE_VALUE) {
        m_path = m_dirUtf8;
        return;
    }
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_APPEND | _O_BINARY);
    if (fd < 0) {
        CloseHandle(handle);
        m_path = m_dirUtf8;
        return;
    }
    FILE* fp = _fdopen(fd, "ab");
    if (!fp) {
        _close(fd);
        m_path = m_dirUtf8;
        return;
    }
    m_file = fp;
    m_openName = name;
    m_path = RemotePaths::narrow(path);
    std::fprintf(m_file, "==== run log open role=%s ====\r\n", m_role.c_str());
    std::fflush(m_file);
    m_lastFlushMs = nowMs();
}

void RemoteLog::purgeOldLocked()
{
    if (m_keepDays <= 0 || m_role.empty()) {
        return;
    }
    const std::wstring dir = RemotePaths::logDirW();
    const std::wstring pattern = dir + L"\\" + RemotePaths::widen(m_role.c_str()) + L"-run-*.log";
    WIN32_FIND_DATAW find = {};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &find);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    FILETIME nowFt = {};
    SystemTimeToFileTime(&now, &nowFt);
    ULARGE_INTEGER now64;
    now64.LowPart = nowFt.dwLowDateTime;
    now64.HighPart = nowFt.dwHighDateTime;
    const ULONGLONG keep100ns = static_cast<ULONGLONG>(m_keepDays) * 24ull * 60ull * 60ull * 10000000ull;
    do {
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        ULARGE_INTEGER write64;
        write64.LowPart = find.ftLastWriteTime.dwLowDateTime;
        write64.HighPart = find.ftLastWriteTime.dwHighDateTime;
        if (now64.QuadPart > write64.QuadPart && now64.QuadPart - write64.QuadPart > keep100ns) {
            DeleteFileW((dir + L"\\" + find.cFileName).c_str());
        }
    } while (FindNextFileW(handle, &find));
    FindClose(handle);
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

    if (!m_disk) {
        return;
    }
    const bool pictureInfo = level == Info && isPictureTag(tag);
    if (pictureInfo && !m_pictureDisk) {
        return;
    }
    openBucketLocked();
    if (!m_file) {
        return;
    }
    const char* lv = "I";
    if (level == Warn) {
        lv = "W";
    } else if (level == Error) {
        lv = "E";
    }
    std::fprintf(m_file, "%s [%s] [%s] %s\r\n", line.time, lv, line.tag, line.text);
    const uint64_t now = nowMs();
    const bool urgent = level != Info;
    if (urgent || m_lastFlushMs == 0 || now - m_lastFlushMs >= 500) {
        std::fflush(m_file);
        m_lastFlushMs = now;
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
