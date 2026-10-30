/**
 * @file SharedRegion.cpp
 * @brief 命名共享内存的创建和映射
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SharedRegion.h"
#include "IpcSession.h"

namespace Ipc {

SharedRegion::SharedRegion()
    : m_mapping(0)
    , m_view(0)
    , m_bytes(0)
    , m_created(false)
    , m_global(false)
{
}

SharedRegion::~SharedRegion()
{
    close();
}

bool SharedRegion::open(const wchar_t* name, uint32_t bytes, std::string* err)
{
    if (m_view) {
        return true;
    }
    if (!name || !name[0] || bytes == 0) {
        if (err) {
            *err = "共享内存参数无效";
        }
        return false;
    }
    IpcSession session;
    const std::wstring globalName = session.objectName(true, name);
    void* handle = CreateFileMappingW(INVALID_HANDLE_VALUE, session.attributes(), PAGE_READWRITE,
                                      0, bytes, globalName.c_str());
    bool global = true;
    if (!handle) {
        global = false;
        const std::wstring localName = session.objectName(false, name);
        handle = CreateFileMappingW(INVALID_HANDLE_VALUE, session.attributes(), PAGE_READWRITE,
                                    0, bytes, localName.c_str());
    }
    if (!handle) {
        if (err) {
            *err = "创建共享内存失败";
        }
        return false;
    }
    const bool created = GetLastError() != ERROR_ALREADY_EXISTS;
    void* view = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (!view) {
        CloseHandle(handle);
        if (err) {
            *err = "映射共享内存失败";
        }
        return false;
    }
    m_mapping = handle;
    m_view = view;
    m_bytes = bytes;
    m_created = created;
    m_global = global;
    return true;
}

void SharedRegion::close()
{
    if (m_view) {
        UnmapViewOfFile(m_view);
        m_view = 0;
    }
    if (m_mapping) {
        CloseHandle(m_mapping);
        m_mapping = 0;
    }
    m_bytes = 0;
    m_created = false;
    m_global = false;
}

bool SharedRegion::opened() const
{
    return m_view != 0;
}

bool SharedRegion::created() const
{
    return m_created;
}

bool SharedRegion::global() const
{
    return m_global;
}

void* SharedRegion::data() const
{
    return m_view;
}

uint32_t SharedRegion::size() const
{
    return m_bytes;
}

}
