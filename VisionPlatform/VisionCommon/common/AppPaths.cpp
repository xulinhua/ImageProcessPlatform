/**
 * @file AppPaths.cpp
 * @brief AppPaths实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AppPaths.h"
#include "TextEncoding.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace Vision {

static std::string g_exeDir;

// 确保根目录、日志、方案目录存在
static std::string joinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) {
        return b;
    }
    const char c = a[a.size() - 1];
    if (c == '\\' || c == '/') {
        return a + b;
    }
    return a + "\\" + b;
}

// 确保根目录、日志、方案目录存在
static void mkdirOne(const std::string& p)
{
    CreateDirectoryW(utf8ToWide(p).c_str(), 0);
}

// 返回已设置的 exe 目录；未设置则从当前模块宽路径截取
void AppPaths::setExeDir(const std::string& dir)
{
    g_exeDir = dir;
}

// 返回已设置的 exe 目录；未设置则从当前模块宽路径截取
std::string AppPaths::exeDir()
{
    if (!g_exeDir.empty()) {
        return g_exeDir;
    }
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(0, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return std::string(".");
    }
    const std::string s = wideToUtf8(buf);
    const size_t slash = s.find_last_of("\\/");
    if (slash == std::string::npos) {
        return std::string(".");
    }
    return s.substr(0, slash);
}

// 数据根固定为 exe\data（内部相对 logs/solutions 不变）
std::string AppPaths::rootDir()
{
    return joinPath(exeDir(), "data");
}

// 与 rootDir 相同
std::string AppPaths::dataDir()
{
    return rootDir();
}

// 确保根目录、日志、方案目录存在
std::string AppPaths::logsDir()
{
    return joinPath(rootDir(), "logs");
}

// 确保根目录、日志、方案目录存在
std::string AppPaths::solutionsDir()
{
    return joinPath(rootDir(), "solutions");
}

std::string AppPaths::compositesDir()
{
    return joinPath(rootDir(), "composites");
}

// 本机设置 JSON 路径
std::string AppPaths::settingsFile()
{
    return joinPath(rootDir(), "settings.json");
}

// 旧的单一库，只在迁移时打开
std::string AppPaths::legacyDatabaseFile()
{
    return joinPath(rootDir(), "vision.db");
}

// 权限库
std::string AppPaths::authFile()
{
    return joinPath(rootDir(), "auth.db");
}

// 日志库
std::string AppPaths::logFile()
{
    return joinPath(rootDir(), "log.db");
}

// 生产库
std::string AppPaths::prodFile()
{
    return joinPath(rootDir(), "prod.db");
}

// 验证复核库
std::string AppPaths::verifyFile()
{
    return joinPath(rootDir(), "verify.db");
}

// 标定记录库
std::string AppPaths::calibFile()
{
    return joinPath(rootDir(), "calib.db");
}

// 标定追溯文件目录
std::string AppPaths::calibDir()
{
    return joinPath(rootDir(), "calib");
}

// 插件根目录
std::string AppPaths::pluginsDir()
{
    return joinPath(exeDir(), "plugins");
}

// 工具插件目录
std::string AppPaths::pluginsToolsDir()
{
    return joinPath(pluginsDir(), "tools");
}

// 算法插件目录
std::string AppPaths::pluginsAlgosDir()
{
    return joinPath(pluginsDir(), "algos");
}

// 创建数据、日志、方案和插件目录
void AppPaths::ensureDirs()
{
    mkdirOne(rootDir());
    mkdirOne(calibDir());
    mkdirOne(logsDir());
    mkdirOne(solutionsDir());
    mkdirOne(compositesDir());
    mkdirOne(pluginsDir());
    mkdirOne(pluginsToolsDir());
    mkdirOne(pluginsAlgosDir());
}

}