/**
 * @file RemoteCrash.cpp
 * @brief 崩溃捕获、非致命报警、C++/SEH 守护
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteCrash.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#include <cstdlib>
#include <crtdbg.h>
#endif

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shell32.lib")

namespace Vision {

namespace {

std::mutex g_mu;
std::string g_role = "remote";
std::string g_logPath;
std::vector<std::string> g_checkpoints;
const size_t kMaxCheckpoints = 48;
bool g_installed = false;
volatile long g_inReport = 0;
DWORD g_mainTid = 0;

std::wstring widen(const char* s)
{
    if (!s || !*s) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, 0, 0);
    if (n <= 1) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], n);
    return out;
}

std::string narrow(const wchar_t* s)
{
    if (!s || !*s) {
        return std::string();
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, 0, 0, 0, 0);
    if (n <= 1) {
        return std::string();
    }
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, &out[0], n, 0, 0);
    return out;
}

std::string nowText()
{
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char buf[64];
    sprintf_s(buf, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

void appendFile(const std::string& line)
{
    if (g_logPath.empty()) {
        return;
    }
    const std::wstring path = widen(g_logPath.c_str());
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    const std::string row = line + "\r\n";
    DWORD written = 0;
    WriteFile(file, row.data(), static_cast<DWORD>(row.size()), &written, 0);
    CloseHandle(file);
}

std::string checkpointsText()
{
    std::string out;
    for (size_t i = 0; i < g_checkpoints.size(); ++i) {
        out += "  - ";
        out += g_checkpoints[i];
        out += "\r\n";
    }
    if (out.empty()) {
        out = "  (无)\r\n";
    }
    return out;
}

std::string moduleAt(void* addr, DWORD64* offsetOut)
{
    HMODULE mod = 0;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(addr), &mod) || !mod) {
        if (offsetOut) {
            *offsetOut = reinterpret_cast<DWORD64>(addr);
        }
        return "?";
    }
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(mod, path, MAX_PATH);
    const wchar_t* name = wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    if (offsetOut) {
        *offsetOut = reinterpret_cast<DWORD64>(addr) - reinterpret_cast<DWORD64>(mod);
    }
    return narrow(name);
}

std::string stackText(CONTEXT* ctx)
{
    if (!ctx) {
        return "  (无栈)\r\n";
    }
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, 0, TRUE);

    CONTEXT copy = *ctx;
    STACKFRAME64 frame = {};
    DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = copy.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = copy.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = copy.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    std::string out;
    for (int i = 0; i < 24; ++i) {
        if (!StackWalk64(machine, process, thread, &frame, &copy, 0, SymFunctionTableAccess64, SymGetModuleBase64, 0)) {
            break;
        }
        if (frame.AddrPC.Offset == 0) {
            break;
        }
        DWORD64 offset = 0;
        const std::string mod = moduleAt(reinterpret_cast<void*>(frame.AddrPC.Offset), &offset);
        char line[320];
        char nameBuf[sizeof(SYMBOL_INFO) + 256] = {};
        SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(nameBuf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        if (SymFromAddr(process, frame.AddrPC.Offset, &disp, sym)) {
            sprintf_s(line, "  #%02d %s!%s + 0x%llx\r\n", i, mod.c_str(), sym->Name,
                static_cast<unsigned long long>(disp));
        } else {
            sprintf_s(line, "  #%02d %s + 0x%llx\r\n", i, mod.c_str(),
                static_cast<unsigned long long>(offset));
        }
        out += line;
    }
    SymCleanup(process);
    if (out.empty()) {
        out = "  (无栈)\r\n";
    }
    return out;
}

void ensureLogPath()
{
    if (!g_logPath.empty()) {
        return;
    }
    wchar_t dir[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA, 0, 0, dir))) {
        GetTempPathW(MAX_PATH, dir);
    }
    std::wstring folder = dir;
    folder += L"\\Vision.Remote\\";
    CreateDirectoryW(folder.c_str(), 0);
    folder += widen(g_role.c_str());
    folder += L"-crash.log";
    g_logPath = narrow(folder.c_str());
}

void writeReport(const char* kind, const std::string& detail, CONTEXT* ctx, bool fatal)
{
    std::string body;
    body += nowText();
    body += fatal ? " [FATAL] " : " [ALERT] ";
    body += g_role;
    body += " ";
    body += kind ? kind : "?";
    body += "\r\n";
    body += detail;
    body += "checkpoints:\r\n";
    body += checkpointsText();
    if (ctx) {
        body += "stack:\r\n";
        body += stackText(ctx);
    }
    body += "log: ";
    body += g_logPath;
    body += "\r\n";

    {
        std::lock_guard<std::mutex> lock(g_mu);
        appendFile(body);
        appendFile("----");
    }

    std::wstring tip = widen((std::string(fatal ? "远程异常退出（" : "远程告警（")
        + g_role + "）\n\n" + detail
        + "\n最近检查点：\n" + checkpointsText()
        + "\n完整日志：\n" + g_logPath).c_str());
    MessageBoxW(0, tip.c_str(), L"Vision.Remote", fatal ? (MB_ICONERROR | MB_OK) : (MB_ICONWARNING | MB_OK));
}

void reportFatal(const char* kind, const std::string& detail, CONTEXT* ctx)
{
    if (InterlockedCompareExchange(&g_inReport, 1, 0) != 0) {
        return;
    }
    writeReport(kind, detail, ctx, true);
    InterlockedExchange(&g_inReport, 0);
}

LONG WINAPI onUnhandled(EXCEPTION_POINTERS* info)
{
    if (!info || !info->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    DWORD64 off = 0;
    const std::string mod = moduleAt(info->ExceptionRecord->ExceptionAddress, &off);
    char detail[512];
    sprintf_s(detail, "SEH 0x%08lX\n模块 %s\n偏移 0x%llX\n地址 %p\n",
        static_cast<unsigned long>(code), mod.c_str(),
        static_cast<unsigned long long>(off), info->ExceptionRecord->ExceptionAddress);
    reportFatal("SEH", detail, info->ContextRecord);
    return EXCEPTION_EXECUTE_HANDLER;
}

LONG CALLBACK onVectored(EXCEPTION_POINTERS* info)
{
    if (!info || !info->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    // 只记日志，不吞掉；真正收尾仍靠 UnhandledExceptionFilter，避免干扰正常 SEH
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_STACK_OVERFLOW
        || code == 0xC0000409ul /* STATUS_STACK_BUFFER_OVERRUN */) {
        DWORD64 off = 0;
        const std::string mod = moduleAt(info->ExceptionRecord->ExceptionAddress, &off);
        char line[256];
        sprintf_s(line, "VEH 0x%08lX %s+0x%llX", static_cast<unsigned long>(code),
            mod.c_str(), static_cast<unsigned long long>(off));
        std::lock_guard<std::mutex> lock(g_mu);
        ensureLogPath();
        appendFile(std::string("[VEH] ") + nowText() + " " + line);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#if defined(_MSC_VER)
/** @brief 崩溃路径禁止再抢 g_mu / 走堆栈游走，避免二次非法参数或死锁导致只闪退无日志 */
void emergencyLog(const char* text)
{
    if (!text) {
        return;
    }
    wchar_t dir[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA, 0, 0, dir))) {
        return;
    }
    std::wstring path = dir;
    path += L"\\Vision.Remote\\";
    CreateDirectoryW(path.c_str(), 0);
    path += widen(g_role.c_str());
    path += L"-crash.log";
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, 0);
    const char nl[] = "\r\n";
    WriteFile(file, nl, 2, &written, 0);
    FlushFileBuffers(file);
    CloseHandle(file);
}

