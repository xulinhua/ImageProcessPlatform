/**
 * @file Session.h
 * @brief Session头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SESSION_H
#define VISION_SESSION_H
#include "VisionAuthExport.h"

#include "AppSettings.h"
#include "Perm.h"
#include "AuthMgr.h"
#include <stdint.h>
#include <string>

namespace Vision {

/**
 * @class ISessionListener
 * @brief 会话变化回调：当前用户或界面语言变了通知界面
 */
class ISessionListener {
public:
    virtual ~ISessionListener() {}
    /** @brief 当前用户或权限变了 */
    virtual void onUserChanged() = 0;
    virtual void onUiLocaleChanged() {}
};

/**
 * @class Session
 * @brief 当前登录用户、权限掩码，以及临时提权
 */
class VISION_AUTH_API Session {
public:
    /** @brief 准备目录、内置权限和出厂用户 */
    Session();
    /** @brief 进程内唯一会话。用户、权限都从这里取 */
    static Session& instance();

    /** @brief 设置界面回调，可空 */
    void setListener(ISessionListener* listener);

    AuthMgr& users();
    const AuthMgr& users() const;
    AppSettings& settings();
    const AppSettings& settings() const;

    bool isLoggedIn() const;
    const UserAccount& user() const;
    UserRole role() const;
    uint32_t perms() const;
    /** @brief 当前掩码是否包含该点分权限标识 */
    bool has(const std::string& permId) const;
    /** @brief 当前掩码是否包含该权限位 */
    bool has(Perm p) const;

    /** @brief 校验口令后进入；失败写 error */
    bool login(const std::string& loginName, const std::string& password, std::string* error);
    /** @brief 不校验口令，直接以该启用用户进入 */
    bool enterAs(const std::string& loginName);
    /** @brief 开发态进入管理员，并把角色权限恢复出厂 */
    bool enterAdminForDev();
    /** @brief 三角色权限恢复出厂并重新套用 */
    void resetDevPerms();
    /**
     * @brief 校验口令后临时换成该用户
     * @param neededPerm 必须具备的权限标识；空则只验口令
     */
    bool elevate(const std::string& loginName, const std::string& password,
                 const std::string& neededPerm, std::string* error);
    /** @brief 结束提权，回到提权前的用户 */
    void dropElevate();
    bool isElevated() const;
    /** @brief 把内存用户库写回数据库 */
    bool persistUsers();
    /** @brief 设置允许时，以操作员账户进入 */
    bool autoLoginOperator();
    /** @brief 清除当前用户并通知界面 */
    void logout();
    /** @brief 按角色从库刷新掩码，并并上不能关掉的权限 */
    void reloadPerms();
    /** @brief 用给定账户替换当前用户（不含口令哈希） */
    void touchUser(const UserAccount& u);
    /** @brief 按 id 从用户库重新套用当前用户 */
    void refreshCurrentUser();
    /** @brief 通知界面刷新当前用户 */
    void notifyChanged();
    /** @brief 通知界面语言已切换 */
    void notifyUiLocaleChanged();

private:
    /** @brief 写入当前用户、刷新掩码并回调 */
    void applyUser(const UserAccount& u);

    ISessionListener* m_listener; ///< 界面回调，可空
    AuthMgr m_users;            ///< 内存用户库
    AppSettings m_settings;       ///< 本机设置
    UserAccount m_user;           ///< 当前登录用户
    uint32_t m_perms;             ///< 当前权限掩码
    UserAccount m_savedUser;      ///< 提权前的用户
    uint32_t m_savedPerms;        ///< 提权前的掩码
    bool m_elevated;              ///< 是否处于提权
};

}

#endif
