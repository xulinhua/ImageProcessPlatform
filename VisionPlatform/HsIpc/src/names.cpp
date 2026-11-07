/**
 * @file names.cpp
 * @brief 空 DACL 和内核对象
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 共享内存先试 Global\。普通用户会得到拒绝，这时改用 Local\，
 * 同一 Kernel 上随后创建的互斥量和事件不再改回 Global\。
 */
#include "names.h"

namespace HsIpc {

Security::Security()
{
    InitializeSecurityDescriptor(&m_sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&m_sd, TRUE, NULL, FALSE);
    m_sa.nLength = sizeof(m_sa);
    m_sa.lpSecurityDescriptor = &m_sd;
    m_sa.bInheritHandle = FALSE;
}

SECURITY_ATTRIBUTES* Security::attr()
{
    return &m_sa;
}

bool validChannelName(const char* name)
{
    if (!name || !name[0] || !name[1]) {
        return false;
    }
    for (const char* p = name; *p; ++p) {
        const unsigned char c = (unsigned char)*p;
        const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || c == '.' || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::wstring widen(const char* utf8)
{
    if (!utf8 || !utf8[0]) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
    if (n <= 0) {
        return std::wstring();
    }
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], n);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

std::wstring objectName(bool global, const std::wstring& shortName)
{
    return (global ? L"Global\\" : L"Local\\") + shortName;
}

Kernel::Kernel()
    : m_mapping(0)
    , m_view(0)
    , m_mutex(0)
    , m_event(0)
    , m_global(true)
    , m_namespaceChosen(false)
    , m_lastError(0)
{
}

Kernel::~Kernel()
{
    close();
}

void Kernel::close()
{
    if (m_view) {
        UnmapViewOfFile(m_view);
        m_view = 0;
    }
    if (m_mapping) {
        CloseHandle(m_mapping);
        m_mapping = 0;
    }
    if (m_mutex) {
        CloseHandle(m_mutex);
        m_mutex = 0;
    }
    if (m_event) {
        CloseHandle(m_event);
        m_event = 0;
    }
}

HANDLE Kernel::create(const std::wstring& shortName, bool eventWanted, uint64_t bytes, bool* created)
{
    if (created) {
        *created = false;
    }
    const bool tryGlobal = !m_namespaceChosen || m_global;
    if (tryGlobal) {
        const std::wstring full = objectName(true, shortName);
        HANDLE handle = eventWanted
            ? CreateEventW(m_security.attr(), FALSE, FALSE, full.c_str())
            : (bytes == 0
                ? CreateMutexW(m_security.attr(), FALSE, full.c_str())
                : CreateFileMappingW(INVALID_HANDLE_VALUE, m_security.attr(), PAGE_READWRITE,
                    (DWORD)(bytes >> 32), (DWORD)bytes, full.c_str()));
        if (handle) {
            m_global = true;
            m_namespaceChosen = true;
            if (created) {
                *created = GetLastError() != ERROR_ALREADY_EXISTS;
            }
            return handle;
        }
        if (m_namespaceChosen && m_global) {
            m_lastError = GetLastError();
            return 0;
        }
    }
    const std::wstring local = objectName(false, shortName);
    HANDLE handle = eventWanted
        ? CreateEventW(m_security.attr(), FALSE, FALSE, local.c_str())
        : (bytes == 0
            ? CreateMutexW(m_security.attr(), FALSE, local.c_str())
            : CreateFileMappingW(INVALID_HANDLE_VALUE, m_security.attr(), PAGE_READWRITE,
                (DWORD)(bytes >> 32), (DWORD)bytes, local.c_str()));
    if (handle) {
        m_global = false;
        m_namespaceChosen = true;
        if (created) {
            *created = GetLastError() != ERROR_ALREADY_EXISTS;
        }
    }
    if (!handle) {
        m_lastError = GetLastError();
    }
    return handle;
}

void Kernel::useNamespace(bool global)
{
    m_global = global;
    m_namespaceChosen = true;
}

bool Kernel::openEvent(const std::wstring& shortName, std::string* err)
{
    m_event = create(shortName, true, 0, 0);
    if (!m_event && err) {
        *err = "创建命名事件失败";
    }
    return m_event != 0;
}

bool Kernel::openMutex(const std::wstring& shortName, std::string* err)
{
    m_mutex = create(shortName, false, 0, 0);
    if (!m_mutex && err) {
        *err = "创建互斥量失败";
    }
    return m_mutex != 0;
}

bool Kernel::openMapping(const std::wstring& shortName, uint64_t bytes, bool* created, std::string* err)
{
    if (bytes == 0 || bytes > 0xFFFFFFFFull) {
        if (err) {
            *err = "共享内存大小无效";
        }
        return false;
    }
    m_mapping = create(shortName, false, bytes, created);
    if (!m_mapping) {
        if (err) {
            *err = "创建共享内存失败";
        }
        return false;
    }
    m_view = MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, (SIZE_T)bytes);
    if (!m_view) {
        CloseHandle(m_mapping);
        m_mapping = 0;
        if (err) {
            *err = "映射共享内存失败";
        }
        return false;
    }
    return true;
}

bool Kernel::lock(int timeoutMs)
{
    if (!m_mutex) {
        return false;
    }
    const DWORD wait = WaitForSingleObject(m_mutex, timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs);
    return wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
}

void Kernel::unlock()
{
    if (m_mutex) {
        ReleaseMutex(m_mutex);
    }
}

void Kernel::signal()
{
    if (m_event) {
        SetEvent(m_event);
    }
}

int Kernel::waitEvent(int timeoutMs)
{
    if (!m_event) {
        return -3;
    }
    const DWORD wait = WaitForSingleObject(m_event, timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs);
    if (wait == WAIT_OBJECT_0) {
        return 0;
    }
    if (wait == WAIT_TIMEOUT) {
        return 1;
    }
    return -3;
}

}
