/**
 * @file IRemoteAuth.h
 * @brief 可选认证插件。检测平台实现这个接口，远程控制不链接平台库
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_IREMOTE_AUTH_H
#define VISION_REMOTE_IREMOTE_AUTH_H

#include <stdint.h>

namespace Vision {

/** @brief 插件登录结果。perms 用 Messages.h 里的 Perm* 位 */
struct RemoteAuthResult {
    char user[64];              ///< 登录成功后的账号，空则调用方用输入的账号
    char role[32];              ///< 角色名。viewer 只看
    uint32_t perms;             ///< Perm* 位，0 时调用方按只看处理
    uint8_t highestPrivilege;   ///< 1 开 0 关；未填时默认 1
    RemoteAuthResult() : user(), role(), perms(1), highestPrivilege(1) {}
};

/**
 * @class IRemoteAuth
 * @brief 平台账号插件
 * @note DLL 导出 CreateRemoteAuth，返回的对象由插件分配，调用方用 DestroyRemoteAuth 交还
 */
class IRemoteAuth {
public:
    virtual ~IRemoteAuth() {}
    /** @brief 校验账号。成功时填写 role 与 perms */
    virtual bool login(const char* user, const char* password, RemoteAuthResult* out) = 0;
};

/** @brief 插件导出。返回的对象由插件分配 */
typedef IRemoteAuth* (*CreateRemoteAuthFn)();
/** @brief 交还 CreateRemoteAuth 返回的对象 */
typedef void (*DestroyRemoteAuthFn)(IRemoteAuth*);

}

#endif
