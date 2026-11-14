/**
 * @file RelayMain.cpp
 * @brief 中继：登记设备、配对一次会话、原样转发两边的字节
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
#include <shellapi.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <bcrypt.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")


#include "RelayHub.h"

/**
 * @class RelayWindow
 * @brief 中继窗口和托盘。关掉窗口只是隐藏，托盘菜单里的退出才结束进程
 */
class RelayWindow {
public:
    /** @brief 字体和托盘都还没建 */
    RelayWindow()
        : m_hwnd(nullptr)
        , m_icon(nullptr)
        , m_quit(false)
        , m_port(0)
        , m_wsPort(0)
        , m_dpi(96)
        , m_titleFont(nullptr)
        , m_subFont(nullptr)
        , m_labelFont(nullptr)
        , m_valueFont(nullptr)
        , m_bodyFont(nullptr)
    {
        std::memset(&m_nid, 0, sizeof(m_nid));
    }

    /** @brief 已经有一份中继时，把它的窗口拉到前台 */
    static void activateExisting()
    {
        HWND hwnd = FindWindowW(L"VisionRemoteRelayWindow", L"中继");
        if (!hwnd) {
            return;
        }
        ShowWindow(hwnd, SW_SHOW);
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }
        SetForegroundWindow(hwnd);
    }

    /** @brief 建窗口、放托盘图标。失败返回 false */
    bool open(int port, int wsPort)
    {
        m_port = port;
        m_wsPort = wsPort;
        m_quit = false;
        m_icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
        HDC screen = GetDC(nullptr);
        if (screen) {
            m_dpi = GetDeviceCaps(screen, LOGPIXELSX);
            if (m_dpi < 96) {
                m_dpi = 96;
            }
            ReleaseDC(nullptr, screen);
        }
        WNDCLASSW wc = {};
        wc.lpfnWndProc = &RelayWindow::proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hIcon = m_icon;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"VisionRemoteRelayWindow";
        RegisterClassW(&wc);
        const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT frame = { 0, 0, px(640), px(400) };
        AdjustWindowRectEx(&frame, style, FALSE, 0);
        const int width = frame.right - frame.left;
        const int height = frame.bottom - frame.top;
        const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
        m_hwnd = CreateWindowExW(0, wc.lpszClassName, L"中继",
            style, x, y, width, height, nullptr, nullptr, wc.hInstance, this);
        if (!m_hwnd) {
            return false;
        }
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        return true;
    }

    /** @brief 消息循环。返回前托盘图标已去掉 */
    int loop()
    {
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return 0;
    }

