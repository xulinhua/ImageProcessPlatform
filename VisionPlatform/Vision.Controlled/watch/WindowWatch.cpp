/**
 * @file WindowWatch.cpp
 * @brief 周期查找目标窗口。找不到时不改去抓桌面
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "WindowWatch.h"
#include <dwmapi.h>
#include <algorithm>

namespace Vision {

WindowWatch::WindowWatch()
    : m_run(false)
    , m_hwnd(0)
{
}

WindowWatch::~WindowWatch()
{
    stop();
}

void WindowWatch::start(const std::wstring& title, const std::wstring& processName)
{
    stop();
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_title = title;
        m_process = processName;
        m_hwnd = 0;
        m_run = true;
    }
    m_thread = std::thread(&WindowWatch::loop, this);
}

void WindowWatch::stop()
{
    m_run = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

HWND WindowWatch::current() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_hwnd;
}

std::vector<WindowWatch::Process> WindowWatch::processes()
{
    std::vector<Process> out;
    EnumWindows(listProc, reinterpret_cast<LPARAM>(&out));
    std::sort(out.begin(), out.end(), [](const Process& a, const Process& b) {
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return out;
}

BOOL CALLBACK WindowWatch::listProc(HWND hwnd, LPARAM param)
{
    std::vector<Process>* out = reinterpret_cast<std::vector<Process>*>(param);
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != 0 || GetWindowTextLengthW(hwnd) == 0) {
        return TRUE;
    }
    if ((GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) || cloaked(hwnd)) {
        return TRUE;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) {
        return TRUE;
    }
    Process item;
    item.path = exePath(hwnd);
    item.name = fileName(item.path);
    if (item.name.empty()) {
        return TRUE;
    }
    for (size_t i = 0; i < out->size(); ++i) {
        if (_wcsicmp((*out)[i].name.c_str(), item.name.c_str()) == 0) {
            return TRUE;
        }
    }
    out->push_back(item);
    return TRUE;
}

std::wstring WindowWatch::exePath(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return std::wstring();
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return std::wstring();
    }
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    const BOOL ok = QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    return ok ? std::wstring(path, size) : std::wstring();
}

std::wstring WindowWatch::fileName(const std::wstring& path)
{
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

bool WindowWatch::matchProcess(HWND hwnd, const std::wstring& processName)
{
    const std::wstring name = fileName(exePath(hwnd));
    return !name.empty() && _wcsicmp(name.c_str(), processName.c_str()) == 0;
}

BOOL CALLBACK WindowWatch::enumProc(HWND hwnd, LPARAM param)
{
    Want* want = reinterpret_cast<Want*>(param);
    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
        return TRUE;
    }
    if (GetWindow(hwnd, GW_OWNER) != 0) {
        return TRUE;
    }
    if (!want->processName.empty() && !matchProcess(hwnd, want->processName)) {
        return TRUE;
    }
    if (!want->title.empty()) {
        wchar_t title[512];
        GetWindowTextW(hwnd, title, 512);
        if (!wcsstr(title, want->title.c_str())) {
            return TRUE;
        }
    }
    if (want->title.empty() && want->processName.empty()) {
        return TRUE;
    }
    want->hits.push_back(hwnd);
    return TRUE;
}

HWND WindowWatch::pick(const std::vector<HWND>& hits, HWND last)
{
    HWND best = 0;
    int bestRank = -1;
    long long bestArea = -1;
    for (size_t i = 0; i < hits.size(); ++i) {
        const int r = rank(hits[i]);
        const long long a = area(hits[i]);
        if (r > bestRank || (r == bestRank && a > bestArea)) {
            best = hits[i];
            bestRank = r;
            bestArea = a;
        }
    }
    if (last && std::find(hits.begin(), hits.end(), last) != hits.end() && rank(last) >= bestRank) {
        return last;
    }
    return best;
}

int WindowWatch::rank(HWND hwnd)
{
    int r = cloaked(hwnd) ? 0 : 2;
    if (GetWindowTextLengthW(hwnd) > 0 && !(GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
        r += 1;
    }
    return r;
}

long long WindowWatch::area(HWND hwnd)
{
    RECT rc = {};
    if (IsIconic(hwnd)) {
        WINDOWPLACEMENT wp = {};
        wp.length = sizeof(wp);
        if (!GetWindowPlacement(hwnd, &wp)) {
            return 0;
        }
        rc = wp.rcNormalPosition;
    } else if (!GetWindowRect(hwnd, &rc)) {
        return 0;
    }
    return static_cast<long long>(rc.right - rc.left) * (rc.bottom - rc.top);
}

bool WindowWatch::cloaked(HWND hwnd)
{
    DWORD value = 0;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &value, sizeof(value))) && value != 0;
}

void WindowWatch::loop()
{
    while (m_run) {
        Want want;
        HWND last = 0;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            want.title = m_title;
            want.processName = m_process;
            last = m_hwnd;
        }
        EnumWindows(enumProc, reinterpret_cast<LPARAM>(&want));
        const HWND found = pick(want.hits, last);
        {
            std::lock_guard<std::mutex> lock(m_mu);
            m_hwnd = found;
        }
        Sleep(300);
    }
}

}
