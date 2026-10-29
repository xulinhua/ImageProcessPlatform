/**
 * @file AuthDb.h
 * @brief AuthDb头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_AUTHDB_H
#define VISION_AUTHDB_H
#include "VisionAuthExport.h"

#include "AuthMgr.h"
#include "AppSettings.h"

namespace Vision {

/**
 * @class AuthDb
 * @brief 权限库。users 只放账号，perm 按角色和权限各一行
 */
class AuthDb {
public:
    /** @brief 进程内唯一用户库 */
    static AuthDb& instance();
    /** @brief 打开数据库并升级表结构 */
    bool open(std::string* error);
    /** @brief 关闭连接 */
    void close();
    /** @brief 读出全部用户到内存库 */
    bool loadUsers(AuthMgr* users, std::string* error);
    /** @brief 清空后整表写回用户 */
    bool saveUsers(const AuthMgr& users, std::string* error);
    /** @brief 读某角色的权限掩码 */
    bool loadRoleMask(UserRole role, uint32_t* mask, std::string* error);
    /** @brief 写某角色的权限掩码 */
    bool saveRoleMask(UserRole role, uint32_t mask, std::string* error);

private:
    AuthDb() {}
    /** @brief 创建 users 和 perm，并把旧的整型掩码拆成权限行 */
    bool migrateSchema(std::string* error);
    /** @brief 权限库还是空的时候，从旧 vision.db 搬用户和权限 */
    bool importLegacy(std::string* error);
    /** @brief 按掩码重写某角色在 perm 里的全部行。调用方负责事务 */
    bool writeRolePerms(int role, uint32_t mask, std::string* error);
};

}

#endif
