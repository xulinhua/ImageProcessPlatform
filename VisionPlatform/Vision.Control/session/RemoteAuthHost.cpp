/**
 * @file RemoteAuthHost.cpp
 * @brief 插件或本地账号文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteAuthHost.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <fstream>
#include <sstream>

namespace Vision {

RemoteAuthHost::RemoteAuthHost()
    : m_dll(0)
    , m_plugin(0)
    , m_destroy(0)
    , m_file(false)
    , m_perms(1)
{
}

RemoteAuthHost::~RemoteAuthHost()
{
    if (m_plugin && m_destroy) {
        m_destroy(m_plugin);
    }
    if (m_dll) {
        FreeLibrary(static_cast<HMODULE>(m_dll));
    }
}

void RemoteAuthHost::load()
{
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(0, path, MAX_PATH) == 0) {
        return;
    }
    std::wstring dir = path;
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        dir.resize(slash + 1);
    }
    const std::wstring dll = dir + L"RemoteAuth.dll";
    HMODULE mod = LoadLibraryW(dll.c_str());
    if (mod) {
        CreateRemoteAuthFn create = reinterpret_cast<CreateRemoteAuthFn>(GetProcAddress(mod, "CreateRemoteAuth"));
        m_destroy = reinterpret_cast<DestroyRemoteAuthFn>(GetProcAddress(mod, "DestroyRemoteAuth"));
        if (create) {
            m_plugin = create();
            m_dll = mod;
        } else {
            FreeLibrary(mod);
        }
    }
    const std::wstring file = dir + L"accounts.json";
    m_file = GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool RemoteAuthHost::loginRequired() const
{
    return m_plugin != 0 || m_file;
}

bool RemoteAuthHost::login(const std::string& user, const std::string& password)
{
    if (m_plugin) {
        RemoteAuthResult result;
        if (!m_plugin->login(user.c_str(), password.c_str(), &result)) {
            return false;
        }
        m_user = result.user[0] ? result.user : user;
        m_role = result.role;
        m_perms = result.perms == 0 ? 1 : result.perms;
        return true;
    }
    return loginFile(user, password);
}

bool RemoteAuthHost::loginFile(const std::string& user, const std::string& password)
{
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(0, path, MAX_PATH) == 0) {
        return false;
    }
    std::wstring file = path;
    const size_t slash = file.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        file.resize(slash + 1);
    }
    file += L"accounts.json";
    std::ifstream in(file);
    if (!in) {
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();
    const std::string needle = "\"user\":\"" + user + "\"";
    const size_t at = text.find(needle);
    if (at == std::string::npos) {
        return false;
    }
    const std::string pass = "\"password\":\"" + password + "\"";
    const size_t window = text.find(pass, at);
    if (window == std::string::npos || window > at + 240) {
        return false;
    }
    m_user = user;
    m_role = "operator";
    const size_t role = text.find("\"role\":\"", at);
    if (role != std::string::npos && role < at + 240) {
        const size_t begin = role + 8;
        const size_t end = text.find('"', begin);
        if (end != std::string::npos) {
            m_role = text.substr(begin, end - begin);
        }
    }
    m_perms = 1 | 2 | 4 | 8 | 16 | 32;
    if (m_role == "viewer") {
        m_perms = 1;
    }
    return true;
}

const std::string& RemoteAuthHost::user() const
{
    return m_user;
}

const std::string& RemoteAuthHost::role() const
{
    return m_role;
}

uint32_t RemoteAuthHost::perms() const
{
    return m_perms;
}

}
