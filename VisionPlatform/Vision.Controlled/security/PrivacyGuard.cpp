/**
 * @file PrivacyGuard.cpp
 * @brief 隐私屏低级钩子
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PrivacyGuard.h"

namespace Vision {

PrivacyGuard* PrivacyGuard::s_guard = 0;

PrivacyGuard::PrivacyGuard()
    : m_key(0)
    , m_mouse(0)
    , m_on(false)
{
}

PrivacyGuard::~PrivacyGuard()
{
    setEnabled(false);
}

void PrivacyGuard::setEnabled(bool on)
{
    if (on == m_on) {
        return;
    }
    if (!on) {
        if (m_key) {
            UnhookWindowsHookEx(m_key);
            m_key = 0;
        }
        if (m_mouse) {
            UnhookWindowsHookEx(m_mouse);
            m_mouse = 0;
        }
        if (s_guard == this) {
            s_guard = 0;
        }
        m_on = false;
        return;
    }
    s_guard = this;
    m_key = SetWindowsHookExW(WH_KEYBOARD_LL, &PrivacyGuard::onKey, GetModuleHandleW(0), 0);
    m_mouse = SetWindowsHookExW(WH_MOUSE_LL, &PrivacyGuard::onMouse, GetModuleHandleW(0), 0);
    m_on = m_key != 0 && m_mouse != 0;
    if (!m_on) {
        setEnabled(false);
    }
}

LRESULT CALLBACK PrivacyGuard::onKey(int code, WPARAM wp, LPARAM lp)
{
    if (s_guard) {
        return s_guard->filter(code, wp, lp, true);
    }
    return CallNextHookEx(0, code, wp, lp);
}

LRESULT CALLBACK PrivacyGuard::onMouse(int code, WPARAM wp, LPARAM lp)
{
    if (s_guard) {
        return s_guard->filter(code, wp, lp, false);
    }
    return CallNextHookEx(0, code, wp, lp);
}

LRESULT PrivacyGuard::filter(int code, WPARAM wp, LPARAM lp, bool keyboard)
{
    if (code == HC_ACTION && m_on) {
        if (keyboard) {
            const KBDLLHOOKSTRUCT* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
            if (info && (info->flags & LLKHF_INJECTED) == 0) {
                return 1;
            }
        } else {
            const MSLLHOOKSTRUCT* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lp);
            if (info && (info->flags & LLMHF_INJECTED) == 0) {
                return 1;
            }
        }
    }
    return CallNextHookEx(keyboard ? m_key : m_mouse, code, wp, lp);
}

}
