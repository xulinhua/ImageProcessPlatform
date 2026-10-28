/**
 * @file AppSettings.h
 * @brief AppSettings头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_APPSETTINGS_H
#define VISION_APPSETTINGS_H
#include "VisionAuthExport.h"

#include "Perm.h"
#include <stdint.h>
#include <string>

namespace Vision {

/** @brief 启动后默认进入的主界面模式（本机偏好） */
enum StartupAppMode {
    StartupModeDesign = 0,  ///< 设计态
    StartupModeRuntime = 1  ///< 运行态
};

/** @brief 关闭主窗口时的行为（托盘项预留） */
enum CloseMainWindowAction {
    CloseActionExit = 0,         ///< 退出程序
    CloseActionMinimizeTray = 1  ///< 最小化到托盘（首版未实现，UI 灰显）
};

/**
 * @class AppSettings
 * @brief 本机设置：主题、语言、自动登录、启动/关闭偏好、流程试跑弹窗、权限掩码
 */
struct VISION_AUTH_API AppSettings {
    bool autoLoginOperator;              ///< 开机以操作员自动登录
    std::string lastLoginName;           ///< 上次登录名
    std::string lastSolutionPath;        ///< 上次成功打开或保存的方案
    std::string themeId;                 ///< 主题档：industrial / industrialLarge / industrialContrast
    std::string language;                ///< 界面语言：zh-CN / en-US
    StartupAppMode startupMode;          ///< 启动后默认模式
    CloseMainWindowAction closeAction;   ///< 关闭主窗口行为
    bool popupJobOk;                     ///< 流程试跑 OK 时弹窗
    bool popupJobNg;                     ///< 流程试跑 NG 时弹窗
    bool popupJobException;              ///< 流程试跑异常时弹窗
    uint32_t permOperator;               ///< 操作员权限掩码
    uint32_t permEngineer;               ///< 工程师权限掩码
    uint32_t permAdmin;                  ///< 管理员权限掩码

    AppSettings()
        : autoLoginOperator(false)
        , themeId("industrial")
        , language("zh-CN")
        , startupMode(StartupModeDesign)
        , closeAction(CloseActionExit)
        , popupJobOk(false)
        , popupJobNg(false)
        , popupJobException(false)
        , permOperator(defaultPerms(RoleOperator))
        , permEngineer(defaultPerms(RoleEngineer))
        , permAdmin(defaultPerms(RoleAdmin))
    {}

    /** @brief 某角色当前掩码 */
    uint32_t permMask(UserRole role) const;
    /** @brief 写某角色掩码（调用方应 OR 上 lockedPerms） */
    void setPermMask(UserRole role, uint32_t mask);
    /** @brief 三角色恢复出厂权限 */
    void resetPermsToDefault();
    /**
     * @brief 流程试跑结束后是否弹窗
     * @param ok 判定 OK
     * @param isException 运行异常（非普通 NG）
     */
    bool shouldPopupJobResult(bool ok, bool isException) const;
};

/** @brief 启动模式显示名 */
VISION_AUTH_API std::string startupAppModeTitle(StartupAppMode m);
/** @brief 关闭行为显示名 */
VISION_AUTH_API std::string closeMainWindowActionTitle(CloseMainWindowAction a);
/** @brief 整数转启动模式（越界回设计） */
VISION_AUTH_API StartupAppMode startupAppModeFromInt(int v);
/** @brief 整数转关闭行为（越界回退出） */
VISION_AUTH_API CloseMainWindowAction closeMainWindowActionFromInt(int v);

}

#endif