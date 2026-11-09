/**
 * @file AgentService.cpp
 * @brief 被控端服务：SessionWatch + 拉起代理 + CAD 管道
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentService.h"
#include "SasSender.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>

#include <string>
#include <vector>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")

namespace Vision {

const wchar_t* AgentService::kName = L"RemoteClient";
const wchar_t* AgentService::kDisplay = L"RemoteClient 被控端";
const wchar_t* AgentService::kPipe = L"\\\\.\\pipe\\RemoteClient.Cad";

namespace {

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status = {};
HANDLE g_stopEvent = nullptr;
HANDLE g_agentProcess = nullptr;

void report(DWORD state, DWORD win32Exit = NO_ERROR, DWORD waitHint = 0)
{
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = win32Exit;
    g_status.dwWaitHint = waitHint;
    g_status.dwControlsAccepted = (state == SERVICE_RUNNING) ? (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE) : 0;
    if (g_statusHandle) {
        SetServiceStatus(g_statusHandle, &g_status);
    }
}

std::wstring exePath()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

bool launchAgentInSession(DWORD sessionId)
{
    if (g_agentProcess) {
        DWORD code = 0;
        if (GetExitCodeProcess(g_agentProcess, &code) && code == STILL_ACTIVE) {
            return true;
        }
        CloseHandle(g_agentProcess);
        g_agentProcess = nullptr;
    }
    HANDLE primary = nullptr;
    HANDLE userToken = nullptr;
    if (WTSQueryUserToken(sessionId, &userToken)) {
        DuplicateTokenEx(userToken, MAXIMUM_ALLOWED, nullptr, SecurityIdentification, TokenPrimary, &primary);
        CloseHandle(userToken);
    }
    // 登录界面 / 未登录：没有用户令牌时，把当前进程（LocalSystem）令牌绑到活动会话再拉起代理
    if (!primary) {
        HANDLE self = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY, &self)) {
            return false;
        }
        if (!DuplicateTokenEx(self, MAXIMUM_ALLOWED, nullptr, SecurityIdentification, TokenPrimary, &primary)) {
            CloseHandle(self);
            return false;
        }
        CloseHandle(self);
        if (!SetTokenInformation(primary, TokenSessionId, &sessionId, sizeof(sessionId))) {
            CloseHandle(primary);
            return false;
        }
    }

    wchar_t cmd[1024];
    swprintf_s(cmd, L"\"%s\" --agent", exePath().c_str());
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");
    PROCESS_INFORMATION pi = {};
    void* env = nullptr;
    CreateEnvironmentBlock(&env, primary, FALSE);
    const BOOL ok = CreateProcessAsUserW(primary, nullptr, cmd, nullptr, nullptr, FALSE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE, env, nullptr, &si, &pi);
    if (env) {
        DestroyEnvironmentBlock(env);
    }
    CloseHandle(primary);
    if (!ok) {
        return false;
    }
    CloseHandle(pi.hThread);
    g_agentProcess = pi.hProcess;
    return true;
}

DWORD activeSession()
{
    const DWORD id = WTSGetActiveConsoleSessionId();
    return id == 0xFFFFFFFF ? 0 : id;
}

void stopAgent()
{
    if (!g_agentProcess) {
        return;
    }
    TerminateProcess(g_agentProcess, 0);
    WaitForSingleObject(g_agentProcess, 3000);
    CloseHandle(g_agentProcess);
    g_agentProcess = nullptr;
}

DWORD WINAPI pipeThread(LPVOID)
{
    while (WaitForSingleObject(g_stopEvent, 0) != WAIT_OBJECT_0) {
        HANDLE pipe = CreateNamedPipeW(AgentService::kPipe, PIPE_ACCESS_INBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 256, 256, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(500);
            continue;
        }
        const BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            char buf[32] = {};
            DWORD read = 0;
            if (ReadFile(pipe, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
                if (strncmp(buf, "CAD", 3) == 0) {
                    SasSender::send();
                }
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
    return 0;
}

DWORD WINAPI serviceCtrl(DWORD ctrl, DWORD eventType, LPVOID, LPVOID)
{
    if (ctrl == SERVICE_CONTROL_STOP) {
        report(SERVICE_STOP_PENDING, NO_ERROR, 3000);
        SetEvent(g_stopEvent);
        return NO_ERROR;
    }
    if (ctrl == SERVICE_CONTROL_INTERROGATE) {
        return NO_ERROR;
    }
    if (ctrl == SERVICE_CONTROL_SESSIONCHANGE) {
        if (eventType == WTS_CONSOLE_CONNECT || eventType == WTS_SESSION_LOGON
            || eventType == WTS_SESSION_UNLOCK) {
            launchAgentInSession(activeSession());
        } else if (eventType == WTS_CONSOLE_DISCONNECT || eventType == WTS_SESSION_LOGOFF) {
            stopAgent();
        }
        return NO_ERROR;
    }
    return ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI serviceMain(DWORD, LPWSTR*)
{
    g_statusHandle = RegisterServiceCtrlHandlerExW(AgentService::kName, serviceCtrl, nullptr);
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    report(SERVICE_START_PENDING, NO_ERROR, 3000);
    HANDLE pipe = CreateThread(nullptr, 0, pipeThread, nullptr, 0, nullptr);
    launchAgentInSession(activeSession());
    report(SERVICE_RUNNING);
    WaitForSingleObject(g_stopEvent, INFINITE);
    stopAgent();
    if (pipe) {
        // 打断阻塞中的 ConnectNamedPipe：再建一个客户端连一下
        HANDLE wake = CreateFileW(AgentService::kPipe, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (wake != INVALID_HANDLE_VALUE) {
            CloseHandle(wake);
        }
        WaitForSingleObject(pipe, 2000);
        CloseHandle(pipe);
    }
    report(SERVICE_STOPPED);
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
}

}

bool AgentService::install(std::string* err)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        if (err) {
            *err = "无法打开服务管理器（需要管理员）";
        }
        return false;
    }
    const std::wstring path = L"\"" + exePath() + L"\" --service";
    SC_HANDLE svc = CreateServiceW(scm, kName, kDisplay, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!svc) {
        const DWORD e = GetLastError();
        CloseServiceHandle(scm);
        if (e == ERROR_SERVICE_EXISTS) {
            return true;
        }
        if (err) {
            *err = "创建服务失败";
        }
        return false;
    }
    // 描述
    SERVICE_DESCRIPTIONW desc = {};
    desc.lpDescription = const_cast<LPWSTR>(L"RemoteClient 被控端服务：守护会话代理并发送 Ctrl+Alt+Del。");
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);
    StartServiceW(svc, 0, nullptr);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return true;
}

bool AgentService::uninstall(std::string* err)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        if (err) {
            *err = "无法打开服务管理器";
        }
        return false;
    }
    SC_HANDLE svc = OpenServiceW(scm, kName, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        return true;
    }
    SERVICE_STATUS st = {};
    ControlService(svc, SERVICE_CONTROL_STOP, &st);
    DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return true;
}

int AgentService::run()
{
    SERVICE_TABLE_ENTRYW table[] = {
        { const_cast<LPWSTR>(kName), serviceMain },
        { nullptr, nullptr }
    };
    if (!StartServiceCtrlDispatcherW(table)) {
        return static_cast<int>(GetLastError());
    }
    return 0;
}

bool AgentService::requestCad()
{
    HANDLE pipe = CreateFileW(kPipe, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        // 无服务时本进程直接发
        return SasSender::send();
    }
    DWORD written = 0;
    const char* msg = "CAD";
    const BOOL ok = WriteFile(pipe, msg, 3, &written, nullptr);
    CloseHandle(pipe);
    return ok == TRUE;
}

}
