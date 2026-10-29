/**
 * @file Perm.h
 * @brief Perm头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PERMISSION_H
#define VISION_PERMISSION_H

#include "VisionAuthExport.h"
#include "Translate.h"
#include <stdint.h>
#include <string>

namespace Vision {

/** @brief 用户角色。权限表可改，角色本身不可删 */
enum UserRole {
    RoleOperator = 0, ///< 操作员
    RoleEngineer,     ///< 工程师
    RoleAdmin,        ///< 管理员
    RoleFactory       ///< 厂家。界面不列出，登录后显示等级，权限始终全开
};

/** @brief 流程工具权限按种类统一，或按流程里的实例分开 */
enum PermScope {
    PermScopeUnified = 0, ///< 同一种工具共用一套参数权限
    PermScopeInstance = 1 ///< 不同流程里的同一工具参数分开
};

/** @brief 细粒度权限位。与系统设置里的角色权限表对应 */
enum Perm {
    PermNone                 = 0,       ///< 无
    PermRuntimeView          = 1u << 0, ///< 查看运行界面
    PermRuntimeStartStop     = 1u << 1, ///< 开始/停止
    PermRuntimeResetCount    = 1u << 2, ///< 清零产量
    PermRuntimeSwitchSolution= 1u << 3, ///< 运行态切换方案
    PermEnterDesign          = 1u << 4, ///< 进入设计态
    PermEditSolution         = 1u << 5, ///< 编辑方案/工站/流程
    PermEditDevice           = 1u << 6, ///< 设备管理
    PermRunJobDebug          = 1u << 7, ///< 调试运行本工具/流程
    PermManageUsers          = 1u << 8, ///< 用户管理
    PermEditAppSettings      = 1u << 9, ///< 系统设置、控件权限
    PermViewLogs             = 1u << 10, ///< 查看操作与调试日志
    PermManageLogs           = 1u << 11, ///< 删除日志文件
    PermEditComm             = 1u << 12  ///< 通信管理
};


/**
 * @class PermPageDesc
 * @brief 权限表的一页：页面名 + 该页下的控件权限列表
 */
struct PermPageDesc {
    std::string title;       ///< 页面名
    std::string hint;        ///< 页面说明
    const Perm* items; ///< 本页控件
    int itemCount;           ///< 控件数
};


/** @brief 可配置权限条数（不含 PermNone） */
inline int permissionCount()
{
    return 13;
}

/** @brief 第 i 条权限；越界返回 PermNone */
VISION_AUTH_API Perm permissionAt(int i);

/** @brief 点分权限标识（与位枚举对照） */
VISION_AUTH_API const char* permissionId(Perm p);

/** @brief 标识 → 位；未知返回 PermNone */
VISION_AUTH_API Perm permissionFromId(const char* id);

/** @brief 权限中文标题（设置表左侧） */
VISION_AUTH_API std::string permissionTitle(Perm p);

/** @brief 权限说明（设置表提示） */
VISION_AUTH_API std::string permissionHint(Perm p);

/** @brief 页面在权限树里的节点。page|runtime 这种 */
VISION_AUTH_API std::string permPageNodeId(int page);

/** @brief 控件在权限树里的节点。page|runtime|view 这种 */
VISION_AUTH_API std::string permNodeId(Perm p);

/** @brief 没有保存过该节点时，这一角色是否默认允许 */
VISION_AUTH_API bool permDefaultOn(UserRole role, Perm p);

/** @brief 这一项不能在权限树里关掉 */
VISION_AUTH_API bool permNodeLocked(UserRole role, Perm p);

/** @brief 角色出厂权限掩码。只给旧的角色表读取，界面判断走权限树 */
VISION_AUTH_API uint32_t defaultPerms(UserRole role);

/** @brief 角色锁定位：不能在设置里关掉 */
VISION_AUTH_API uint32_t lockedPerms(UserRole role);

/**
 * @brief 结合操作员两项开关后的有效掩码
 * @param operatorReset 操作员是否允许清零
 * @param operatorSwitch 操作员是否允许切换方案
 */
VISION_AUTH_API uint32_t permissionsForRole(UserRole role, bool operatorReset, bool operatorSwitch);

/** @brief 权限页面数 */
inline int permPageCount()
{
    return 5;
}

/** @brief 第 page 个权限页面；越界返回空页 */
VISION_AUTH_API PermPageDesc permPageAt(int page);

/** @brief 旧设置没有日志位时，给工程师、管理员补上出厂日志权限 */
VISION_AUTH_API void migrateLogPermBits(uint32_t* op, uint32_t* eng, uint32_t* admin);

/** @brief 角色中文名 */
VISION_AUTH_API std::string roleTitle(UserRole role);

/** @brief 整数转角色；越界回退操作员 */
inline UserRole roleFromInt(int v)
{
    if (v == RoleAdmin || v == RoleEngineer || v == RoleFactory) {
        return static_cast<UserRole>(v);
    }
    return RoleOperator;
};

}

#endif
