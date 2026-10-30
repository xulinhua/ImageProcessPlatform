/**
 * @file AgentAudit.cpp
 * @brief 被控端 JSON Lines 审计
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentAudit.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

#include <stdio.h>

namespace Vision {

void AgentAudit::add(const char* event, const std::string& detail)
{
    if (!event || !event[0]) {
        return;
    }
    wchar_t roaming[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_APPDATA, 0, 0, roaming))) {
        return;
    }
    std::wstring dir = roaming;
    dir += L"\\RemoteClient\\audit";
    CreateDirectoryW((std::wstring(roaming) + L"\\RemoteClient").c_str(), 0);
    CreateDirectoryW(dir.c_str(), 0);
    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t name[64];
    swprintf_s(name, L"\\%04u-%02u-%02u.jsonl", now.wYear, now.wMonth, now.wDay);
    const std::wstring path = dir + name;
    std::string line = "{\"event\":\"";
    line += event;
    line += "\",\"detail\":\"";
    for (size_t i = 0; i < detail.size() && i < 400; ++i) {
        const char ch = detail[i];
        if (ch == '"' || ch == '\\') {
            line += ' ';
        } else if (ch == '\n' || ch == '\r') {
            line += ' ';
        } else {
            line += ch;
        }
    }
    char stamp[40];
    sprintf_s(stamp, "%04u-%02u-%02u %02u:%02u:%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    line += "\",\"time\":\"";
    line += stamp;
    line += "\"}\n";
    std::lock_guard<std::mutex> lock(m_mu);
    FILE* file = 0;
    if (_wfopen_s(&file, path.c_str(), L"ab") != 0 || !file) {
        return;
    }
    fwrite(line.data(), 1, line.size(), file);
    fclose(file);
}

}
