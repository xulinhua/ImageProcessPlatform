/**
 * @file Perm.cpp
 * @brief Perm实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Perm.h"

namespace Vision {

// 第 i 条权限；越界返回 PermNone
Perm permissionAt(int i)
{
    static const Perm k[] = {
        PermRuntimeView,
        PermRuntimeStartStop,
        PermRuntimeResetCount,
        PermRuntimeSwitchSolution,
        PermEnterDesign,
        PermEditSolution,
        PermEditDevice,
        PermRunJobDebug,
        PermManageUsers,
        PermEditAppSettings,
        PermViewLogs,
        PermManageLogs,
        PermEditComm
    };
    if (i < 0 || i >= 13) {
        return PermNone;
    }
    return k[i];
}

// 点分权限标识（与位枚举对照）
const char* permissionId(Perm p)
{
    switch (p) {
    case PermRuntimeView:           return "runtime.view";
    case PermRuntimeStartStop:      return "runtime.start_stop";
    case PermRuntimeResetCount:     return "runtime.reset_count";
    case PermRuntimeSwitchSolution: return "runtime.switch_solution";
    case PermEnterDesign:           return "design.enter";
    case PermEditSolution:          return "design.edit_solution";
    case PermEditDevice:            return "device.edit";
    case PermEditComm:              return "comm.edit";
    case PermRunJobDebug:           return "design.run_debug";
    case PermManageUsers:           return "user.manage";
    case PermEditAppSettings:       return "app.settings";
    case PermViewLogs:              return "log.view";
    case PermManageLogs:            return "log.manage";
    default:                        return "";
    }
}

// 标识 → 位；未知返回 PermNone
Perm permissionFromId(const char* id)
{
    if (!id || !*id) {
        return PermNone;
    }
    for (int i = 0; i < permissionCount(); ++i) {
        const Perm p = permissionAt(i);
        if (std::string(permissionId(p)) == id) {
            return p;
        }
    }
    return PermNone;
}

// 权限中文标题（设置表左侧）
std::string permissionTitle(Perm p)
{
    switch (p) {
    case PermRuntimeView:           return Translate::tr("查看运行界面");
    case PermRuntimeStartStop:      return Translate::tr("开始 / 停止");
    case PermRuntimeResetCount:     return Translate::tr("清零产量");
    case PermRuntimeSwitchSolution: return Translate::tr("运行态切换方案");
    case PermEnterDesign:           return Translate::tr("进入设计态");
    case PermEditSolution:          return Translate::tr("编辑方案 / 工站 / 流程");
    case PermEditDevice:            return Translate::tr("设备管理");
    case PermEditComm:              return Translate::tr("通信管理");
    case PermRunJobDebug:           return Translate::tr("调试运行本工具 / 流程");
    case PermManageUsers:           return Translate::tr("用户管理");
    case PermEditAppSettings:       return Translate::tr("系统设置、控件权限");
    case PermViewLogs:              return Translate::tr("查看日志");
    case PermManageLogs:            return Translate::tr("日志管理");
    default:                        return "";
    }
}

// 权限说明（设置表提示）
std::string permissionHint(Perm p)
{
    switch (p) {
    case PermRuntimeView:           return Translate::tr("运行页图像、统计、结果历史");
    case PermRuntimeStartStop:      return Translate::tr("左侧开始、停止按钮与菜单");
    case PermRuntimeResetCount:     return Translate::tr("清零按钮与菜单");
    case PermRuntimeSwitchSolution: return Translate::tr("运行页「切换方案」");
    case PermEnterDesign:           return Translate::tr("顶栏切到设计；无权限时弹登录提权");
    case PermEditSolution:          return Translate::tr("改工站、流程、工具、ROI");
    case PermEditDevice:            return Translate::tr("设备台账与连接参数");
    case PermEditComm:              return Translate::tr("串口 / TCP / UDP / MODBUS 通道");
    case PermRunJobDebug:           return Translate::tr("设计态「运行本工具 / 本流程」");
    case PermManageUsers:           return Translate::tr("增减用户、改角色、重置口令");
    case PermEditAppSettings:       return Translate::tr("开机自动登录、角色权限表");
    case PermViewLogs:              return Translate::tr("打开日志管控，查看操作与调试记录");
    case PermManageLogs:            return Translate::tr("按日删除操作日志、调试日志文件");
    default:                        return "";
    }
}

// 角色出厂权限掩码
uint32_t defaultPerms(UserRole role)
{
    uint32_t p = PermRuntimeView | PermRuntimeStartStop;
    if (role == RoleOperator) {
        return p;
    }
    p |= PermRuntimeResetCount | PermRuntimeSwitchSolution | PermEnterDesign
         | PermEditSolution | PermEditDevice | PermEditComm | PermRunJobDebug | PermViewLogs;
    if (role == RoleAdmin) {
        p |= PermManageUsers | PermEditAppSettings | PermManageLogs;
    }
    return p;
}

// 角色锁定位：不能在设置里关掉
uint32_t lockedPerms(UserRole role)
{
    uint32_t p = PermRuntimeView;
    if (role == RoleAdmin) {
        p |= PermManageUsers | PermEditAppSettings | PermViewLogs | PermManageLogs;
    }
    return p;
}

// 结合操作员两项开关后的有效掩码
uint32_t permissionsForRole(UserRole role, bool operatorReset, bool operatorSwitch)
{
    uint32_t p = defaultPerms(role);
    if (role == RoleOperator) {
        if (operatorReset) {
            p |= PermRuntimeResetCount;
        }
        if (operatorSwitch) {
            p |= PermRuntimeSwitchSolution;
        }
    }
    return p;
}

// 第 page 个权限页面；越界返回空页
PermPageDesc permPageAt(int page)
{
    static const Perm kRuntime[] = {
        PermRuntimeView, PermRuntimeStartStop, PermRuntimeResetCount, PermRuntimeSwitchSolution
    };
    static const Perm kDesign[] = {
        PermEnterDesign, PermEditSolution, PermRunJobDebug
    };
    static const Perm kDevice[] = {
        PermEditDevice, PermEditComm
    };
    static const Perm kSystem[] = {
        PermManageUsers, PermEditAppSettings
    };
    static const Perm kLog[] = {
        PermViewLogs, PermManageLogs
    };
    PermPageDesc d;
    d.title = "";
    d.hint = "";
    d.items = 0;
    d.itemCount = 0;
    if (page == 0) {
        d.title = Translate::tr("运行页");
        d.hint = Translate::tr("运行态画面与按钮");
        d.items = kRuntime;
        d.itemCount = 4;
    } else if (page == 1) {
        d.title = Translate::tr("设计页");
        d.hint = Translate::tr("配方编辑与调试运行");
        d.items = kDesign;
        d.itemCount = 3;
    } else if (page == 2) {
        d.title = Translate::tr("设备页");
        d.hint = Translate::tr("设备台账");
        d.items = kDevice;
        d.itemCount = 2;
    } else if (page == 3) {
        d.title = Translate::tr("系统页");
        d.hint = Translate::tr("用户与本机设置");
        d.items = kSystem;
        d.itemCount = 2;
    } else if (page == 4) {
        d.title = Translate::tr("日志页");
        d.hint = Translate::tr("操作日志与调试日志");
        d.items = kLog;
        d.itemCount = 2;
    }
    return d;
}

// 旧设置没有日志位时，给工程师、管理员补上出厂日志权限
void migrateLogPermBits(uint32_t* op, uint32_t* eng, uint32_t* admin)
{
    if (!op || !eng || !admin) {
        return;
    }
    const uint32_t logBits = static_cast<uint32_t>(PermViewLogs) | static_cast<uint32_t>(PermManageLogs);
    if (((*op | *eng | *admin) & logBits) == 0) {
        *eng |= static_cast<uint32_t>(PermViewLogs);
        *admin |= logBits;
    }
}

// 角色中文名
std::string roleTitle(UserRole role)
{
    switch (role) {
    case RoleAdmin:    return Translate::tr("管理员");
    case RoleEngineer: return Translate::tr("工程师");
    case RoleOperator: return Translate::tr("操作员");
    default:           return Translate::tr("操作员");
    }
}

}
