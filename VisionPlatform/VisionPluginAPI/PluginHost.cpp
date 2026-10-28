/**
 * @file PluginHost.cpp
 * @brief PluginHost实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PluginHost.h"
#include "AlgLibKind.h"
#include "AlgToolBase.h"
#include "AppPaths.h"
#include "TextEncoding.h"
#include "ToolBase.h"
#include "LogMgr.h"
#define WIN32_LEAN_AND_MEAN
#include <cstdio>
#include <map>
#include <windows.h>

namespace Vision {

// 将工具实例写入进程内 ToolRegistry（空指针忽略）
void PluginHost::addTool(ToolBase* tool)
{
    if (tool) {
        ToolRegistry::instance().add(tool);
    }
}

// 将算法实例写入 AlgToolBaseRegistry（空指针忽略）
void PluginHost::addAlg(AlgToolBase* algor)
{
    if (algor) {
        AlgToolBaseRegistry::instance().add(algor);
    }
}

// 加载期系统日志，分类固定为 plugin_load
void PluginHost::logSystem(const char* message)
{
    LogMgr::writeSystem(LogLevelInfo, "plugin_load", message ? message : "");
}

typedef int (*FnApiVersion)();              ///< 插件：VisionPlugin_apiVersion
typedef void (*FnRegister)(IPluginHost*);   ///< 插件：VisionPlugin_register

// 把 FILETIME 转成本地可读时间串，供登记「插件文件时间戳」
std::string fileTimeStamp(const FILETIME& ft)
{
    SYSTEMTIME utc;
    SYSTEMTIME local;
    if (!FileTimeToSystemTime(&ft, &utc)) {
        return std::string();
    }
    if (!SystemTimeToTzSpecificLocalTime(0, &utc, &local)) {
        local = utc;
    }
    char buf[32];
    sprintf_s(buf, "%04u-%02u-%02u %02u:%02u:%02u",
              static_cast<unsigned>(local.wYear),
              static_cast<unsigned>(local.wMonth),
              static_cast<unsigned>(local.wDay),
              static_cast<unsigned>(local.wHour),
              static_cast<unsigned>(local.wMinute),
              static_cast<unsigned>(local.wSecond));
    return std::string(buf);
}

namespace {

// 枚举目录下 *.dll：校验 apiVersion==1 后调用 register；成功加载的不卸载
int loadDir(const std::string& dir, IPluginHost* host)
{
    int n = 0;
    const std::wstring pat = utf8ToWide(dir + "\\*.dll");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        const std::string name = wideToUtf8(fd.cFileName);
        // 只加载当前命名的插件，跳过目录里残留的旧文件，避免重复登记时按新基类释放旧对象
        if (name.size() < 7 || name.compare(0, 7, "Plugin.") != 0) {
            continue;
        }
        const std::wstring full = utf8ToWide(dir + "\\" + name);
        HMODULE mod = LoadLibraryW(full.c_str());
        if (!mod) {
            LogMgr::writeSystem(LogLevelWarn, "plugin_load", name + " LoadLibrary failed");
            continue;
        }
        FnApiVersion ver = reinterpret_cast<FnApiVersion>(GetProcAddress(mod, "VisionPlugin_apiVersion"));
        FnRegister reg = reinterpret_cast<FnRegister>(GetProcAddress(mod, "VisionPlugin_register"));
        // 缺符号或版本不是 1：跳过（模块仍留在进程中，避免反复加载）
        if (!ver || !reg || ver() != 1) {
            LogMgr::writeSystem(LogLevelWarn, "plugin_load", name + " api mismatch");
            continue;
        }
        // 登记期内把 DLL 文件时间写入注册表侧，便于版本对话框展示
        const std::string stamp = fileTimeStamp(fd.ftLastWriteTime);
        ToolRegistry::instance().setIncomingPluginStamp(stamp);
        reg(host);
        ToolRegistry::instance().setIncomingPluginStamp(std::string());
        ++n;
        LogMgr::writeSystem(LogLevelInfo, "plugin_load", name);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

} // namespace

// 先保证目录存在，再扫 tools、algos 两个插件目录并合计成功数
int loadVisionPlugins(IPluginHost* host)
{
    if (!host) {
        host = &ToolLibMgr::instance();
    }
    return ToolLibMgr::instance().loadAll();
}

struct ToolLibMgr::Impl {
    std::map<std::string, ToolLibItem> items; ///< 已加载 DLL
    std::vector<ToolLibReg> regs;             ///< 登记项
    std::string loadingFile;                  ///< 正在 register 的 DLL
    std::string loadingKind;                  ///< tool 或 algo
    std::string loadingStamp;                 ///< 该 DLL 文件时间
};

ToolLibMgr& ToolLibMgr::instance()
{
    static ToolLibMgr mgr;
    return mgr;
}

ToolLibMgr::ToolLibMgr()
    : m_impl(new Impl())
{
}

void ToolLibMgr::addTool(ToolBase* tool)
{
    if (tool && m_impl) {
        ToolLibReg rec;
        rec.fileName = m_impl->loadingFile;
        rec.kind = "tool";
        rec.displayName = tool->displayName();
        rec.toolName = tool->toolName();
        rec.version = tool->version();
        rec.notes = tool->versionNotes();
        rec.description = tool->description();
        rec.stamp = m_impl->loadingStamp;
        m_impl->regs.push_back(rec);
    }
    PluginHost host;
    host.addTool(tool);
}

void ToolLibMgr::addAlg(AlgToolBase* algor)
{
    if (algor && m_impl) {
        ToolLibReg rec;
        rec.fileName = m_impl->loadingFile;
        rec.kind = "algo";
        rec.displayName = algor->toolName();
        rec.toolName = algor->toolName();
        rec.algLib = algLibKindTitle(algor->algLib());
        rec.version = algor->version();
        rec.notes = algor->versionNotes();
        rec.stamp = m_impl->loadingStamp;
        m_impl->regs.push_back(rec);
    }
    PluginHost host;
    host.addAlg(algor);
}

void ToolLibMgr::logSystem(const char* message)
{
    PluginHost host;
    host.logSystem(message);
}

std::vector<ToolLibItem> ToolLibMgr::loaded() const
{
    std::vector<ToolLibItem> out;
    if (!m_impl) {
        return out;
    }
    for (std::map<std::string, ToolLibItem>::const_iterator it = m_impl->items.begin();
         it != m_impl->items.end(); ++it) {
        out.push_back(it->second);
    }
    return out;
}

// 已登记项
std::vector<ToolLibReg> ToolLibMgr::registrations() const
{
    if (!m_impl) {
        return std::vector<ToolLibReg>();
    }
    return m_impl->regs;
}

int ToolLibMgr::loadAll()
{
    if (!m_impl) {
        return 0;
    }
    AppPaths::ensureDirs();
    const std::string dirs[2] = { AppPaths::pluginsToolsDir(), AppPaths::pluginsAlgosDir() };
    const char* kinds[2] = { "tool", "algo" };
    int n = 0;
    for (int d = 0; d < 2; ++d) {
        const std::wstring pat = utf8ToWide(dirs[d] + "\\*.dll");
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pat.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            const std::string name = wideToUtf8(fd.cFileName);
            if (name.size() < 7 || name.compare(0, 7, "Plugin.") != 0) {
                continue;
            }
            const std::string key = kinds[d] + std::string(":") + name;
            if (m_impl->items.find(key) != m_impl->items.end()) {
                continue;
            }
            const std::wstring full = utf8ToWide(dirs[d] + "\\" + name);
            HMODULE mod = LoadLibraryW(full.c_str());
            if (!mod) {
                LogMgr::writeSystem(LogLevelWarn, "plugin_load", name + " LoadLibrary failed");
                continue;
            }
            FnApiVersion ver = reinterpret_cast<FnApiVersion>(GetProcAddress(mod, "VisionPlugin_apiVersion"));
            FnRegister reg = reinterpret_cast<FnRegister>(GetProcAddress(mod, "VisionPlugin_register"));
            if (!ver || !reg || ver() != 1) {
                LogMgr::writeSystem(LogLevelWarn, "plugin_load", name + " api mismatch");
                continue;
            }
            const std::string stamp = fileTimeStamp(fd.ftLastWriteTime);
            m_impl->loadingFile = name;
            m_impl->loadingKind = kinds[d];
            m_impl->loadingStamp = stamp;
            ToolRegistry::instance().setIncomingPluginStamp(stamp);
            reg(this);
            ToolRegistry::instance().setIncomingPluginStamp(std::string());
            m_impl->loadingFile.clear();
            m_impl->loadingKind.clear();
            m_impl->loadingStamp.clear();
            ToolLibItem item;
            item.fileName = name;
            item.kind = kinds[d];
            item.stamp = stamp;
            m_impl->items[key] = item;
            ++n;
            LogMgr::writeSystem(LogLevelInfo, "plugin_load", name);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return n;
}

}
