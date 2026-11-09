/**
 * @file solution_procs.cpp
 * @brief 按写死的程序名判断进程是否要结束
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "solution_procs.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

namespace {

const wchar_t* kNames[] = {
    L"VisionPlatform.exe",
    L"Vision.Station.exe",
    L"Vision.Link.exe",
    L"Vision.PortDebug.exe",
    L"HsIpcDebug.exe",
    L"RemoteMaster.exe",
    L"RemoteClient.exe",
    L"RemoteRelay.exe",
    L"Vision.LicenseRequest.exe",
    L"Vision.LicenseSign.exe",
    L"Vision.ObfusTool.exe",
    L"VisionCoreTest.exe",
};

std::wstring baseName(const std::wstring& path)
{
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

}

SolutionProcs::SolutionProcs()
    : m_self(GetCurrentProcessId())
{
}

bool SolutionProcs::named(const std::wstring& fileName) const
{
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); ++i) {
        if (_wcsicmp(fileName.c_str(), kNames[i]) == 0) {
            return true;
        }
    }
    return false;
}

void SolutionProcs::refresh()
{
    m_list.clear();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return;
    }
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID == 0 || entry.th32ProcessID == m_self) {
                continue;
            }
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process) {
                continue;
            }
            wchar_t path[MAX_PATH] = {};
            DWORD bytes = MAX_PATH;
            const BOOL ok = QueryFullProcessImageNameW(process, 0, path, &bytes);
            CloseHandle(process);
            if (!ok || !named(baseName(path))) {
                continue;
            }
            SolutionProc proc;
            proc.pid = entry.th32ProcessID;
            proc.name = baseName(path);
            proc.path = path;
            m_list.push_back(proc);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
}

int SolutionProcs::kill(const std::vector<unsigned long>& pids, std::wstring* err) const
{
    int failed = 0;
    if (err) {
        err->clear();
    }
    for (size_t i = 0; i < pids.size(); ++i) {
        if (pids[i] == 0 || pids[i] == m_self) {
            continue;
        }
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pids[i]);
        if (!process || !TerminateProcess(process, 1)) {
            ++failed;
            if (err && err->empty()) {
                *err = L"有进程无法结束，可能权限不够或已经退出";
            }
        }
        if (process) {
            CloseHandle(process);
        }
    }
    return failed;
}
