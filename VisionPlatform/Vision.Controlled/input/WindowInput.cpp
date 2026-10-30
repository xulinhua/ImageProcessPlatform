/**
 * @file WindowInput.cpp
 * @brief PostMessage 到目标客户区，不用 SendInput
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "WindowInput.h"

namespace Vision {

WindowInput::WindowInput()
    : m_haveLast(false)
    , m_pauseUntil(0)
{
    m_last.x = 0;
    m_last.y = 0;
}

int WindowInput::integrityRid(HANDLE token) const
{
    DWORD need = 0;
    GetTokenInformation(token, TokenIntegrityLevel, 0, 0, &need);
    if (need == 0) {
        return 0;
    }
    CommBytes buf(need);
    if (!GetTokenInformation(token, TokenIntegrityLevel, &buf[0], need, &need)) {
        return 0;
    }
    TOKEN_MANDATORY_LABEL* label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(&buf[0]);
    PUCHAR count = GetSidSubAuthorityCount(label->Label.Sid);
    if (!count || *count == 0) {
        return 0;
    }
    DWORD* rid = GetSidSubAuthority(label->Label.Sid, static_cast<DWORD>(*count) - 1);
    if (!rid) {
        return 0;
    }
    return static_cast<int>(*rid);
}

bool WindowInput::operable(HWND hwnd) const
{
    if (!hwnd) {
        return false;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return false;
    }
    HANDLE token = 0;
    const BOOL opened = OpenProcessToken(process, TOKEN_QUERY, &token);
    CloseHandle(process);
    if (!opened) {
        return false;
    }
    HANDLE self = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &self)) {
        CloseHandle(token);
        return false;
    }
    const int target = integrityRid(token);
    const int mine = integrityRid(self);
    CloseHandle(token);
    CloseHandle(self);
    if (target == 0 || mine == 0) {
        return false;
    }
    return target <= mine;
}

bool WindowInput::allowRemote(HWND hwnd)
{
    if (!hwnd) {
        return false;
    }
    POINT pt;
    GetCursorPos(&pt);
    const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
        || (GetAsyncKeyState(VK_MBUTTON) & 0x8000);
    const bool moved = m_haveLast && (pt.x != m_last.x || pt.y != m_last.y);
    m_last = pt;
    m_haveLast = true;
    POINT client = pt;
    ScreenToClient(hwnd, &client);
    RECT rect;
    GetClientRect(hwnd, &rect);
    const bool inside = PtInRect(&rect, client) != 0;
    if (GetForegroundWindow() == hwnd && inside && (down || moved)) {
        m_pauseUntil = GetTickCount64() + 1000;
    }
    return GetTickCount64() >= m_pauseUntil;
}

bool WindowInput::allowRemoteRect(const RECT& rect)
{
    POINT pt;
    GetCursorPos(&pt);
    const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        || (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
        || (GetAsyncKeyState(VK_MBUTTON) & 0x8000);
    const bool moved = m_haveLast && (pt.x != m_last.x || pt.y != m_last.y);
    m_last = pt;
    m_haveLast = true;
    if (PtInRect(&rect, pt) && (down || moved)) {
        m_pauseUntil = GetTickCount64() + 1000;
    }
    return GetTickCount64() >= m_pauseUntil;
}

void WindowInput::postScreen(int originX, int originY, const InputMsg& msg, uint32_t clientW, uint32_t clientH, uint32_t imageW, uint32_t imageH)
{
    if (imageW == 0 || imageH == 0 || clientW == 0 || clientH == 0) {
        return;
    }
    int x = originX + static_cast<int>(static_cast<uint64_t>(msg.x) * clientW / imageW);
    int y = originY + static_cast<int>(static_cast<uint64_t>(msg.y) * clientH / imageH);
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw <= 1 || vh <= 1) {
        return;
    }
    INPUT in;
    ZeroMemory(&in, sizeof(in));
    if (msg.vk != 0) {
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(msg.vk);
        in.ki.dwFlags = msg.down ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(in));
        markHeld(0, msg.vk, 0, 0, msg.down != 0);
        return;
    }
    in.type = INPUT_MOUSE;
    in.mi.dx = static_cast<LONG>((x - vx) * 65535 / (vw - 1));
    in.mi.dy = static_cast<LONG>((y - vy) * 65535 / (vh - 1));
    in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE;
    // 移动（含按住键拖动）只发 MOVE，按键状态由系统保持
    const int edge = edgeOf(msg);
    if (msg.wheel != 0) {
        in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_WHEEL;
        in.mi.mouseData = static_cast<DWORD>(msg.wheel * WHEEL_DELTA);
    } else if (edge > 0 && (msg.buttons & 1)) {
        in.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
    } else if (edge < 0 && (msg.buttons & 1)) {
        in.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
    } else if (edge > 0 && (msg.buttons & 2)) {
        in.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
    } else if (edge < 0 && (msg.buttons & 2)) {
        in.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
    } else if (edge > 0 && (msg.buttons & 4)) {
        in.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
    } else if (edge < 0 && (msg.buttons & 4)) {
        in.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
    }
    SendInput(1, &in, sizeof(in));
    if (msg.wheel == 0 && edge != 0) {
        markHeld(0, 0, lowestButton(msg.buttons), 0, edge > 0);
    }
}

uint8_t WindowInput::lowestButton(uint8_t buttons)
{
    if (buttons & 1) {
        return 1;
    }
    if (buttons & 2) {
        return 2;
    }
    return (buttons & 4) ? 4 : 0;
}

void WindowInput::markHeld(HWND hwnd, uint16_t vk, uint8_t button, LPARAM pos, bool down)
{
    for (size_t i = 0; i < m_held.size(); ++i) {
        if (m_held[i].hwnd == hwnd && m_held[i].vk == vk && m_held[i].button == button) {
            if (down) {
                m_held[i].pos = pos;
            } else {
                m_held.erase(m_held.begin() + static_cast<std::ptrdiff_t>(i));
            }
            return;
        }
    }
    if (down) {
        Held held;
        held.hwnd = hwnd;
        held.vk = vk;
        held.button = button;
        held.pos = pos;
        m_held.push_back(held);
    }
}

void WindowInput::releaseAll()
{
    // 窗口消息注入的发到当时的目标窗口（窗口已不在时 PostMessage 失败即可）；SendInput 注入的发系统级抬起
    for (size_t i = 0; i < m_held.size(); ++i) {
        const Held& held = m_held[i];
        if (held.hwnd) {
            if (held.vk != 0) {
                PostMessageW(held.hwnd, WM_KEYUP, held.vk, 0);
            } else {
                const UINT kind = held.button == 1 ? WM_LBUTTONUP : (held.button == 2 ? WM_RBUTTONUP : WM_MBUTTONUP);
                PostMessageW(held.hwnd, kind, 0, held.pos);
            }
            continue;
        }
        INPUT in;
        ZeroMemory(&in, sizeof(in));
        if (held.vk != 0) {
            in.type = INPUT_KEYBOARD;
            in.ki.wVk = static_cast<WORD>(held.vk);
            in.ki.dwFlags = KEYEVENTF_KEYUP;
        } else {
            in.type = INPUT_MOUSE;
            in.mi.dwFlags = held.button == 1 ? MOUSEEVENTF_LEFTUP : (held.button == 2 ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_MIDDLEUP);
        }
        SendInput(1, &in, sizeof(in));
    }
    m_held.clear();
}

int WindowInput::edgeOf(const InputMsg& msg)
{
    if (msg.buttons == 0 || msg.action == InputMsg::ActionMove) {
        return 0;
    }
    if (msg.action == InputMsg::ActionPress) {
        return 1;
    }
    if (msg.action == InputMsg::ActionRelease) {
        return -1;
    }
    return msg.down ? 1 : -1;
}

void WindowInput::post(HWND hwnd, const InputMsg& msg, uint32_t clientW, uint32_t clientH, uint32_t imageW, uint32_t imageH)
{
    if (!hwnd || imageW == 0 || imageH == 0 || clientW == 0 || clientH == 0) {
        return;
    }
    // 图像坐标按比例换成客户区坐标，并夹在客户区内
    int x = static_cast<int>(static_cast<uint64_t>(msg.x) * clientW / imageW);
    int y = static_cast<int>(static_cast<uint64_t>(msg.y) * clientH / imageH);
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x >= static_cast<int>(clientW)) {
        x = static_cast<int>(clientW) - 1;
    }
    if (y >= static_cast<int>(clientH)) {
        y = static_cast<int>(clientH) - 1;
    }
    // 鼠标消息的 wParam 带上当前按住的按钮
    WPARAM mk = 0;
    if (msg.buttons & 1) {
        mk |= MK_LBUTTON;
    }
    if (msg.buttons & 2) {
        mk |= MK_RBUTTON;
    }
    if (msg.buttons & 4) {
        mk |= MK_MBUTTON;
    }
    if (msg.vk != 0) {
        PostMessageW(hwnd, msg.down ? WM_KEYDOWN : WM_KEYUP, msg.vk, 0);
        markHeld(hwnd, msg.vk, 0, 0, msg.down != 0);
        return;
    }
    // WM_MOUSEWHEEL 的坐标是屏幕坐标
    if (msg.wheel != 0) {
        POINT screen;
        screen.x = x;
        screen.y = y;
        ClientToScreen(hwnd, &screen);
        PostMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(mk, msg.wheel * WHEEL_DELTA), MAKELPARAM(screen.x, screen.y));
        return;
    }
    // 按钮按下/抬起按左、右、中的顺序取第一个置位的按钮；移动（含拖动）发 WM_MOUSEMOVE，mk 带按住的按钮
    const int edge = edgeOf(msg);
    UINT kind = WM_MOUSEMOVE;
    if (edge > 0 && (msg.buttons & 1)) {
        kind = WM_LBUTTONDOWN;
    } else if (edge < 0 && (msg.buttons & 1)) {
        kind = WM_LBUTTONUP;
    } else if (edge > 0 && (msg.buttons & 2)) {
        kind = WM_RBUTTONDOWN;
    } else if (edge < 0 && (msg.buttons & 2)) {
        kind = WM_RBUTTONUP;
    } else if (edge > 0 && (msg.buttons & 4)) {
        kind = WM_MBUTTONDOWN;
    } else if (edge < 0 && (msg.buttons & 4)) {
        kind = WM_MBUTTONUP;
    }
    PostMessageW(hwnd, kind, mk, MAKELPARAM(x, y));
    // 按下/抬起登记；拖动时把同一窗口里按着的按钮位置跟到最新，补发抬起落在拖到的位置
    if (edge != 0) {
        markHeld(hwnd, 0, lowestButton(msg.buttons), MAKELPARAM(x, y), edge > 0);
        return;
    }
    for (size_t i = 0; i < m_held.size(); ++i) {
        if (m_held[i].hwnd == hwnd && m_held[i].vk == 0) {
            m_held[i].pos = MAKELPARAM(x, y);
        }
    }
}

}
