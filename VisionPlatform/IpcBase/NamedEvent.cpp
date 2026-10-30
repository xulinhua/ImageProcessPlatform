/**
 * @file NamedEvent.cpp
 * @brief 命名自动复位事件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "NamedEvent.h"
#include "IpcSession.h"

namespace Ipc {

NamedEvent::NamedEvent()
    : m_handle(0)
{
}

NamedEvent::~NamedEvent()
{
    close();
}

bool NamedEvent::open(bool global, const wchar_t* name)
{
    if (m_handle) {
        return true;
    }
    if (!name || !name[0]) {
        return false;
    }
    IpcSession session;
    const std::wstring full = session.objectName(global, name);
    m_handle = CreateEventW(session.attributes(), FALSE, FALSE, full.c_str());
    return m_handle != 0;
}

void NamedEvent::close()
{
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = 0;
    }
}

bool NamedEvent::opened() const
{
    return m_handle != 0;
}

void NamedEvent::signal()
{
    if (m_handle) {
        SetEvent(m_handle);
    }
}

bool NamedEvent::wait(int timeoutMs)
{
    if (!m_handle) {
        return false;
    }
    const DWORD ms = timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs);
    return WaitForSingleObject(m_handle, ms) == WAIT_OBJECT_0;
}

}