void onInvalidParameter(const wchar_t* expression, const wchar_t* function, const wchar_t* file,
    unsigned int line, uintptr_t)
{
    char detail[1024];
    sprintf_s(detail,
        "[FATAL] CRT_INVALID_PARAM fn=%s file=%s line=%u expr=%s",
        function ? narrow(function).c_str() : "?",
        file ? narrow(file).c_str() : "?",
        line,
        expression ? narrow(expression).c_str() : "?");
    emergencyLog(detail);
    OutputDebugStringA(detail);
    // Debug 下 Qt/JPEG 偶发非法参数：只记日志，不 ExitProcess，避免被控一连中控台整进程闪退
#if defined(_DEBUG)
    return;
#else
    if (g_mainTid != 0 && GetCurrentThreadId() == g_mainTid) {
        MessageBoxA(0, detail, "RemoteMaster CRT", MB_ICONERROR | MB_OK | MB_TOPMOST);
    }
    ExitProcess(0xC0000409);
#endif
}

void onPureCall()
{
    emergencyLog("[FATAL] PURE_CALL");
    MessageBoxA(0, "纯虚函数调用", "Vision.Remote", MB_ICONERROR | MB_OK | MB_TOPMOST);
    ExitProcess(1);
}

void onAbortSignal(int)
{
    emergencyLog("[FATAL] ABORT");
    MessageBoxA(0, "abort() / 快速失败", "Vision.Remote", MB_ICONERROR | MB_OK | MB_TOPMOST);
    ExitProcess(1);
}
#endif

