/**
 * @file Session.cpp
 * @brief Session实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Session.h"
#include "AppPaths.h"
#include "AuditLog.h"
#include "AuthDb.h"
#include "PermMgr.h"
#include "Pbkdf2.h"
#include "Translate.h"
#include "VpUtil.h"
#include <cstdio>
#include <ctime>
#include <string>

namespace Vision {

// 当前本地时间，ISO-8601 到秒
static std::string sessionIsoNow()
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

// 准备目录、内置权限和出厂用户
Session::Session()
    : m_listener(nullptr)
    , m_perms(0)
    , m_savedPerms(0)
    , m_elevated(false)
{
    AppPaths::ensureDirs();
    PermMgr::instance().ensureBuiltins();
    m_users.ensureDefaults();
}

// 设置界面回调，可空
void Session::setListener(ISessionListener* listener)
{
    m_listener = listener;
}

// 写入当前用户、刷新掩码并回调
void Session::applyUser(const UserAccount& u)
{
    m_user = u;
    m_user.salt.clear();
    m_user.pwdHash.clear();
    reloadPerms();
    if (m_listener) {
        m_listener->onUserChanged();
    }
}

// 按角色从库刷新掩码，并并上不能关掉的权限
void Session::reloadPerms()
{
    if (m_user.id.empty()) {
        m_perms = 0;
        return;
    }
    uint32_t mask = 0;
    if (!AuthDb::instance().loadRoleMask(m_user.role, &mask, 0)) {
        mask = m_settings.permMask(m_user.role);
    }
    m_perms = mask | lockedPerms(m_user.role);
    m_settings.setPermMask(m_user.role, m_perms);
}

// 点分标识是否落在当前权限掩码里
bool Session::has(const std::string& permId) const
{
    if (permId.empty() || !PermMgr::instance().contains(permId)) {
        return false;
    }
    const Perm bit = permissionFromId(permId.c_str());
    if (bit == PermNone) {
        return false;
    }
    return (m_perms & static_cast<uint32_t>(bit)) != 0;
}

// 权限位是否落在当前掩码里
bool Session::has(Perm p) const
{
    return has(std::string(permissionId(p)));
}

// 把内存用户库整表写回
bool Session::persistUsers()
{
    return AuthDb::instance().saveUsers(m_users, 0);
}

// 用给定账户替换当前用户，不含口令哈希
void Session::touchUser(const UserAccount& u)
{
    applyUser(u);
}

// 按 id 从用户库重新套用当前用户
void Session::refreshCurrentUser()
{
    UserAccount* u = m_users.findById(m_user.id);
    if (u) {
        applyUser(*u);
    }
}

// 校验口令后进入；失败把原因写到 error
bool Session::login(const std::string& loginName, const std::string& password, std::string* error)
{
    UserAccount* u = m_users.findByLogin(loginName);
    if (!u || !u->enabled) {
        auditLog("login_fail", vpLower(loginName), std::string());
        if (error) {
            *error = std::string(Translate::tr("user or password"));
        }
        return false;
    }
    const std::string now = sessionIsoNow();
    if (!u->lockUntil.empty() && u->lockUntil > now) {
        if (error) {
            *error = std::string(Translate::tr("account locked"));
        }
        return false;
    }
#ifndef _DEBUG
    if (password.empty()) {
        if (error) {
            *error = std::string(Translate::tr("empty password"));
        }
        return false;
    }
#endif
    if (!m_users.verifyPassword(*u, password)) {
        u->failCount += 1;
        if (u->failCount >= 5) {
            time_t t = time(0) + 5 * 60;
            struct tm lt;
            localtime_s(&lt, &t);
            char buf[32];
            sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d",
                    lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                    lt.tm_hour, lt.tm_min, lt.tm_sec);
            u->lockUntil = buf;
            u->failCount = 0;
        }
        persistUsers();
        auditLog("login_fail", vpLower(loginName), std::string());
        if (error) {
            *error = std::string(Translate::tr("user or password"));
        }
        return false;
    }
    if (u->hashKind != HashKindPbkdf2) {
        m_users.setPassword(u, password);
    }
    u->failCount = 0;
    u->lockUntil.clear();
    u->lastLoginAt = now;
    persistUsers();
    dropElevate();
    applyUser(*u);
    auditLog("login", u->loginName, std::to_string(static_cast<int>(u->role)));
    return true;
}

// 不校验口令，直接以该启用用户进入
bool Session::enterAs(const std::string& loginName)
{
    UserAccount* u = m_users.findByLogin(loginName);
    if (!u || !u->enabled) {
        return false;
    }
    applyUser(*u);
    auditLog("login", u->loginName, "auto");
    return true;
}

// 三角色权限恢复出厂并重新套用
void Session::resetDevPerms()
{
    m_settings.resetPermsToDefault();
    reloadPerms();
}

// 开发态进入管理员，并把角色权限恢复出厂
bool Session::enterAdminForDev()
{
    m_users.ensureDefaults();
    m_users.ensureBuiltinEmptyPasswords();
    m_users.ensureAdminAccount();
    UserAccount* admin = m_users.findFirstByRole(RoleAdmin, true);
    if (!admin) {
        admin = m_users.findByLogin("admin");
    }
    if (admin && admin->enabled) {
        applyUser(*admin);
    } else {
        for (int i = 0; i < m_users.count(); ++i) {
            const UserAccount& u = m_users.at(i);
            if (u.enabled && enterAs(u.loginName)) {
                break;
            }
        }
    }
    resetDevPerms();
    return isLoggedIn();
}

// 校验口令后临时换成该用户，并记住提权前的会话
bool Session::elevate(const std::string& loginName, const std::string& password,
                      const std::string& neededPerm, std::string* error)
{
    UserAccount* u = m_users.findByLogin(loginName);
    if (!u || !u->enabled || !m_users.verifyPassword(*u, password)) {
        if (error) {
            *error = std::string(Translate::tr("user or password"));
        }
        return false;
    }
    uint32_t mask = 0;
    if (!AuthDb::instance().loadRoleMask(u->role, &mask, 0)) {
        mask = defaultPerms(u->role) | m_settings.permMask(u->role);
    }
    mask |= lockedPerms(u->role);
    const Perm bit = permissionFromId(neededPerm.c_str());
    if (bit != PermNone && (mask & static_cast<uint32_t>(bit)) == 0) {
        if (error) {
            *error = std::string(Translate::tr("no permission"));
        }
        return false;
    }
    if (!m_elevated) {
        m_savedUser = m_user;
        m_savedPerms = m_perms;
        m_elevated = true;
    }
    applyUser(*u);
    auditLog("elevate", u->loginName, neededPerm);
    return true;
}

// 结束提权，回到提权前的用户
void Session::dropElevate()
{
    if (!m_elevated) {
        return;
    }
    m_elevated = false;
    if (!m_savedUser.id.empty()) {
        applyUser(m_savedUser);
    }
    m_savedUser = UserAccount();
    m_savedPerms = 0;
}

// 设置允许时，以操作员账户进入
bool Session::autoLoginOperator()
{
    if (!m_settings.autoLoginOperator) {
        return false;
    }
    UserAccount* u = m_users.findByLogin("operator");
    if (!u || !u->enabled || u->role != RoleOperator) {
        return false;
    }
    applyUser(*u);
    auditLog("login", u->loginName, "auto");
    return true;
}

// 通知界面刷新当前用户
void Session::notifyChanged()
{
    if (m_listener) {
        m_listener->onUserChanged();
    }
}

// 通知界面语言已切换
void Session::notifyUiLocaleChanged()
{
    if (m_listener) {
        m_listener->onUiLocaleChanged();
    }
}

// 清除当前用户并通知界面
void Session::logout()
{
    if (isLoggedIn()) {
        auditLog("logout", m_user.loginName, std::string());
    }
    dropElevate();
    m_user = UserAccount();
    m_user.id.clear();
    m_perms = 0;
    if (m_listener) {
        m_listener->onUserChanged();
    }
}

// 用户库
AuthMgr& Session::users()
{
    return m_users;
}

// 用户库
const AuthMgr& Session::users() const
{
    return m_users;
}

// 应用设置
AppSettings& Session::settings()
{
    return m_settings;
}

// 应用设置
const AppSettings& Session::settings() const
{
    return m_settings;
}

// 是否已登录
bool Session::isLoggedIn() const
{
    return !m_user.id.empty();
}

// 当前用户
const UserAccount& Session::user() const
{
    return m_user;
}

// 当前角色
UserRole Session::role() const
{
    return m_user.role;
}

// 当前权限掩码
uint32_t Session::perms() const
{
    return m_perms;
}

// 是否处于提权
bool Session::isElevated() const
{
    return m_elevated;
}



}
