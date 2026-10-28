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

// 该角色可配置掩码再并上锁定权限
uint32_t AppSettings::permMask(UserRole role) const
{
    switch (role) {
    case RoleAdmin:
        return permAdmin | lockedPerms(RoleAdmin);
    case RoleEngineer:
        return permEngineer | lockedPerms(RoleEngineer);
    default:
        return permOperator | lockedPerms(RoleOperator);
    }
}

// 写入掩码时强制保留锁定权限
void AppSettings::setPermMask(UserRole role, uint32_t mask)
{
    mask |= lockedPerms(role);
    switch (role) {
    case RoleAdmin:
        permAdmin = mask;
        break;
    case RoleEngineer:
        permEngineer = mask;
        break;
    default:
        permOperator = mask;
        break;
    }
}

// 三角色权限恢复出厂默认
void AppSettings::resetPermsToDefault()
{
    permOperator = defaultPerms(RoleOperator);
    permEngineer = defaultPerms(RoleEngineer);
    permAdmin = defaultPerms(RoleAdmin);
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