struct ProtectJob {
    void (*fn)(void*);
    void* arg;
};

int sehInvoke(ProtectJob* job)
{
    __try {
        job->fn(job->arg);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int>(GetExceptionCode());
    }
}

}

void RemoteCrash::install(const char* role)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (role && *role) {
        g_role = role;
    }
    ensureLogPath();
    if (g_installed) {
        return;
    }
    g_installed = true;
    g_mainTid = GetCurrentThreadId();
    // 压掉系统 GPFAULT 框，改由我们的过滤器弹窗说明
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(onUnhandled);
    AddVectoredExceptionHandler(1, onVectored);
#if defined(_MSC_VER)
    _set_invalid_parameter_handler(onInvalidParameter);
    _set_purecall_handler(onPureCall);
    signal(SIGABRT, onAbortSignal);
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif
    appendFile(std::string("==== ") + nowText() + " start " + g_role + " pid="
        + std::to_string(GetCurrentProcessId()) + " ====");
}

void RemoteCrash::checkpoint(const char* where)
{
    if (!where) {
        return;
    }
    const std::string row = nowText() + " " + where;
    std::lock_guard<std::mutex> lock(g_mu);
    ensureLogPath();
    g_checkpoints.push_back(row);
    if (g_checkpoints.size() > kMaxCheckpoints) {
        g_checkpoints.erase(g_checkpoints.begin());
    }
    appendFile(std::string("[CP] ") + row);
}

void RemoteCrash::log(const char* message)
{
    if (!message) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    ensureLogPath();
    appendFile(std::string("[LOG] ") + nowText() + " " + message);
}

void RemoteCrash::alert(const char* where, const char* message, bool popup)
{
    const char* w = where ? where : "?";
    const char* m = message ? message : "";
    {
        std::lock_guard<std::mutex> lock(g_mu);
        ensureLogPath();
        appendFile(std::string("[ALERT] ") + nowText() + " " + w + " | " + m);
    }
    if (!popup) {
        return;
    }
    // 工作线程弹 MessageBox 在 Debug/Qt 下易直接 fail-fast，只在主线程弹窗
    if (g_mainTid != 0 && GetCurrentThreadId() != g_mainTid) {
        OutputDebugStringA((std::string("[ALERT-bg] ") + w + " | " + m + "\n").c_str());
        return;
    }
    if (InterlockedCompareExchange(&g_inReport, 1, 0) != 0) {
        return;
    }
    std::wstring tip = widen((std::string("位置：") + w + "\n\n" + m
        + "\n\n日志：\n" + g_logPath).c_str());
    MessageBoxW(0, tip.c_str(), L"Vision.Remote 告警", MB_ICONWARNING | MB_OK);
    InterlockedExchange(&g_inReport, 0);
}

bool RemoteCrash::run(const char* where, const std::function<void()>& fn)
{
    if (!fn) {
        alert(where, "回调为空", false);
        return false;
    }
    try {
        fn();
        return true;
    } catch (const std::exception& e) {
        alert(where, e.what(), true);
        return false;
    } catch (...) {
        alert(where, "未知 C++ 异常", true);
        return false;
    }
}

bool RemoteCrash::protect(const char* where, void (*fn)(void*), void* arg)
{
    if (!fn) {
        alert(where, "SEH 回调为空", false);
        return false;
    }
    ProtectJob job = { fn, arg };
    const int code = sehInvoke(&job);
    if (code == 0) {
        return true;
    }
    char detail[128];
    sprintf_s(detail, "SEH 已拦截 0x%08X，操作已中止（进程继续）", static_cast<unsigned>(code));
    alert(where, detail, true);
    return false;
}

const char* RemoteCrash::logPath()
{
    std::lock_guard<std::mutex> lock(g_mu);
    ensureLogPath();
    return g_logPath.c_str();
}

}
