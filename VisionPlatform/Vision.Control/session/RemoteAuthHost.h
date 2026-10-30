/**
 * @file RemoteAuthHost.h
 * @brief 加载可选的 IRemoteAuth 插件；没有插件时读 exe 旁的 accounts.json
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_REMOTE_AUTH_HOST_H
#define VISION_CONTROL_REMOTE_AUTH_HOST_H

#include "IRemoteAuth.h"
#include <string>

namespace Vision {

/**
 * @class RemoteAuthHost
 * @brief 登录入口。插件和账号文件都没有时 loginRequired 为 false，沿用现有无人值守方式
 */
class RemoteAuthHost {
public:
    /** @brief 尚未加载插件 */
    RemoteAuthHost();
    /** @brief 交还插件对象并卸载 DLL */
    ~RemoteAuthHost();

    /** @brief 尝试加载 RemoteAuth.dll，再看 accounts.json */
    void load();
    /** @brief 需要在进入主界面前登录 */
    bool loginRequired() const;
    /** @brief 校验账号。成功后 user() / perms() 可用 */
    bool login(const std::string& user, const std::string& password);
    /** @brief 最近一次登录成功的账号。未登录为空 */
    const std::string& user() const;
    /** @brief 角色名。未登录为空 */
    const std::string& role() const;
    /** @brief 权限位。未登录为只看 */
    uint32_t perms() const;

private:
    /** @brief 用 exe 旁的 accounts.json 校验。密码是明文对照 */
    bool loginFile(const std::string& user, const std::string& password);

    void* m_dll;                        ///< RemoteAuth.dll，没有插件时为空
    IRemoteAuth* m_plugin;              ///< 插件对象
    DestroyRemoteAuthFn m_destroy;      ///< 交还插件对象
    bool m_file;                        ///< 存在 accounts.json
    std::string m_user;                 ///< 当前账号
    std::string m_role;                 ///< 当前角色
    uint32_t m_perms;                   ///< 当前权限位
};

}

#endif
