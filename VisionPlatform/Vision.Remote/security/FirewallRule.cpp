/**
 * @file FirewallRule.cpp
 * @brief netsh 入站规则
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FirewallRule.h"

#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Vision {

bool FirewallRule::allowTcp(const std::wstring& name, const std::wstring& exe, int port, std::string* err) const
{
    if (name.empty() || exe.empty() || port < 1 || port > 65535) {
        if (err) {
            *err = "防火墙参数无效";
        }
        return false;
    }
    std::wstring del = L"netsh advfirewall firewall delete rule name=\"" + name + L"\"";
    std::wstring add = L"netsh advfirewall firewall add rule name=\"" + name
        + L"\" dir=in action=allow program=\"" + exe + L"\" protocol=TCP localport="
        + std::to_wstring(port) + L" profile=any";
    STARTUPINFOW delInfo;
    PROCESS_INFORMATION delProc;
    std::memset(&delInfo, 0, sizeof(delInfo));
    std::memset(&delProc, 0, sizeof(delProc));
    delInfo.cb = sizeof(delInfo);
    std::wstring delCmd = del;
    if (CreateProcessW(0, &delCmd[0], 0, 0, FALSE, CREATE_NO_WINDOW, 0, 0, &delInfo, &delProc)) {
        WaitForSingleObject(delProc.hProcess, 5000);
        CloseHandle(delProc.hProcess);
        CloseHandle(delProc.hThread);
    }
    STARTUPINFOW info;
    PROCESS_INFORMATION proc;
    std::memset(&info, 0, sizeof(info));
    std::memset(&proc, 0, sizeof(proc));
    info.cb = sizeof(info);
    std::wstring cmd = add;
    if (!CreateProcessW(0, &cmd[0], 0, 0, FALSE, CREATE_NO_WINDOW, 0, 0, &info, &proc)) {
        if (err) {
            *err = "无法启动 netsh";
        }
        return false;
    }
    WaitForSingleObject(proc.hProcess, 8000);
    DWORD code = 1;
    GetExitCodeProcess(proc.hProcess, &code);
    CloseHandle(proc.hProcess);
    CloseHandle(proc.hThread);
    if (code != 0) {
        if (err) {
            *err = "添加防火墙规则失败，请用管理员运行";
        }
        return false;
    }
    return true;
}

}
