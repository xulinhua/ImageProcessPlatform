/**
 * @file StartupSweep.cpp
 * @brief 启动时结束同名进程，并放开这些进程留下的端口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <iphlpapi.h>

#include "StartupSweep.h"

#include <cstdio>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace Vision {

namespace {

const unsigned long kProcessCommandLine = 60;
const unsigned long kTcpDelete = 12;

struct NtUnicodeString {
    unsigned short length;
    unsigned short capacity;
    wchar_t* buffer;
};

typedef long (__stdcall* NtQueryProcess)(HANDLE, unsigned long, void*, unsigned long, unsigned long*);

}

std::wstring StartupSweep::ourExeName()
{
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
        return std::wstring();
    }
    const wchar_t* slash = wcsrchr(path, L'\\');
    return slash ? std::wstring(slash + 1) : std::wstring(path);
}

std::wstring StartupSweep::commandLineOf(unsigned long pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return std::wstring();
    }
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    NtQueryProcess query = ntdll
        ? reinterpret_cast<NtQueryProcess>(GetProcAddress(ntdll, "NtQueryInformationProcess"))
        : 0;
    std::wstring text;
    if (query) {
        unsigned char buf[4096];
        unsigned long got = 0;
        const long status = query(process, kProcessCommandLine, buf, sizeof(buf), &got);
        if (status >= 0) {
            const NtUnicodeString* us = reinterpret_cast<const NtUnicodeString*>(buf);
            if (us->buffer && us->length > 0) {
                text.assign(us->buffer, us->buffer + us->length / sizeof(wchar_t));
            }
        }
    }
    CloseHandle(process);
    return text;
}

bool StartupSweep::matchesRole(Role role, const std::wstring& cmd)
{
    if (cmd.empty()) {
        return false;
    }
    const bool service = cmd.find(L"--service") != std::wstring::npos;
    const bool agent = cmd.find(L"--agent") != std::wstring::npos;
    if (role == RoleAgent) {
        return agent && !service;
    }
    return !agent && !service;
}

bool StartupSweep::alive(unsigned long pid)
{
    if (pid == 0) {
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return false;
    }
    DWORD code = 0;
    const bool running = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return running;
}

bool StartupSweep::killPid(unsigned long pid)
{
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!process) {
        return false;
    }
    const BOOL ok = TerminateProcess(process, 1);
    const DWORD wait = ok ? WaitForSingleObject(process, 3000) : WAIT_FAILED;
    CloseHandle(process);
    return ok && wait == WAIT_OBJECT_0;
}

bool StartupSweep::sameName(const wchar_t* exeName)
{
    if (!exeName || !exeName[0]) {
        return false;
    }
    const std::wstring ours = ourExeName();
    return !ours.empty() && _wcsicmp(ours.c_str(), exeName) == 0;
}

std::wstring StartupSweep::exeNameOf(unsigned long pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return std::wstring();
    }
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    std::wstring name;
    if (QueryFullProcessImageNameW(process, 0, path, &size) && size > 0) {
        const wchar_t* slash = wcsrchr(path, L'\\');
        name = slash ? std::wstring(slash + 1) : std::wstring(path);
    }
    CloseHandle(process);
    return name;
}

void StartupSweep::addLine(Report* report, const std::string& line)
{
    if (!report || line.empty()) {
        return;
    }
    if (!report->text.empty()) {
        report->text.push_back('\n');
    }
    report->text.append(line);
}

int StartupSweep::releaseDeadTcp(int port)
{
    if (port <= 0 || port > 65535) {
        return 0;
    }
    DWORD size = 0;
    GetExtendedTcpTable(0, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) {
        return 0;
    }
    std::vector<unsigned char> buf(size);
    if (GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
        return 0;
    }
    const unsigned long count = *reinterpret_cast<unsigned long*>(buf.data());
    const unsigned char* row = buf.data() + sizeof(unsigned long);
    const unsigned long rowSize = 24;
    int closed = 0;
    const unsigned long self = GetCurrentProcessId();
    for (unsigned long i = 0; i < count; ++i) {
        const unsigned char* at = row + i * rowSize;
        const unsigned long localPort = ntohs(*reinterpret_cast<const unsigned short*>(at + 8));
        const unsigned long pid = *reinterpret_cast<const unsigned long*>(at + 20);
        if (static_cast<int>(localPort) != port || pid == 0 || pid == self || alive(pid)) {
            continue;
        }
        MIB_TCPROW item;
        item.dwState = kTcpDelete;
        item.dwLocalAddr = *reinterpret_cast<const unsigned long*>(at + 4);
        item.dwLocalPort = *reinterpret_cast<const unsigned long*>(at + 8);
        item.dwRemoteAddr = *reinterpret_cast<const unsigned long*>(at + 12);
        item.dwRemotePort = *reinterpret_cast<const unsigned long*>(at + 16);
        if (SetTcpEntry(&item) == NO_ERROR) {
            closed += 1;
        }
    }
    return closed;
}

StartupSweep::Report StartupSweep::run(Role role, const std::vector<int>& tcpPorts, const std::vector<int>& udpPorts)
{
    Report report;
    report.killed = 0;
    report.portsClosed = 0;
    report.portsLeft = 0;
    const unsigned long self = GetCurrentProcessId();
    const std::wstring exe = ourExeName();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snap, &entry)) {
            do {
                if (entry.th32ProcessID == self || !sameName(entry.szExeFile)) {
                    continue;
                }
                const std::wstring cmd = commandLineOf(entry.th32ProcessID);
                if (!matchesRole(role, cmd)) {
                    continue;
                }
                char line[160];
                if (killPid(entry.th32ProcessID)) {
                    report.killed += 1;
                    sprintf_s(line, "已结束同名进程 %ls（%lu）", exe.c_str(), entry.th32ProcessID);
                } else {
                    sprintf_s(line, "同名进程 %ls（%lu）没有结束", exe.c_str(), entry.th32ProcessID);
                }
                addLine(&report, line);
            } while (Process32NextW(snap, &entry));
        }
        CloseHandle(snap);
    }
    if (report.killed > 0) {
        Sleep(200);
    }

    for (size_t i = 0; i < tcpPorts.size(); ++i) {
        const int port = tcpPorts[i];
        if (port <= 0 || port > 65535) {
            continue;
        }
        DWORD size = 0;
        GetExtendedTcpTable(0, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        std::vector<unsigned char> buf(size ? size : 1);
        if (size == 0 || GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
            continue;
        }
        const unsigned long count = *reinterpret_cast<unsigned long*>(buf.data());
        const unsigned char* row = buf.data() + sizeof(unsigned long);
        unsigned long holder = 0;
        bool held = false;
        for (unsigned long n = 0; n < count; ++n) {
            const unsigned char* at = row + n * 24;
            const int localPort = ntohs(*reinterpret_cast<const unsigned short*>(at + 8));
            const unsigned long pid = *reinterpret_cast<const unsigned long*>(at + 20);
            if (localPort != port || pid == 0 || pid == self) {
                continue;
            }
            held = true;
            holder = pid;
            if (alive(pid)) {
                break;
            }
        }
        if (!held) {
            continue;
        }
        char line[192];
        if (!alive(holder)) {
            const int closed = releaseDeadTcp(port);
            if (closed > 0) {
                report.portsClosed += 1;
                sprintf_s(line, "已放开端口 %d", port);
            } else {
                report.portsLeft += 1;
                sprintf_s(line, "端口 %d 仍被已退出的进程占着", port);
            }
            addLine(&report, line);
            continue;
        }
        const std::wstring name = exeNameOf(holder);
        if (sameName(name.c_str()) && matchesRole(role, commandLineOf(holder)) && killPid(holder)) {
            report.killed += 1;
            report.portsClosed += 1;
            sprintf_s(line, "已结束占用端口 %d 的同名进程（%lu）", port, holder);
            addLine(&report, line);
            continue;
        }
        report.portsLeft += 1;
        if (name.empty()) {
            sprintf_s(line, "端口 %d 被进程 %lu 占用，没有结束它", port, holder);
        } else {
            sprintf_s(line, "端口 %d 被 %ls（%lu）占用，没有结束它", port, name.c_str(), holder);
        }
        addLine(&report, line);
    }

    for (size_t i = 0; i < udpPorts.size(); ++i) {
        const int port = udpPorts[i];
        if (port <= 0 || port > 65535) {
            continue;
        }
        DWORD size = 0;
        GetExtendedUdpTable(0, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
        std::vector<unsigned char> buf(size ? size : 1);
        if (size == 0 || GetExtendedUdpTable(buf.data(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) {
            continue;
        }
        const unsigned long count = *reinterpret_cast<unsigned long*>(buf.data());
        const unsigned char* row = buf.data() + sizeof(unsigned long);
        for (unsigned long n = 0; n < count; ++n) {
            const unsigned char* at = row + n * 12;
            const int localPort = ntohs(*reinterpret_cast<const unsigned short*>(at + 4));
            const unsigned long pid = *reinterpret_cast<const unsigned long*>(at + 8);
            if (localPort != port || pid == 0 || pid == self || !alive(pid)) {
                continue;
            }
            const std::wstring name = exeNameOf(pid);
            if (!sameName(name.c_str()) || !matchesRole(role, commandLineOf(pid))) {
                continue;
            }
            char line[160];
            if (killPid(pid)) {
                report.killed += 1;
                report.portsClosed += 1;
                sprintf_s(line, "已结束占用 UDP %d 的同名进程（%lu）", port, pid);
            } else {
                report.portsLeft += 1;
                sprintf_s(line, "UDP %d 被同名进程（%lu）占用，没有结束它", port, pid);
            }
            addLine(&report, line);
        }
    }
    return report;
}

} // namespace Vision