private:
    static const UINT kTray = WM_APP + 1;

    /** @brief 窗口过程。指针放在 GWLP_USERDATA */
    static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        RelayWindow* self = reinterpret_cast<RelayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            CREATESTRUCTW* created = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = reinterpret_cast<RelayWindow*>(created->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->m_hwnd = hwnd;
        }
        if (!self) {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
        if (message == WM_CREATE) {
            self->onCreate(hwnd);
            return 0;
        }
        if (message == WM_ERASEBKGND) {
            return 1;
        }
        if (message == WM_PAINT) {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            self->paint(dc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        if (message == WM_CLOSE) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        if (message == kTray) {
            self->onTray(lparam);
            return 0;
        }
        if (message == WM_COMMAND && LOWORD(wparam) == 1) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            return 0;
        }
        if (message == WM_COMMAND && LOWORD(wparam) == 2) {
            self->quit();
            return 0;
        }
        if (message == WM_DESTROY) {
            self->releaseFonts();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    /** @brief 按当前 DPI 把设计像素换成屏幕像素 */
    int px(int dip) const
    {
        return MulDiv(dip, m_dpi, 96);
    }

    /** @brief 标题、数字和说明各一把字体，窗口关掉时释放 */
    void ensureFonts()
    {
        if (m_titleFont) {
            return;
        }
        m_titleFont = makeFont(28, FW_SEMIBOLD);
        m_subFont = makeFont(13, FW_NORMAL);
        m_labelFont = makeFont(12, FW_NORMAL);
        m_valueFont = makeFont(26, FW_SEMIBOLD);
        m_bodyFont = makeFont(13, FW_NORMAL);
    }

    /** @brief Segoe UI，按设计字号随 DPI 放大 */
    HFONT makeFont(int dip, int weight) const
    {
        return CreateFontW(-px(dip), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    /** @brief 整块铺色 */
    void fill(HDC dc, const RECT& box, COLORREF color) const
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &box, brush);
        DeleteObject(brush);
    }

    /** @brief 圆角卡片。边框和填充同一支笔刷画出来 */
    void fillRound(HDC dc, const RECT& box, int radius, COLORREF color, COLORREF edge) const
    {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_SOLID, 1, edge);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        RoundRect(dc, box.left, box.top, box.right, box.bottom, radius, radius);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    /** @brief 透明底的一行字 */
    void drawText(HDC dc, const RECT& box, const wchar_t* text, HFONT font, COLORREF color, UINT format) const
    {
        HGDIOBJ old = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        RECT area = box;
        DrawTextW(dc, text, -1, &area, format);
        SelectObject(dc, old);
    }

    /** @brief 页头右侧的两个白点，和任务栏图标同一个图形 */
    void drawMark(HDC dc, int x, int y, int size) const
    {
        HPEN pen = CreatePen(PS_SOLID, px(3), RGB(255, 255, 255));
        HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        const int cy = y + size / 2;
        const int rad = size / 7;
        MoveToEx(dc, x + size / 4, cy, nullptr);
        LineTo(dc, x + size * 3 / 4, cy);
        Ellipse(dc, x + size / 5 - rad, cy - rad, x + size / 5 + rad, cy + rad);
        Ellipse(dc, x + size * 4 / 5 - rad, cy - rad, x + size * 4 / 5 + rad, cy + rad);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    /** @brief 双缓冲画页头、两张端口卡和托盘说明 */
    void paint(HDC target)
    {
        ensureFonts();
        RECT client;
        GetClientRect(m_hwnd, &client);
        const int width = client.right;
        const int height = client.bottom;
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

        RECT page = { 0, 0, width, height };
        fill(dc, page, RGB(245, 245, 245));
        RECT head = { 0, 0, width, px(108) };
        fill(dc, head, RGB(194, 65, 12));
        RECT title = { px(28), px(26), width - px(96), px(62) };
        drawText(dc, title, L"中继", m_titleFont, RGB(255, 255, 255), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        RECT sub = { px(28), px(62), width - px(96), px(92) };
        drawText(dc, sub, L"局域网配对和识别码转发", m_subFont, RGB(255, 220, 200), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawMark(dc, width - px(72), px(36), px(36));

        const int gap = px(16);
        const int cardTop = px(132);
        const int cardH = px(168);
        const int cardW = (width - gap * 3) / 2;
        RECT left = { gap, cardTop, gap + cardW, cardTop + cardH };
        RECT right = { gap * 2 + cardW, cardTop, gap * 2 + cardW * 2, cardTop + cardH };
        fillRound(dc, left, px(12), RGB(255, 255, 255), RGB(232, 232, 232));
        fillRound(dc, right, px(12), RGB(255, 255, 255), RGB(232, 232, 232));

        wchar_t lan[32];
        wchar_t access[64];
        swprintf_s(lan, L"%d", m_port);
        swprintf_s(access, L"127.0.0.1:%d", m_wsPort);
        paintCard(dc, left, L"局域网", lan, L"同一网段的中控台和被控端在这里配对。");
        paintCard(dc, right, L"识别码", access, L"只监听本机，交给隧道转发出去。");

        RECT foot = { px(20), height - px(48), width - px(20), height - px(16) };
        drawText(dc, foot, L"关闭窗口会留在任务栏托盘。要退出，在托盘图标上右键。",
            m_bodyFont, RGB(140, 140, 140), DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }

    /** @brief 一张端口卡：小标题、大数字、一行说明 */
    void paintCard(HDC dc, const RECT& box, const wchar_t* name, const wchar_t* value, const wchar_t* note) const
    {
        RECT label = { box.left + px(20), box.top + px(18), box.right - px(16), box.top + px(40) };
        drawText(dc, label, name, m_labelFont, RGB(140, 140, 140), DT_LEFT | DT_SINGLELINE);
        RECT number = { box.left + px(20), box.top + px(44), box.right - px(12), box.top + px(88) };
        drawText(dc, number, value, m_valueFont, RGB(38, 38, 38), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT body = { box.left + px(20), box.top + px(96), box.right - px(16), box.bottom - px(16) };
        drawText(dc, body, note, m_bodyFont, RGB(89, 89, 89), DT_LEFT | DT_WORDBREAK);
    }

    /** @brief 托盘图标。窗口内容在 WM_PAINT 里画 */
    void onCreate(HWND hwnd)
    {
        ensureFonts();
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(m_icon));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(m_icon));

        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = kTray;
        nid.hIcon = m_icon;
        wcscpy_s(nid.szTip, L"中继");
        Shell_NotifyIconW(NIM_ADD, &nid);
        m_nid = nid;
    }

    /** @brief 单击托盘打开窗口，右键弹出菜单 */
    void onTray(LPARAM lparam)
    {
        if (lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK) {
            ShowWindow(m_hwnd, SW_SHOW);
            SetForegroundWindow(m_hwnd);
            return;
        }
        if (lparam != WM_RBUTTONUP) {
            return;
        }
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 1, L"打开");
        AppendMenuW(menu, MF_STRING, 2, L"退出");
        POINT point;
        GetCursorPos(&point);
        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, m_hwnd, nullptr);
        DestroyMenu(menu);
    }

    /** @brief 去掉托盘图标并销毁窗口，消息循环随后结束 */
    void quit()
    {
        if (m_quit) {
            return;
        }
        m_quit = true;
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        DestroyWindow(m_hwnd);
    }

    /** @brief 窗口销毁前丢掉字体 */
    void releaseFonts()
    {
        HFONT* fonts[] = { &m_titleFont, &m_subFont, &m_labelFont, &m_valueFont, &m_bodyFont };
        for (int i = 0; i < 5; ++i) {
            if (*fonts[i]) {
                DeleteObject(*fonts[i]);
                *fonts[i] = nullptr;
            }
        }
    }

    HWND m_hwnd;
    NOTIFYICONDATAW m_nid;
    HICON m_icon;
    bool m_quit;
    int m_port;
    int m_wsPort;
    int m_dpi;
    HFONT m_titleFont;
    HFONT m_subFont;
    HFONT m_labelFont;
    HFONT m_valueFont;
    HFONT m_bodyFont;
};

/** @brief 第一个参数是局域网 TCP 端口，第二个是识别码 WebSocket 端口 */
int main(int argc, char** argv)
{
    int port = 19200;
    int wsPort = 19280;
    if (argc >= 2) {
        port = atoi(argv[1]);
    }
    if (argc >= 3) {
        wsPort = atoi(argv[2]);
    }
    if (port < 1 || port > 65535) {
        port = 19200;
    }
    if (wsPort < 1 || wsPort > 65535) {
        wsPort = 19280;
    }
    SetLastError(ERROR_SUCCESS);
    HANDLE single = CreateMutexW(nullptr, TRUE, L"Local\\VisionRemoteRelay");
    if (!single || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (single) {
            CloseHandle(single);
        }
        RelayWindow::activateExisting();
        return 0;
    }
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        CloseHandle(single);
        return 1;
    }
    Vision::RelayHub hub;
    if (!hub.start(port)) {
        MessageBoxW(nullptr, L"局域网端口绑定失败。", L"中继", MB_ICONERROR);
        WSACleanup();
        CloseHandle(single);
        return 1;
    }
    if (!hub.startWs(wsPort)) {
        MessageBoxW(nullptr, L"识别码端口绑定失败，局域网中继仍可用。", L"中继", MB_ICONWARNING);
    }
    RelayWindow window;
    if (!window.open(port, wsPort)) {
        hub.stop();
        WSACleanup();
        CloseHandle(single);
        return 1;
    }
    const int code = window.loop();
    hub.stop();
    WSACleanup();
    CloseHandle(single);
    return code;
}

