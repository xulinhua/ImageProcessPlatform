/**
 * @file clean_window.cpp
 * @brief 清理窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "clean_window.h"

#include <commctrl.h>
#include <string>

namespace {
const int kList = 101;
const int kRefresh = 102;
const int kKill = 103;
const int kKillAll = 104;
}

CleanWindow::CleanWindow()
    : m_list(0)
    , m_status(0)
    , m_font(0)
{
}

int CleanWindow::run(HINSTANCE instance)
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW cls;
    ZeroMemory(&cls, sizeof(cls));
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursor(0, IDC_ARROW);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    cls.hIcon = reinterpret_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    cls.hIconSm = reinterpret_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    cls.lpszClassName = L"VisionCleanWindow";
    RegisterClassExW(&cls);

    HWND hwnd = CreateWindowExW(0, cls.lpszClassName, L"VisionPlatform关联进程清理",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 860, 520,
        0, 0, instance, this);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK CleanWindow::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    CleanWindow* self = 0;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<CleanWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<CleanWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return self->onMessage(hwnd, msg, wp, lp);
}

void CleanWindow::createChild(HWND hwnd)
{
    m_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
        L"Microsoft YaHei UI");
    HWND refresh = CreateWindowExW(0, L"BUTTON", L"刷新", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        12, 12, 88, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRefresh)), 0, 0);
    HWND kill = CreateWindowExW(0, L"BUTTON", L"结束选中", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        108, 12, 96, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kKill)), 0, 0);
    HWND all = CreateWindowExW(0, L"BUTTON", L"全部结束", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        212, 12, 96, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kKillAll)), 0, 0);
    m_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        12, 50, 820, 380, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kList)), 0, 0);
    ListView_SetExtendedListViewStyle(m_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = const_cast<wchar_t*>(L"PID");
    col.cx = 80;
    ListView_InsertColumn(m_list, 0, &col);
    col.pszText = const_cast<wchar_t*>(L"进程");
    col.cx = 180;
    ListView_InsertColumn(m_list, 1, &col);
    col.pszText = const_cast<wchar_t*>(L"路径");
    col.cx = 540;
    ListView_InsertColumn(m_list, 2, &col);
    m_status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        12, 440, 820, 24, hwnd, 0, 0, 0);
    SendMessageW(refresh, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    SendMessageW(kill, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    SendMessageW(all, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    SendMessageW(m_list, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    SendMessageW(m_status, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    fill();
}

void CleanWindow::fill()
{
    m_procs.refresh();
    ListView_DeleteAllItems(m_list);
    const std::vector<SolutionProc>& rows = m_procs.list();
    for (int i = 0; i < (int)rows.size(); ++i) {
        wchar_t pid[32];
        wsprintfW(pid, L"%lu", rows[i].pid);
        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.pszText = pid;
        item.lParam = (LPARAM)rows[i].pid;
        ListView_InsertItem(m_list, &item);
        ListView_SetItemText(m_list, i, 1, const_cast<wchar_t*>(rows[i].name.c_str()));
        ListView_SetItemText(m_list, i, 2, const_cast<wchar_t*>(rows[i].path.c_str()));
    }
    std::wstring text = L"按程序名    进程 " + std::to_wstring(rows.size());
    SetWindowTextW(m_status, text.c_str());
}

std::vector<unsigned long> CleanWindow::selected() const
{
    std::vector<unsigned long> ids;
    int index = -1;
    while ((index = ListView_GetNextItem(m_list, index, LVNI_SELECTED)) >= 0) {
        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_PARAM;
        item.iItem = index;
        if (ListView_GetItem(m_list, &item)) {
            ids.push_back((unsigned long)item.lParam);
        }
    }
    return ids;
}

void CleanWindow::killIds(const std::vector<unsigned long>& pids, bool ask)
{
    if (pids.empty()) {
        SetWindowTextW(m_status, L"没有选中进程");
        return;
    }
    if (ask) {
        std::wstring tip = L"结束 " + std::to_wstring(pids.size()) + L" 个进程？";
        if (MessageBoxW(GetParent(m_list), tip.c_str(), L"VisionPlatform关联进程清理", MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
            return;
        }
    }
    std::wstring err;
    const int failed = m_procs.kill(pids, &err);
    fill();
    if (failed && !err.empty()) {
        SetWindowTextW(m_status, err.c_str());
    }
}

LRESULT CleanWindow::onMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        createChild(hwnd);
        return 0;
    case WM_SIZE: {
        const int w = LOWORD(lp);
        const int h = HIWORD(lp);
        if (m_list) {
            MoveWindow(m_list, 12, 50, w - 24, h - 90, TRUE);
            MoveWindow(m_status, 12, h - 32, w - 24, 22, TRUE);
        }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == kRefresh) {
            fill();
        } else if (LOWORD(wp) == kKill) {
            killIds(selected(), true);
        } else if (LOWORD(wp) == kKillAll) {
            std::vector<unsigned long> ids;
            const std::vector<SolutionProc>& rows = m_procs.list();
            for (size_t i = 0; i < rows.size(); ++i) {
                ids.push_back(rows[i].pid);
            }
            killIds(ids, true);
        }
        return 0;
    case WM_DESTROY:
        if (m_font) {
            DeleteObject(m_font);
            m_font = 0;
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
