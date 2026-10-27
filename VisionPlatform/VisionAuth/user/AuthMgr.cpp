/**
 * @file AuthMgr.cpp
 * @brief AuthMgr实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AuthMgr.h"
#include "AuthDb.h"
#include "Translate.h"
#include "ObjectId.h"
#include "Sha256.h"
#include "VpUtil.h"
#include <cstdio>
#include <ctime>
#include <rpc.h>
#include <windows.h>

#pragma comment(lib, "rpcrt4.lib")

namespace Vision {

// 本地时间 ISO-8601（无时区）
static std::string isoNow()
{
    time_t t = time(0);
    struct tm lt;
    localtime_s(&lt, &t);
    char buf[32];
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d",
            lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
            lt.tm_hour, lt.tm_min, lt.tm_sec);
    return std::string(buf);
}

// 用 UUID 16 字节作盐
static std::vector<unsigned char> randomSalt()
{
    UUID u;
    UuidCreate(&u);
    const unsigned char* p = reinterpret_cast<const unsigned char*>(&u);
    return std::vector<unsigned char>(p, p + 16);
}

// SHA256(salt || password)
std::vector<unsigned char> AuthMgr::hashPassword(const std::vector<unsigned char>& salt, const std::string& password)
{
    std::vector<unsigned char> buf = salt;
    buf.insert(buf.end(), password.begin(), password.end());
    unsigned char dig[32];
    sha256(buf.empty() ? 0 : &buf[0], buf.size(), dig);
    return std::vector<unsigned char>(dig, dig + 32);
}

// 将出厂三账户口令重置为空
UserAccount AuthMgr::makeUser(const std::string& login, const std::string& display, UserRole role, const std::string& password)
{
    UserAccount u;
    u.id = ObjectId::create().toString();
    u.loginName = vpLower(login);
    u.displayName = display;
    u.role = role;
    u.salt = randomSalt();
    u.pwdHash = hashPassword(u.salt, password);
    u.enabled = true;
    u.mustChangePwd = false;
    u.createdAt = isoNow();
    return u;
}

// 将出厂三账户口令重置为空
bool AuthMgr::verifyPassword(const UserAccount& u, const std::string& password) const
{
    const std::vector<unsigned char> h = hashPassword(u.salt, password);
    return h == u.pwdHash;
}

// 将出厂三账户口令重置为空
void AuthMgr::clear()
{
    m_users.clear();
}

// 按 id 删除
UserAccount* AuthMgr::findById(const std::string& id)
{
    for (int i = 0; i < static_cast<int>(m_users.size()); ++i) {
        if (m_users[static_cast<size_t>(i)].id == id) {
            return &m_users[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 将出厂三账户口令重置为空
UserAccount* AuthMgr::findByLogin(const std::string& loginName)
{
    const std::string key = vpLower(loginName);
    for (int i = 0; i < static_cast<int>(m_users.size()); ++i) {
        if (m_users[static_cast<size_t>(i)].loginName == key) {
            return &m_users[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 将出厂三账户口令重置为空
const UserAccount* AuthMgr::findByLogin(const std::string& loginName) const
{
    const std::string key = vpLower(loginName);
    for (int i = 0; i < static_cast<int>(m_users.size()); ++i) {
        if (m_users[static_cast<size_t>(i)].loginName == key) {
            return &m_users[static_cast<size_t>(i)];
        }
    }
    return 0;
}

// 将出厂三账户口令重置为空
bool AuthMgr::addUser(const UserAccount& u)
{
    if (u.loginName.empty() || findByLogin(u.loginName)) {
        return false;
    }
    m_users.push_back(u);
    return true;
}

// 按 id 删除
bool AuthMgr::removeById(const std::string& id)
{
    for (int i = 0; i < static_cast<int>(m_users.size()); ++i) {
        if (m_users[static_cast<size_t>(i)].id == id) {
            vpRemoveAt(m_users, i);
            return true;
        }
    }
    return false;
}

// 将出厂三账户口令重置为空
bool AuthMgr::setPassword(UserAccount* u, const std::string& password)
{
    if (!u) {
        return false;
    }
    u->salt = randomSalt();
    u->pwdHash = hashPassword(u->salt, password);
    u->mustChangePwd = false;
    return true;
}

// 将出厂三账户口令重置为空
int AuthMgr::countRole(UserRole role, bool enabledOnly) const
{
    int n = 0;
    for (int i = 0; i < static_cast<int>(m_users.size()); ++i) {
        if (m_users[static_cast<size_t>(i)].role != role) {
            continue;
        }
        if (enabledOnly && !m_users[static_cast<size_t>(i)].enabled) {
            continue;
        }
        ++n;
    }
    return n;
}

// 将出厂三账户口令重置为空
UserAccount* AuthMgr::findFirstByRole(UserRole role, bool enabledOnly)
{
    for (int i = 0; i < static_cast<int>(m_users.size()); ++i) {
        UserAccount& u = m_users[static_cast<size_t>(i)];
        if (u.role != role) {
            continue;
        }
        if (enabledOnly && !u.enabled) {
            continue;
        }
        return &u;
    }
    return 0;
}

// 将出厂三账户口令重置为空
bool AuthMgr::ensureAdminAccount()
{
    if (findFirstByRole(RoleAdmin, true)) {
        return false;
    }
    UserAccount* a = findByLogin("admin");
    if (a) {
        a->role = RoleAdmin;
        a->enabled = true;
        return true;
    }
    return addUser(makeUser("admin", std::string(Translate::tr("管理员")), RoleAdmin, std::string()));
}

// 空库时写入 admin/engineer/operator，默认无口令
bool AuthMgr::ensureDefaults()
{
    if (!m_users.empty()) {
        return true;
    }
    addUser(makeUser("admin", std::string(Translate::tr("管理员")), RoleAdmin, std::string()));
    addUser(makeUser("engineer", std::string(Translate::tr("工程师")), RoleEngineer, std::string()));
    addUser(makeUser("operator", std::string(Translate::tr("操作员")), RoleOperator, std::string()));
    return true;
}

// 将出厂三账户口令重置为空
bool AuthMgr::ensureBuiltinEmptyPasswords()
{
    static const char* kBuiltin[] = { "admin", "engineer", "operator" };
    bool changed = false;
    for (int i = 0; i < 3; ++i) {
        UserAccount* u = findByLogin(kBuiltin[i]);
        if (!u) {
            continue;
        }
        if (!verifyPassword(*u, std::string())) {
            setPassword(u, std::string());
            changed = true;
        }
    }
    return changed;
}

// 从用户库读入
bool AuthMgr::load(std::string* error)
{
    return AuthDb::instance().loadUsers(this, error);
}

// 把内存用户整表写回
bool AuthMgr::save(std::string* error) const
{
    return AuthDb::instance().saveUsers(*this, error);
}


// 用户数
int AuthMgr::count() const
{
    return static_cast<int>(m_users.size());
}

// 第 i 个用户
const UserAccount& AuthMgr::at(int i) const
{
    return m_users[static_cast<size_t>(i)];
}

}
