/**
 * @file ConsoleHost.cpp
 * @brief 控制台命令只写共享内存，不在这里跑工站
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ConsoleHost.h"
#include "SharedFrames.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <string>
#include <thread>

ConsoleHost::ConsoleHost()
    : m_thread(0)
    , m_run(false)
    , m_localStart(0)
    , m_localStop(0)
    , m_localUser(0)
{
}

void ConsoleHost::setLocalRun(void (*startFn)(void*), void (*stopFn)(void*), void* user)
{
    m_localStart = startFn;
    m_localStop = stopFn;
    m_localUser = user;
}

ConsoleHost::~ConsoleHost()
{
    m_run = false;
    if (m_thread) {
        std::thread* thread = static_cast<std::thread*>(m_thread);
        if (thread->joinable()) {
            thread->join();
        }
        delete thread;
        m_thread = 0;
    }
}

void ConsoleHost::printStatus()
{
    Vision::SharedFrames& frames = Vision::SharedFrames::instance();
    const std::string fault = frames.fault() == Vision::IpcFaultNone ? std::string("none") : frames.faultText();
    printf("stationBeatAgeMs=%d linkBeatAgeMs=%d fault=%s local=%d remote=%d\n",
           frames.stationBeatAgeMs(), frames.linkBeatAgeMs(), fault.c_str(),
           frames.localCount(), frames.remoteCount());
    fflush(stdout);
}

void ConsoleHost::loop()
{
    HANDLE input = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (input == INVALID_HANDLE_VALUE) {
        return;
    }
    // 先 Peek 再读，没有输入时短睡，让 m_run 置假后能及时退出
    std::string line;
    while (m_run) {
        INPUT_RECORD rec;
        DWORD got = 0;
        if (!PeekConsoleInputW(input, &rec, 1, &got)) {
            break;
        }
        if (got == 0) {
            Sleep(50);
            continue;
        }
        if (!ReadConsoleInputW(input, &rec, 1, &got) || got == 0) {
            break;
        }
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) {
            continue;
        }
        const wchar_t ch = rec.Event.KeyEvent.uChar.UnicodeChar;
        if (ch == 0) {
            continue;
        }
        // 自己回显：只收 ASCII，回车结束一行
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (ch != L'\r' && ch != L'\n') {
            if (ch < 128) {
                line.push_back(static_cast<char>(ch));
                DWORD wrote = 0;
                WriteConsoleW(output, &ch, 1, &wrote, 0);
            }
            continue;
        }
        if (ch == L'\n') {
            continue;
        }
        {
            DWORD wrote = 0;
            WriteConsoleW(output, L"\r\n", 2, &wrote, 0);
        }
        // 单机设了本地回调就直接调，否则写共享内存命令给服务
        if (line == "status") {
            printStatus();
        } else if (line == "start") {
            if (m_localStart) {
                m_localStart(m_localUser);
            } else {
                Vision::SharedFrames::instance().writeCommand(Vision::IpcCmdStartOwned, 0);
            }
            printf("start\n");
        } else if (line == "stop") {
            if (m_localStop) {
                m_localStop(m_localUser);
            } else {
                Vision::SharedFrames::instance().writeCommand(Vision::IpcCmdStopOwned, 0);
            }
            printf("stop\n");
        } else if (line == "reload") {
            Vision::SharedFrames::instance().writeCommand(Vision::IpcCmdReload, 0);
            printf("reload\n");
        } else if (line == "quit") {
            printf("console closed\n");
            fflush(stdout);
            FreeConsole();
            break;
        } else if (!line.empty()) {
            printf("status | start | stop | reload | quit\n");
        }
        fflush(stdout);
        line.clear();
    }
    CloseHandle(input);
    m_run = false;
}

bool ConsoleHost::visible() const
{
    HWND hwnd = GetConsoleWindow();
    return hwnd && IsWindowVisible(hwnd);
}

void ConsoleHost::showWindow()
{
    if (!GetConsoleWindow() || !m_run) {
        start();
        return;
    }
    HWND hwnd = GetConsoleWindow();
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}

void ConsoleHost::hideWindow()
{
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        ShowWindow(hwnd, SW_HIDE);
    }
}

bool ConsoleHost::start()
{
    if (m_run && GetConsoleWindow()) {
        ShowWindow(GetConsoleWindow(), SW_SHOW);
        SetForegroundWindow(GetConsoleWindow());
        return true;
    }
    if (m_thread) {
        std::thread* thread = static_cast<std::thread*>(m_thread);
        if (thread->joinable()) {
            thread->join();
        }
        delete thread;
        m_thread = 0;
    }
    if (!AllocConsole() && !GetConsoleWindow()) {
        return false;
    }
    FILE* out = 0;
    FILE* in = 0;
    freopen_s(&out, "CONOUT$", "w", stdout);
    freopen_s(&in, "CONIN$", "r", stdin);
    SetConsoleTitleW(L"VisionPlatform");
    printf("status | start | stop | reload | quit\n");
    fflush(stdout);
    m_run = true;
    m_thread = new std::thread(&ConsoleHost::loop, this);
    return true;
}
