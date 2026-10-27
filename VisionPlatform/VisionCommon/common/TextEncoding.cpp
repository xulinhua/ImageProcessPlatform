/**
 * @file TextEncoding.cpp
 * @brief TextEncoding实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TextEncoding.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

// 宽字符转到指定代码页
std::string wideToCp(const std::wstring& w, UINT cp)
{
    if (w.empty()) {
        return std::string();
    }
    const int n = WideCharToMultiByte(cp, 0, w.c_str(), static_cast<int>(w.size()),
                                      0, 0, 0, 0);
    if (n <= 0) {
        return std::string();
    }
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(cp, 0, w.c_str(), static_cast<int>(w.size()),
                        &out[0], n, 0, 0);
    return out;
}

// 指定代码页转到宽字符
std::wstring cpToWide(const std::string& s, UINT cp)
{
    if (s.empty()) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(cp, 0, s.data(), static_cast<int>(s.size()), 0, 0);
    if (n <= 0) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(cp, 0, s.data(), static_cast<int>(s.size()), &out[0], n);
    return out;
}

} // namespace

// UTF-8 转到系统 ANSI
std::string utf8ToAcp(const std::string& utf8)
{
    return wideToCp(cpToWide(utf8, CP_UTF8), CP_ACP);
}

// 系统 ANSI 转到 UTF-8
std::string acpToUtf8(const std::string& acp)
{
    return wideToCp(cpToWide(acp, CP_ACP), CP_UTF8);
}

// 系统 ANSI 转到宽字符，供宽字符文件接口使用
std::wstring acpToWide(const std::string& acp)
{
    return cpToWide(acp, CP_ACP);
}

// 宽字符路径转到系统 ANSI
std::string wideToAcp(const wchar_t* wide)
{
    if (!wide || !wide[0]) {
        return std::string();
    }
    const int n = WideCharToMultiByte(CP_ACP, 0, wide, -1, 0, 0, 0, 0);
    if (n <= 1) {
        return std::string();
    }
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_ACP, 0, wide, -1, &out[0], n, 0, 0);
    return out;
}


// UTF-8路径转宽字符
std::wstring utf8ToWide(const std::string& utf8)
{
    return cpToWide(utf8, CP_UTF8);
}

// 宽字符路径转 UTF-8
std::string wideToUtf8(const wchar_t* wide)
{
    if (!wide || !wide[0]) {
        return std::string();
    }
    return wideToCp(std::wstring(wide), CP_UTF8);
}
}
