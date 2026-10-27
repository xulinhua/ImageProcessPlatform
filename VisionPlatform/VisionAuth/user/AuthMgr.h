/**
 * @file AuthMgr.h
 * @brief AuthMgr头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_USERSTORE_H
#define VISION_USERSTORE_H
#include "VisionAuthExport.h"

#include "Perm.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class UserAccount
 * @brief 内存中的用户。口令只存 salt + SHA256，窗口在 VisionUi
 */
struct UserAccount {
    std::string id;                      ///< 用户 id
    std::string loginName;               ///< 登录名
    std::string displayName;             ///< 显示名
    UserRole role;                       ///< 角色
    std::vector<unsigned char> salt;     ///< 口令盐
    std::vector<unsigned char> pwdHash;  ///< 口令哈希
    int hashKind;                        ///< 0=SHA256，1=PBKDF2
    int failCount;                       ///< 连续失败次数
    std::string lockUntil;               ///< 锁定截止 ISO；空=未锁
    bool enabled;                        ///< 是否允许登录
    bool mustChangePwd;                  ///< 兼容旧用户库，界面不再使用
    std::string createdAt;               ///< 创建时间文本
    std::string lastLoginAt;             ///< 上次登录时间文本

    UserAccount()
        : role(RoleOperator)
        , hashKind(0)
        , failCount(0)
        , enabled(true)
        , mustChangePwd(false)
    {
    } ///< 默认操作员、无强制改密
};

/**
 * @class AuthMgr
 * @brief 内存用户库。持久化走 AuthDb，本类不直接访问数据库
 */
class VISION_AUTH_API AuthMgr {
public:
    /** @brief 空库时写入出厂 admin/engineer/operator */
    bool ensureDefaults();
    /** @brief 将 admin/engineer/operator 口令重置为空；有变更返回 true */
    bool ensureBuiltinEmptyPasswords();
    /** @brief 清空全部用户 */
    void clear();

    /** @brief 人数 */
    int count() const;
    /** @brief 按下标取（调用方保证范围） */
    const UserAccount& at(int i) const;
    /** @brief 按 id 查找 */
    UserAccount* findById(const std::string& id);
    /** @brief 按登录名查找 */
    UserAccount* findByLogin(const std::string& loginName);
    /** @brief 按登录名查找（只读） */
    const UserAccount* findByLogin(const std::string& loginName) const;
    /** @brief 找第一个指定角色的用户；enabledOnly 为 true 时跳过禁用账户 */
    UserAccount* findFirstByRole(UserRole role, bool enabledOnly);
    /** @brief 保证至少有一个启用的管理员账户 */
    bool ensureAdminAccount();

    /** @brief 追加用户；登录名冲突失败 */
    bool addUser(const UserAccount& u);
    /** @brief 按 id 删除 */
    bool removeById(const std::string& id);
    /** @brief 写新口令哈希（空口令表示无密码） */
    bool setPassword(UserAccount* u, const std::string& password);
    /** @brief 校验口令 */
    bool verifyPassword(const UserAccount& u, const std::string& password) const;
    /** @brief 统计某角色人数 */
    int countRole(UserRole role, bool enabledOnly) const;

    /** @brief SHA256(salt || utf8 口令) */
    static std::vector<unsigned char> hashPassword(const std::vector<unsigned char>& salt, const std::string& password);
    /** @brief 构造带哈希的新用户 */
    static UserAccount makeUser(const std::string& login, const std::string& display, UserRole role, const std::string& password);
    /** @brief 从用户库读入内存 */
    bool load(std::string* error);
    /** @brief 把内存用户整表写回用户库 */
    bool save(std::string* error) const;

private:
    std::vector<UserAccount> m_users; ///< 用户列表
};

}

#endif
