/**
 * @file AppSettings.cpp
 * @brief AppSettings实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AppSettings.h"
#include "Translate.h"

namespace Vision {

void AppSettings::resetPermsToDefault()
{
    permScope = PermScopeUnified;
    permNodes.clear();
}

bool AppSettings::nodeGranted(UserRole role, const std::string& id, bool* found) const
{
    if (role == RoleFactory) {
        if (found) {
            *found = true;
        }
        return true;
    }
    for (size_t i = 0; i < permNodes.size(); ++i) {
        if (permNodes[i].id != id) {
            continue;
        }
        if (found) {
            *found = true;
        }
        for (int n = 0; n < permissionCount(); ++n) {
            const Perm perm = permissionAt(n);
            if (permNodeId(perm) == id && permNodeLocked(role, perm)) {
                return true;
            }
        }
        if (role == RoleAdmin) {
            return permNodes[i].admin;
        }
        if (role == RoleEngineer) {
            return permNodes[i].eng;
        }
        return permNodes[i].op;
    }
    if (found) {
        *found = false;
    }
    for (int n = 0; n < permissionCount(); ++n) {
        const Perm perm = permissionAt(n);
        if (permNodeId(perm) == id) {
            return permDefaultOn(role, perm);
        }
    }
    return true;
}

void AppSettings::setNodeGrant(const std::string& id, bool op, bool eng, bool admin)
{
    for (size_t i = 0; i < permNodes.size(); ++i) {
        if (permNodes[i].id == id) {
            permNodes[i].op = op;
            permNodes[i].eng = eng;
            permNodes[i].admin = admin;
            return;
        }
    }
    PermNodeGrant g;
    g.id = id;
    g.op = op;
    g.eng = eng;
    g.admin = admin;
    permNodes.push_back(g);
}

void AppSettings::clearNodeGrants()
{
    permNodes.clear();
}

void AppSettings::importLegacyMasks(uint32_t op, uint32_t eng, uint32_t admin)
{
    for (size_t i = 0; i < permNodes.size(); ++i) {
        if (permNodes[i].id.compare(0, 5, "page|") == 0) {
            return;
        }
    }
    for (int i = 0; i < permissionCount(); ++i) {
        const Perm perm = permissionAt(i);
        const uint32_t bit = static_cast<uint32_t>(perm);
        setNodeGrant(permNodeId(perm), (op & bit) != 0, (eng & bit) != 0, (admin & bit) != 0);
    }
    for (int page = 0; page < permPageCount(); ++page) {
        const PermPageDesc desc = permPageAt(page);
        bool opOn = false;
        bool engOn = false;
        bool adminOn = false;
        for (int i = 0; i < desc.itemCount; ++i) {
            const uint32_t bit = static_cast<uint32_t>(desc.items[i]);
            opOn = opOn || (op & bit) != 0;
            engOn = engOn || (eng & bit) != 0;
            adminOn = adminOn || (admin & bit) != 0;
        }
        setNodeGrant(permPageNodeId(page), opOn, engOn, adminOn);
    }
}

// 流程试跑结果是否弹窗
bool AppSettings::shouldPopupJobResult(bool ok, bool isException) const
{
    if (isException) {
        return popupJobException;
    }
    if (ok) {
        return popupJobOk;
    }
    return popupJobNg;
}

// 启动模式显示名
std::string startupAppModeTitle(StartupAppMode m)
{
    switch (m) {
    case StartupModeRuntime:
        return Translate::tr("运行");
    case StartupModeDesign:
    default:
        return Translate::tr("设计");
    }
}

// 关闭行为显示名
std::string closeMainWindowActionTitle(CloseMainWindowAction a)
{
    switch (a) {
    case CloseActionMinimizeTray:
        return Translate::tr("最小化到托盘（后续版本）");
    case CloseActionExit:
    default:
        return Translate::tr("退出程序");
    }
}

// 整数转启动模式，越界回设计态
StartupAppMode startupAppModeFromInt(int v)
{
    if (v == StartupModeRuntime) {
        return StartupModeRuntime;
    }
    return StartupModeDesign;
}

// 整数转关闭行为，越界回退出
CloseMainWindowAction closeMainWindowActionFromInt(int v)
{
    if (v == CloseActionMinimizeTray) {
        return CloseActionMinimizeTray;
    }
    return CloseActionExit;
}

}