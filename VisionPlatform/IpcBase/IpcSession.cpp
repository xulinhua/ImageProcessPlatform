/**
 * @file IpcSession.cpp
 * @brief 空 DACL 和 Global/Local 对象名
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "IpcSession.h"

namespace Ipc {

IpcSession::IpcSession()
{
    InitializeSecurityDescriptor(&m_sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&m_sd, TRUE, NULL, FALSE);
    m_sa.nLength = sizeof(m_sa);
    m_sa.lpSecurityDescriptor = &m_sd;
    m_sa.bInheritHandle = FALSE;
}

SECURITY_ATTRIBUTES* IpcSession::attributes()
{
    return &m_sa;
}

std::wstring IpcSession::objectName(bool global, const wchar_t* name) const
{
    std::wstring full = global ? L"Global\\" : L"Local\\";
    if (name) {
        full += name;
    }
    return full;
}

}
