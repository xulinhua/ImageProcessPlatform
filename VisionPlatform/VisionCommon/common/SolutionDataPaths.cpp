/**
 * @file SolutionDataPaths.cpp
 * @brief SolutionDataPaths实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SolutionDataPaths.h"
#include "TextEncoding.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace Vision {

// 拼接两段路径（路径串约定为 UTF-8）
std::string pathJoin(const std::string& a, const std::string& b)
{
    if (a.empty()) {
        return b;
    }
    if (b.empty()) {
        return a;
    }
    const char c = a[a.size() - 1];
    if (c == '\\' || c == '/') {
        return a + b;
    }
    return a + "\\" + b;
}

// 目录部分
std::string pathDirName(const std::string& filePath)
{
    const size_t slash = filePath.find_last_of("\\/");
    if (slash == std::string::npos) {
        return std::string();
    }
    return filePath.substr(0, slash);
}

// 无扩展名文件名
std::string pathFileStem(const std::string& filePath)
{
    std::string name = filePath;
    const size_t slash = name.find_last_of("\\/");
    if (slash != std::string::npos) {
        name = name.substr(slash + 1);
    }
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    return name;
}

// 文件是否存在（UTF-8 路径）
bool pathFileExists(const std::string& filePath)
{
    if (filePath.empty()) {
        return false;
    }
    const DWORD attr = GetFileAttributesW(utf8ToWide(filePath).c_str());
    return attr != INVALID_FILE_ATTRIBUTES
        && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// 目录是否存在（UTF-8 路径）
static bool dirExists(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    const DWORD attr = GetFileAttributesW(utf8ToWide(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES
        && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// MyLine.vpsol → MyLine.vpdata
std::string solutionDataRootFromFile(const std::string& vpsolPath)
{
    if (vpsolPath.empty()) {
        return std::string();
    }
    const std::string dir = pathDirName(vpsolPath);
    const std::string stem = pathFileStem(vpsolPath);
    if (stem.empty()) {
        return std::string();
    }
    return pathJoin(dir, stem + ".vpdata");
}

// 单工具目录
std::string toolDataDir(const std::string& vpsolPath,
                        const ObjectId& stationId,
                        const ObjectId& jobId,
                        const ObjectId& toolId)
{
    const std::string root = solutionDataRootFromFile(vpsolPath);
    if (root.empty() || toolId.isNull()) {
        return std::string();
    }
    std::string p = pathJoin(root, "stations");
    p = pathJoin(p, stationId.isNull() ? std::string("_") : stationId.toString());
    p = pathJoin(p, "jobs");
    p = pathJoin(p, jobId.isNull() ? std::string("_") : jobId.toString());
    p = pathJoin(p, "tools");
    p = pathJoin(p, toolId.toString());
    return p;
}

// 递归建目录（UTF-8 路径 → CreateDirectoryW）
bool ensureDirectoryTree(const std::string& absPath)
{
    if (absPath.empty()) {
        return false;
    }
    if (dirExists(absPath)) {
        return true;
    }
    const std::string parent = pathDirName(absPath);
    if (!parent.empty() && parent != absPath) {
        if (!ensureDirectoryTree(parent)) {
            return false;
        }
    }
    if (CreateDirectoryW(utf8ToWide(absPath).c_str(), 0)) {
        return true;
    }
    return dirExists(absPath);
}

// 拷贝单文件
static bool copyOneFile(const std::string& src, const std::string& dst)
{
    return CopyFileW(utf8ToWide(src).c_str(), utf8ToWide(dst).c_str(), FALSE) != 0;
}

// 递归拷贝
bool copyDirectoryRecursive(const std::string& src, const std::string& dst)
{
    if (!dirExists(src)) {
        return true;
    }
    if (!ensureDirectoryTree(dst)) {
        return false;
    }
    const std::wstring pattern = utf8ToWide(pathJoin(src, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return true;
    }
    bool ok = true;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }
        const std::string name = wideToUtf8(fd.cFileName);
        const std::string from = pathJoin(src, name);
        const std::string to = pathJoin(dst, name);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!copyDirectoryRecursive(from, to)) {
                ok = false;
                break;
            }
        } else {
            if (!copyOneFile(from, to)) {
                ok = false;
                break;
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}

// 删文件
bool removeFileIfExists(const std::string& filePath)
{
    if (!pathFileExists(filePath)) {
        return true;
    }
    return DeleteFileW(utf8ToWide(filePath).c_str()) != 0;
}

// 相对名解析
std::string resolveToolAssetPath(const std::string& dataDir, const std::string& relativeName)
{
    if (relativeName.empty()) {
        return std::string();
    }
    if (relativeName.size() >= 2 && relativeName[1] == ':') {
        return relativeName;
    }
    if (!relativeName.empty() && (relativeName[0] == '\\' || relativeName[0] == '/')) {
        return relativeName;
    }
    if (dataDir.empty()) {
        return relativeName;
    }
    return pathJoin(dataDir, relativeName);
}

}