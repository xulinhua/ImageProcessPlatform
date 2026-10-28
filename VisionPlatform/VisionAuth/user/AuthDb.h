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
 * @brief 用户与角色权限的 SQLite 仓储。文本列为 UTF-8
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
    /** @brief 创建用户表和角色权限表 */
    bool migrateSchema(std::string* error);
};

}

#endif
