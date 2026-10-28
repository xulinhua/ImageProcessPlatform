/**
 * @file StationService.cpp
 * @brief 工站服务主循环
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StationService.h"
#include "AppPaths.h"
#include "PluginHost.h"
#include "TextEncoding.h"
#include "SharedFrames.h"
#include "Solution.h"
#include "ThreadPool.h"
#include "VisionCfgMgr.h"
#include "SoftLicense.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Vision {

StationService::StationService()
    : m_sol(0)
    , m_stop(false)
    , m_licenseTicks(0)
{
}

StationService::~StationService()
{
    m_host.stop();
    delete m_sol;
    m_sol = 0;
}

bool StationService::loadContext(std::string* err)
{
    AppPaths::ensureDirs();
    if (!m_node.load(err)) {
        return false;
    }
    if (!m_node.stationEnabled) {
        if (err) {
            *err = "本机 services.station 是 disabled";
        }
        return false;
    }
    const AppSettings settings = VisionCfgMgr::loadSettings();
    if (settings.lastSolutionPath.empty()) {
        if (err) {
            *err = "还没有打开过方案";
        }
        return false;
    }
    m_sol = new Solution();
    if (!VisionCfgMgr::loadSolution(settings.lastSolutionPath, m_sol, err)) {
        return false;
    }
    m_deploy.loadBeside(settings.lastSolutionPath, err);
    return true;
}

bool StationService::licenseAllowsRun()
{
    SoftLicense license;
    const LicenseReason reason = license.poll();
    if (reason == LicenseOk) {
        return true;
    }
    if (m_host.running()) {
        m_host.stop();
    }
    SharedFrames::instance().setFault(IpcFaultRejected, license.reasonText(reason));
    return false;
}

void StationService::handle(IpcCommand cmd)
{
    if (cmd == IpcCmdStartOwned) {
        if (!licenseAllowsRun()) {
            return;
        }
        m_host.startOwned();
    } else if (cmd == IpcCmdStopOwned) {
        m_host.stop();
    } else if (cmd == IpcCmdReload) {
        if (m_host.running()) {
            SharedFrames::instance().setFault(IpcFaultRejected, "工站正在运行，无法重新加载方案");
            return;
        }
        std::string err;
        delete m_sol;
        m_sol = 0;
        if (!loadContext(&err)) {
            SharedFrames::instance().setFault(IpcFaultRejected, err);
            return;
        }
        m_host.bind(m_sol, &m_node, &m_deploy, &ThreadPool::shared());
    }
}

int StationService::run()
{
    {
        wchar_t path[MAX_PATH];
        const DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            std::wstring ws(path, n);
            const size_t slash = ws.find_last_of(L"\\/");
            if (slash != std::wstring::npos) {
                AppPaths::setExeDir(wideToUtf8(ws.substr(0, slash).c_str()));
            }
        }
    }
    static PluginHost host;
    loadVisionPlugins(&host);
    std::string err;
    if (!loadContext(&err)) {
        return 1;
    }
    SharedFrames& frames = SharedFrames::instance();
    if (!frames.map(m_node.previewSlotBytes, &err)) {
        return 1;
    }
    frames.setRole(m_node.role);
    m_host.bind(m_sol, &m_node, &m_deploy, &ThreadPool::shared());
    while (!m_stop) {
        frames.beatStation();
        ++m_licenseTicks;
        if (m_host.running() && m_licenseTicks >= 30) {
            m_licenseTicks = 0;
            licenseAllowsRun();
        }
        const IpcCommand cmd = frames.waitCommand(1000, 0, 0);
        if (cmd != IpcCmdNone) {
            handle(cmd);
        }
    }
    m_host.stop();
    return 0;
}

}
