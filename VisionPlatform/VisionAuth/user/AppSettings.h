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
#include <vector>

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
    int permScope;                       ///< PermScope。只有厂家能改

    /** @brief 工具或实例参数的一条勾选。三种角色各一位，厂家不记 */
    struct PermNodeGrant {
        std::string id; ///< 节点键
        bool op;        ///< 操作员
        bool eng;       ///< 工程师
        bool admin;     ///< 管理员
        /** @brief 三种角色都允许 */
        PermNodeGrant() : op(true), eng(true), admin(true) {}
    };
    std::vector<PermNodeGrant> permNodes; ///< 记在本机设置里，不进方案

    /** @brief 出厂设置：工业主题、简体中文、设计模式启动、关窗即退出、不弹窗、权限统一 */
    AppSettings()
        : autoLoginOperator(false)
        , themeId("industrial")
        , language("zh-CN")
        , startupMode(StartupModeDesign)
        , closeAction(CloseActionExit)
        , popupJobOk(false)
        , popupJobNg(false)
        , popupJobException(false)
        , permScope(PermScopeUnified)
    {}

    /** @brief 清掉树上的勾选，页面项回到内置默认 */
    void resetPermsToDefault();
    /** @brief 读节点。页面项没有记录时用内置默认。工具项没有记录时允许。厂家始终允许 */
    bool nodeGranted(UserRole role, const std::string& id, bool* found) const;
    /** @brief 写入或覆盖一条节点 */
    void setNodeGrant(const std::string& id, bool op, bool eng, bool admin);
    /** @brief 清掉全部节点 */
    void clearNodeGrants();
    /** @brief 把旧的三位掩码转成树。已经有页面节点时不覆盖 */
    void importLegacyMasks(uint32_t op, uint32_t eng, uint32_t admin);
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