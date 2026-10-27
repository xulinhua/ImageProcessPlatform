/**
 * @file AuthDb.cpp
 * @brief AuthDb实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AuthDb.h"
#include "SqliteDatabase.h"
#include "AppPaths.h"
#include <cstdio>
#include <cstdlib>

namespace Vision {

namespace {

// 本模块共用的 SQLite 连接
SqliteDatabase& db()
{
    static SqliteDatabase g;
    return g;
}

// 字节数组包成绑定用的二进制单元格
DbValue blobVal(const std::vector<unsigned char>& b)
{
    return DbValue::fromBlob(b);
}

// 从单元格取出二进制
std::vector<unsigned char> asBlob(const DbValue& v)
{
    if (v.kind == DbValue::Blob) {
        return v.blob;
    }
    if (v.kind == DbValue::Text) {
        return std::vector<unsigned char>(v.text.begin(), v.text.end());
    }
    return std::vector<unsigned char>();
}

// 从单元格取出文本
std::string asText(const DbValue& v)
{
    if (v.kind == DbValue::Text) {
        return v.text;
    }
    if (v.kind == DbValue::Int) {
        char buf[32];
        sprintf(buf, "%lld", static_cast<long long>(v.i));
        return std::string(buf);
    }
    return std::string();
}

// 从单元格取出整数
int64_t asInt(const DbValue& v)
{
    if (v.kind == DbValue::Int) {
        return v.i;
    }
    if (v.kind == DbValue::Text) {
        return atoi(v.text.c_str());
    }
    return 0;
}

} // namespace

// 进程内唯一用户库
AuthDb& AuthDb::instance()
{
    static AuthDb g;
    return g;
}

// 创建用户表和角色权限表
bool AuthDb::migrateSchema(std::string* error)
{
    SchemaMigrator mig(&db());
    if (!mig.ensureVersionTable(error)) {
        return false;
    }
    const int v = mig.currentVersion(error);
    if (v < 0) {
        return false;
    }
    if (v < 1) {
        if (!db().exec(
                "CREATE TABLE IF NOT EXISTS users ("
                "id TEXT PRIMARY KEY,"
                "login_name TEXT UNIQUE NOT NULL,"
                "display_name TEXT,"
                "role INTEGER,"
                "salt BLOB,"
                "pwd_hash BLOB,"
                "hash_kind INTEGER,"
                "enabled INTEGER,"
                "must_change_pwd INTEGER,"
                "created_at TEXT,"
                "last_login_at TEXT,"
                "fail_count INTEGER,"
                "lock_until TEXT"
                ");",
                error)) {
            return false;
        }
        if (!db().exec(
                "CREATE TABLE IF NOT EXISTS role_perm ("
                "role INTEGER NOT NULL,"
                "mask INTEGER NOT NULL,"
                "PRIMARY KEY(role)"
                ");",
                error)) {
            return false;
        }
        if (!mig.setVersion(1, error)) {
            return false;
        }
    }
    return true;
}

// 打开用户库并升级表结构
bool AuthDb::open(std::string* error)
{
    AppPaths::ensureDirs();
    if (!db().isOpen()) {
        if (!db().open(AppPaths::databaseFile(), error)) {
            return false;
        }
    }
    return migrateSchema(error);
}

// 关闭数据库
void AuthDb::close()
{
    db().close();
}

// 读出全部用户
bool AuthDb::loadUsers(AuthMgr* users, std::string* error)
{
    if (!users || !open(error)) {
        return false;
    }
    std::vector<std::vector<DbValue> > rows;
    if (!db().query(
            "SELECT id,login_name,display_name,role,salt,pwd_hash,hash_kind,enabled,"
            "must_change_pwd,created_at,last_login_at,fail_count,lock_until FROM users;",
            std::vector<DbValue>(), &rows, error)) {
        return false;
    }
    users->clear();
    for (size_t i = 0; i < rows.size(); ++i) {
        const std::vector<DbValue>& r = rows[i];
        if (r.size() < 13) {
            continue;
        }
        UserAccount u;
        u.id = asText(r[0]);
        u.loginName = asText(r[1]);
        u.displayName = asText(r[2]);
        u.role = roleFromInt(static_cast<int>(asInt(r[3])));
        u.salt = asBlob(r[4]);
        u.pwdHash = asBlob(r[5]);
        u.hashKind = static_cast<int>(asInt(r[6]));
        u.enabled = asInt(r[7]) != 0;
        u.mustChangePwd = asInt(r[8]) != 0;
        u.createdAt = asText(r[9]);
        u.lastLoginAt = asText(r[10]);
        u.failCount = static_cast<int>(asInt(r[11]));
        u.lockUntil = asText(r[12]);
        if (!u.id.empty() && !u.loginName.empty()) {
            users->addUser(u);
        }
    }
    return true;
}

// 清空后整表写回用户
bool AuthDb::saveUsers(const AuthMgr& users, std::string* error)
{
    if (!open(error)) {
        return false;
    }
    if (!db().begin(error)) {
        return false;
    }
    if (!db().exec("DELETE FROM users;", error)) {
        db().rollback(0);
        return false;
    }
    for (int i = 0; i < users.count(); ++i) {
        const UserAccount& u = users.at(i);
        std::vector<DbValue> a;
        a.push_back(DbValue::fromText(u.id));
        a.push_back(DbValue::fromText(u.loginName));
        a.push_back(DbValue::fromText(u.displayName));
        a.push_back(DbValue::fromInt(u.role));
        a.push_back(blobVal(u.salt));
        a.push_back(blobVal(u.pwdHash));
        a.push_back(DbValue::fromInt(u.hashKind));
        a.push_back(DbValue::fromInt(u.enabled ? 1 : 0));
        a.push_back(DbValue::fromInt(u.mustChangePwd ? 1 : 0));
        a.push_back(DbValue::fromText(u.createdAt));
        a.push_back(DbValue::fromText(u.lastLoginAt));
        a.push_back(DbValue::fromInt(u.failCount));
        a.push_back(DbValue::fromText(u.lockUntil));
        if (!db().exec(
                "INSERT INTO users(id,login_name,display_name,role,salt,pwd_hash,hash_kind,enabled,"
                "must_change_pwd,created_at,last_login_at,fail_count,lock_until) "
                "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13);",
                a, error)) {
            db().rollback(0);
            return false;
        }
    }
    return db().commit(error);
}

// 读某角色的权限掩码
bool AuthDb::loadRoleMask(UserRole role, uint32_t* mask, std::string* error)
{
    if (!mask || !open(error)) {
        return false;
    }
    std::vector<DbValue> args;
    args.push_back(DbValue::fromInt(role));
    std::vector<std::vector<DbValue> > rows;
    if (!db().query("SELECT mask FROM role_perm WHERE role=?1;", args, &rows, error)) {
        return false;
    }
    if (rows.empty() || rows[0].empty()) {
        return false;
    }
    *mask = static_cast<uint32_t>(asInt(rows[0][0])) | lockedPerms(role);
    return true;
}

// 写某角色的权限掩码
bool AuthDb::saveRoleMask(UserRole role, uint32_t mask, std::string* error)
{
    if (!open(error)) {
        return false;
    }
    mask |= lockedPerms(role);
    std::vector<DbValue> a;
    a.push_back(DbValue::fromInt(role));
    a.push_back(DbValue::fromInt(mask));
    return db().exec("INSERT OR REPLACE INTO role_perm(role,mask) VALUES(?1,?2);", a, error);
}

}
