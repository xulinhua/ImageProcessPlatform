/**
 * @file ServiceCtl.cpp
 * @brief 服务控制管理器，未注册时退回同目录进程
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ServiceCtl.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

#pragma comment(lib, "advapi32.lib")

namespace Vision {

namespace {

std::wstring exeDir()
{
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return std::wstring();
    }
    std::wstring s(path, n);
    const size_t slash = s.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return std::wstring();
    }
    return s.substr(0, slash + 1);
}

void setErr(std::wstring* err, const wchar_t* text)
{
    if (err) {
        *err = text;
    }
}

bool processRunning(const std::wstring& exeFile)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exeFile.c_str()) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

void killProcess(const std::wstring& exeFile)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return;
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exeFile.c_str()) == 0 && pe.th32ProcessID != GetCurrentProcessId()) {
                HANDLE proc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
                if (proc) {
                    TerminateProcess(proc, 0);
                    WaitForSingleObject(proc, 3000);
                    CloseHandle(proc);
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

}

bool ServiceCtl::isRunning(const std::wstring& serviceName)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) {
        return processRunning(serviceName + L".exe");
    }
    SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        return processRunning(serviceName + L".exe");
    }
    SERVICE_STATUS st;
    const BOOL ok = QueryServiceStatus(svc, &st);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    if (ok && st.dwCurrentState == SERVICE_RUNNING) {
        return true;
    }
    return processRunning(serviceName + L".exe");
}

bool ServiceCtl::start(const std::wstring& serviceName, const std::wstring& exeFile, std::wstring* err)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
        if (svc) {
            const BOOL ok = StartServiceW(svc, 0, NULL);
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            if (ok || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) {
                return true;
            }
            setErr(err, L"启动服务失败");
            return false;
        }
        CloseServiceHandle(scm);
    }
    if (processRunning(exeFile)) {
        return true;
    }
    const std::wstring path = exeDir() + exeFile;
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    std::wstring cmd = L"\"" + path + L"\"";
    if (!CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, exeDir().c_str(), &si, &pi)) {
        setErr(err, L"拉起进程失败，请确认 exe 在本程序目录");
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool ServiceCtl::stop(const std::wstring& serviceName, const std::wstring& exeFile, std::wstring* err)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (svc) {
            SERVICE_STATUS st;
            const BOOL ok = ControlService(svc, SERVICE_CONTROL_STOP, &st);
            if (!ok && GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
                setErr(err, L"停止服务失败");
            }
            for (int i = 0; i < 30; ++i) {
                if (!QueryServiceStatus(svc, &st) || st.dwCurrentState == SERVICE_STOPPED) {
                    break;
                }
                Sleep(100);
            }
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }
    const std::wstring file = exeFile.empty() ? (serviceName + L".exe") : exeFile;
    killProcess(file);
    for (int i = 0; i < 20 && processRunning(file); ++i) {
        Sleep(100);
    }
    if (processRunning(file)) {
        killProcess(file);
    }
    return !processRunning(file);
}

bool ServiceCtl::install(const std::wstring& serviceName, const std::wstring& exeFile, std::wstring* err)
{
    const std::wstring path = exeDir() + exeFile;
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        setErr(err, L"没有注册服务的权限");
        return false;
    }
    SC_HANDLE svc = CreateServiceW(scm, serviceName.c_str(), serviceName.c_str(),
                                   SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                                   SERVICE_ERROR_NORMAL, path.c_str(), NULL, NULL, NULL, NULL, NULL);
    if (!svc && GetLastError() != ERROR_SERVICE_EXISTS) {
        CloseServiceHandle(scm);
        setErr(err, L"注册服务失败");
        return false;
    }
    if (svc) {
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return true;
}

bool ServiceCtl::uninstall(const std::wstring& serviceName, std::wstring* err)
{
    stop(serviceName, L"", 0);
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) {
        setErr(err, L"没有卸服务的权限");
        return false;
    }
    SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), DELETE);
    if (!svc) {
        CloseServiceHandle(scm);
        return true;
    }
    const BOOL ok = DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    if (!ok) {
        setErr(err, L"删除服务失败");
        return false;
    }
    return true;
}

}
