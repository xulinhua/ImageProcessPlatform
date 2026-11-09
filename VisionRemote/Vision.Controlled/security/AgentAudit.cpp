/**
 * @file AgentAudit.cpp
 * @brief 被控端 JSON Lines 审计
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentAudit.h"
#include "RemotePaths.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdio.h>

namespace Vision {

void AgentAudit::add(const char* event, const std::string& detail)
{
    if (!event || !event[0]) {
        return;
    }
    RemotePaths::ensureLayout();
    std::wstring dir = RemotePaths::logDirW() + L"\\audit";
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
    line += "\"}\r\n";
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, 0);
    CloseHandle(file);
}

}
