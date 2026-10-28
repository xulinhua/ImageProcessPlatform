/**
 * @file CrashDump.cpp
 * @brief 未处理异常时在 exe 旁 Dump 目录写 minidump
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 对齐 HAD8601 VMInterface/minidmp.cpp：Dump\ERLOG_时间.dmp，MiniDumpNormal。
 * 路径取自本进程模块目录，不写绝对盘符。
 */

#include "CrashDump.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>

#pragma comment(lib, "dbghelp.lib")

namespace Vision {

namespace {

// 在 exe 同级 Dump 目录生成 ERLOG_年-月-日-时-分-秒-毫秒.dmp
void writeMiniDump(EXCEPTION_POINTERS* info)
{
    wchar_t exePath[MAX_PATH];
    const DWORD n = GetModuleFileNameW(NULL, exePath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return;
    }
    wchar_t* slash = exePath + n;
    while (slash > exePath && *slash != L'\\' && *slash != L'/') {
        --slash;
    }
    if (*slash != L'\\' && *slash != L'/') {
        return;
    }
    *(slash + 1) = 0;

    wchar_t dir[MAX_PATH];
    if (swprintf_s(dir, L"%sDump\\", exePath) < 0) {
        return;
    }
    CreateDirectoryW(dir, NULL);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t path[MAX_PATH];
    if (swprintf_s(path, L"%sERLOG_%04d-%02d-%02d-%02d-%02d-%02d-%03d.dmp",
                   dir, st.wYear, st.wMonth, st.wDay,
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds) < 0) {
        return;
    }

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION ex;
    ex.ThreadId = GetCurrentThreadId();
    ex.ExceptionPointers = info;
    ex.ClientPointers = FALSE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal,
                      info ? &ex : NULL, NULL, NULL);
    CloseHandle(file);
}

LONG WINAPI onUnhandled(EXCEPTION_POINTERS* info)
{
    writeMiniDump(info);
    FatalAppExitW(0, L"系统错误，需要重新启动!");
    return EXCEPTION_EXECUTE_HANDLER;
}

}

void CrashDump::install()
{
    SetUnhandledExceptionFilter(onUnhandled);
}

}
