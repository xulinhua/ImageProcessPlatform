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
#include "TextEncoding.h"
#include <cstdio>
#include <cstdlib>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

// 账号表和权限表。权限一行是「某角色的某一项」
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
    }
    if (v < 2) {
        if (!db().exec(
                "CREATE TABLE IF NOT EXISTS perm ("
                "role INTEGER NOT NULL,"
                "perm_id TEXT NOT NULL,"
                "allowed INTEGER NOT NULL,"
                "PRIMARY KEY(role, perm_id)"
                ");",
                error)) {
            return false;
        }
        std::vector<std::vector<DbValue> > masks;
        if (db().query("SELECT role, mask FROM role_perm;", std::vector<DbValue>(), &masks, 0)) {
            for (size_t i = 0; i < masks.size(); ++i) {
                if (masks[i].size() < 2) {
                    continue;
                }
                if (!writeRolePerms(static_cast<int>(asInt(masks[i][0])),
                        static_cast<uint32_t>(asInt(masks[i][1])), error)) {
                    return false;
                }
            }
        }
        if (!mig.setVersion(2, error)) {
            return false;
        }
    }
    return true;
}

// 把掩码拆成权限行。调用方负责事务
bool AuthDb::writeRolePerms(int role, uint32_t mask, std::string* error)
{
    std::vector<DbValue> del;
    del.push_back(DbValue::fromInt(role));
    if (!db().exec("DELETE FROM perm WHERE role=?1;", del, error)) {
        return false;
    }
    for (int i = 0; i < permissionCount(); ++i) {
        const Perm bit = permissionAt(i);
        if (bit == PermNone) {
            continue;
        }
        const char* id = permissionId(bit);
        if (!id || !id[0]) {
            continue;
        }
        std::vector<DbValue> a;
        a.push_back(DbValue::fromInt(role));
        a.push_back(DbValue::fromText(id));
        a.push_back(DbValue::fromInt((mask & static_cast<uint32_t>(bit)) != 0 ? 1 : 0));
        if (!db().exec(
                "INSERT INTO perm(role, perm_id, allowed) VALUES(?1,?2,?3);",
                a, error)) {
            return false;
        }
    }
    return true;
}

// 打开权限库。空库时从旧 vision.db 搬一次
bool AuthDb::open(std::string* error)
{
    AppPaths::ensureDirs();
    if (!db().isOpen()) {
        if (!db().open(AppPaths::authFile(), error)) {
            return false;
        }
    }
    if (!migrateSchema(error)) {
        return false;
    }
    return importLegacy(error);
}

// 旧库不在或已经有用户时不动
bool AuthDb::importLegacy(std::string* error)
{
    std::vector<std::vector<DbValue> > countRows;
    if (!db().query("SELECT COUNT(*) FROM users;", std::vector<DbValue>(), &countRows, error)) {
        return false;
    }
    if (!countRows.empty() && !countRows[0].empty() && asInt(countRows[0][0]) > 0) {
        return true;
    }
    const std::wstring path = utf8ToWide(AppPaths::legacyDatabaseFile());
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    SqliteDatabase oldDb;
    if (!oldDb.open(AppPaths::legacyDatabaseFile(), 0)) {
        return true;
    }
    std::vector<std::vector<DbValue> > users;
    if (!oldDb.query(
            "SELECT id,login_name,display_name,role,salt,pwd_hash,hash_kind,enabled,"
            "must_change_pwd,created_at,last_login_at,fail_count,lock_until FROM users;",
            std::vector<DbValue>(), &users, 0)) {
        return true;
    }
    std::vector<std::vector<DbValue> > roles;
    oldDb.query("SELECT role, mask FROM role_perm;", std::vector<DbValue>(), &roles, 0);
    if (users.empty() && roles.empty()) {
        return true;
    }
    if (!db().begin(error)) {
        return false;
    }
    for (size_t i = 0; i < users.size(); ++i) {
        const std::vector<DbValue>& r = users[i];
        if (r.size() < 13) {
            continue;
        }
        std::vector<DbValue> a;
        for (size_t c = 0; c < 13; ++c) {
            a.push_back(r[c]);
        }
        if (!db().exec(
                "INSERT INTO users(id,login_name,display_name,role,salt,pwd_hash,hash_kind,enabled,"
                "must_change_pwd,created_at,last_login_at,fail_count,lock_until) "
                "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13);",
                a, error)) {
            db().rollback(0);
            return false;
        }
    }
    for (size_t i = 0; i < roles.size(); ++i) {
        if (roles[i].size() < 2) {
            continue;
        }
        if (!writeRolePerms(static_cast<int>(asInt(roles[i][0])),
                static_cast<uint32_t>(asInt(roles[i][1])), error)) {
            db().rollback(0);
            return false;
        }
    }
    return db().commit(error);
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
    if (!db().query("SELECT perm_id, allowed FROM perm WHERE role=?1;", args, &rows, error)) {
        return false;
    }
    if (rows.empty()) {
        return false;
    }
    uint32_t bits = 0;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].size() < 2 || asInt(rows[i][1]) == 0) {
            continue;
        }
        const Perm bit = permissionFromId(asText(rows[i][0]).c_str());
        if (bit != PermNone) {
            bits |= static_cast<uint32_t>(bit);
        }
    }
    *mask = bits | lockedPerms(role);
    return true;
}

// 写某角色的权限掩码
bool AuthDb::saveRoleMask(UserRole role, uint32_t mask, std::string* error)
{
    if (!open(error)) {
        return false;
    }
    mask |= lockedPerms(role);
    if (!db().begin(error)) {
        return false;
    }
    if (!writeRolePerms(static_cast<int>(role), mask, error)) {
        db().rollback(0);
        return false;
    }
    return db().commit(error);
}

}
