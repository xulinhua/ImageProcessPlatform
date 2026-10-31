/**
 * @file RemotePaths.cpp
 * @brief exe旁\\exe名 数据目录与旧路径迁移
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemotePaths.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

namespace Vision {

namespace {

std::wstring modulePath()
{
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(0, path, MAX_PATH) == 0) {
        return L".";
    }
    return path;
}

std::wstring moduleDir()
{
    std::wstring path = modulePath();
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash);
    }
    return path.empty() ? L"." : path;
}

std::wstring moduleStem()
{
    std::wstring path = modulePath();
    const size_t slash = path.find_last_of(L"\\/");
    std::wstring name = slash == std::wstring::npos ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos && dot > 0) {
        name.resize(dot);
    }
    return name.empty() ? L"Remote" : name;
}

void makeDir(const std::wstring& dir)
{
    if (!dir.empty()) {
        CreateDirectoryW(dir.c_str(), 0);
    }
}

bool fileExists(const std::wstring& path)
{
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool dirExists(const std::wstring& path)
{
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void moveIfAbsent(const std::wstring& from, const std::wstring& to)
{
    if (!fileExists(from) || fileExists(to)) {
        return;
    }
    if (MoveFileW(from.c_str(), to.c_str())) {
        return;
    }
    if (CopyFileW(from.c_str(), to.c_str(), TRUE)) {
        DeleteFileW(from.c_str());
    }
}

void moveDirFilesIfAbsent(const std::wstring& fromDir, const std::wstring& toDir)
{
    if (!dirExists(fromDir)) {
        return;
    }
    makeDir(toDir);
    WIN32_FIND_DATAW find = {};
    HANDLE handle = FindFirstFileW((fromDir + L"\\*").c_str(), &find);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        moveIfAbsent(fromDir + L"\\" + find.cFileName, toDir + L"\\" + find.cFileName);
    } while (FindNextFileW(handle, &find));
    FindClose(handle);
}

std::wstring localAppVision()
{
    wchar_t dir[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA, 0, SHGFP_TYPE_CURRENT, dir))) {
        return std::wstring();
    }
    return std::wstring(dir) + L"\\Vision.Remote";
}

std::wstring roamingApp(const wchar_t* app)
{
    wchar_t dir[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(0, CSIDL_APPDATA, 0, SHGFP_TYPE_CURRENT, dir))) {
        return std::wstring();
    }
    return std::wstring(dir) + L"\\" + app;
}

}

std::wstring RemotePaths::exeDirW()
{
    return moduleDir();
}

std::wstring RemotePaths::exeStemW()
{
    return moduleStem();
}

std::wstring RemotePaths::dataRootW()
{
    return moduleDir() + L"\\" + moduleStem();
}

std::wstring RemotePaths::configDirW()
{
    return dataRootW() + L"\\config";
}

std::wstring RemotePaths::logDirW()
{
    return dataRootW() + L"\\log";
}

std::wstring RemotePaths::imageDirW()
{
    return dataRootW() + L"\\image";
}

std::wstring RemotePaths::videoDirW()
{
    return dataRootW() + L"\\video";
}

std::wstring RemotePaths::configFileW(const wchar_t* name)
{
    return configDirW() + L"\\" + (name ? name : L"");
}

std::wstring RemotePaths::logFileW(const wchar_t* name)
{
    return logDirW() + L"\\" + (name ? name : L"");
}

std::string RemotePaths::narrow(const std::wstring& path)
{
    if (path.empty()) {
        return std::string();
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, 0, 0, 0, 0);
    if (n <= 1) {
        return std::string();
    }
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &out[0], n, 0, 0);
    return out;
}

std::wstring RemotePaths::widen(const char* utf8)
{
    if (!utf8 || !utf8[0]) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
    if (n <= 1) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], n);
    return out;
}

void RemotePaths::ensureLayout()
{
    const std::wstring exe = moduleDir();
    const std::wstring root = dataRootW();
    const std::wstring config = root + L"\\config";
    const std::wstring log = root + L"\\log";
    const std::wstring image = root + L"\\image";
    const std::wstring video = root + L"\\video";
    makeDir(root);
    makeDir(config);
    makeDir(log);
    makeDir(image);
    makeDir(video);
    makeDir(log + L"\\audit");

    // 上一版直接挂在 exe 旁的 config/log/image/video → 迁进 exe名 下
    moveDirFilesIfAbsent(exe + L"\\config", config);
    moveDirFilesIfAbsent(exe + L"\\log", log);
    moveDirFilesIfAbsent(exe + L"\\log\\audit", log + L"\\audit");
    moveDirFilesIfAbsent(exe + L"\\image", image);
    moveDirFilesIfAbsent(exe + L"\\video", video);

    // exe 根下旧配置 → 数据根\\config
    static const wchar_t* kRootJson[] = {
        L"control.json", L"controlled.json", L"accounts.json", L"devices.json"
    };
    for (size_t i = 0; i < sizeof(kRootJson) / sizeof(kRootJson[0]); ++i) {
        moveIfAbsent(exe + L"\\" + kRootJson[i], config + L"\\" + kRootJson[i]);
    }

    // %APPDATA%\RemoteMaster\devices.json（仅中控台）
    if (_wcsicmp(moduleStem().c_str(), L"RemoteMaster") == 0
        || _wcsicmp(moduleStem().c_str(), L"Vision.Control") == 0) {
        const std::wstring masterRoam = roamingApp(L"RemoteMaster");
        if (!masterRoam.empty()) {
            moveIfAbsent(masterRoam + L"\\devices.json", config + L"\\devices.json");
        }
    }

    // %LOCALAPPDATA%\Vision.Remote 证书与小日志
    const std::wstring oldLocal = localAppVision();
    if (!oldLocal.empty()) {
        WIN32_FIND_DATAW find = {};
        HANDLE handle = FindFirstFileW((oldLocal + L"\\*").c_str(), &find);
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }
                const std::wstring name = find.cFileName;
                const std::wstring from = oldLocal + L"\\" + name;
                if (name.size() > 10 && name.find(L".pfx.dpapi") != std::wstring::npos) {
                    moveIfAbsent(from, config + L"\\" + name);
                } else if (name == L"tls.log"
                    || (name.size() > 10 && name.find(L"-crash.log") != std::wstring::npos)) {
                    moveIfAbsent(from, log + L"\\" + name);
                }
            } while (FindNextFileW(handle, &find));
            FindClose(handle);
        }
    }
}

}
