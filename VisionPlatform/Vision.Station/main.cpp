/**
 * @file main.cpp
 * @brief Vision.Station.exe。由服务控制管理器启动，双击时在本进程里跑同一套循环
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ServiceCtl.h"
#include "StationService.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

SERVICE_STATUS g_status;
SERVICE_STATUS_HANDLE g_handle = 0;
Vision::StationService* g_service = 0;

void setState(DWORD state)
{
    g_status.dwCurrentState = state;
    g_status.dwControlsAccepted = state == SERVICE_START_PENDING ? 0 : SERVICE_ACCEPT_STOP;
    if (g_handle) {
        SetServiceStatus(g_handle, &g_status);
    }
}

void WINAPI onControl(DWORD code)
{
    if (code == SERVICE_CONTROL_STOP) {
        setState(SERVICE_STOP_PENDING);
        // 服务对象堵在命令等待上，进程退出时析构会停工站
        ExitProcess(0);
    }
}

void WINAPI serviceMain(DWORD, LPWSTR*)
{
    g_handle = RegisterServiceCtrlHandlerW(L"Vision.Station", onControl);
    ZeroMemory(&g_status, sizeof(g_status));
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    setState(SERVICE_START_PENDING);
    g_service = new Vision::StationService();
    setState(SERVICE_RUNNING);
    g_service->run();
    delete g_service;
    g_service = 0;
    setState(SERVICE_STOPPED);
}

bool argIs(const wchar_t* name)
{
    const wchar_t* cmd = GetCommandLineW();
    return cmd && wcsstr(cmd, name) != 0;
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    std::wstring err;
    if (argIs(L"--install")) {
        return Vision::ServiceCtl::install(L"Vision.Station", L"Vision.Station.exe", &err) ? 0 : 1;
    }
    if (argIs(L"--uninstall")) {
        return Vision::ServiceCtl::uninstall(L"Vision.Station", &err) ? 0 : 1;
    }
    SERVICE_TABLE_ENTRYW table[2];
    table[0].lpServiceName = const_cast<LPWSTR>(L"Vision.Station");
    table[0].lpServiceProc = serviceMain;
    table[1].lpServiceName = 0;
    table[1].lpServiceProc = 0;
    if (StartServiceCtrlDispatcherW(table)) {
        return 0;
    }
    if (GetLastError() != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
        return 1;
    }
    Vision::StationService service;
    return service.run();
}
