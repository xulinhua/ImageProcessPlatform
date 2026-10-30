/**
 * @file SasSender.cpp
 * @brief SendSAS / 模拟 Ctrl+Alt+Del
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SasSender.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Vision {

namespace {

typedef BOOL (WINAPI* SendSasFn)(BOOL);

}

bool SasSender::send()
{
    // Vista+：sas.dll!SendSAS。组策略“允许软件安全地生成模拟安全提示序列”需开启
    HMODULE mod = LoadLibraryW(L"sas.dll");
    if (mod) {
        SendSasFn fn = reinterpret_cast<SendSasFn>(GetProcAddress(mod, "SendSAS"));
        if (fn) {
            const BOOL ok = fn(FALSE);
            FreeLibrary(mod);
            if (ok) {
                return true;
            }
        } else {
            FreeLibrary(mod);
        }
    }
    // 退路：模拟三键（在登录桌面通常无效，但用户会话里可作提示）
    INPUT keys[6] = {};
    keys[0].type = INPUT_KEYBOARD;
    keys[0].ki.wVk = VK_CONTROL;
    keys[1].type = INPUT_KEYBOARD;
    keys[1].ki.wVk = VK_MENU;
    keys[2].type = INPUT_KEYBOARD;
    keys[2].ki.wVk = VK_DELETE;
    keys[3] = keys[2];
    keys[3].ki.dwFlags = KEYEVENTF_KEYUP;
    keys[4] = keys[1];
    keys[4].ki.dwFlags = KEYEVENTF_KEYUP;
    keys[5] = keys[0];
    keys[5].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(6, keys, sizeof(INPUT)) == 6;
}

}
