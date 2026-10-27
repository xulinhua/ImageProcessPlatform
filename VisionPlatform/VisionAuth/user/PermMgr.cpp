/**
 * @file PermMgr.cpp
 * @brief PermMgr实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PermMgr.h"
#include "AuthDb.h"

namespace Vision {

// 进程内唯一权限目录
PermMgr& PermMgr::instance()
{
    static PermMgr g;
    return g;
}

// 登记一条权限；同标识则覆盖
void PermMgr::add(const PermDesc& d)
{
    if (d.id.empty()) {
        return;
    }
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == d.id) {
            m_items[i] = d;
            return;
        }
    }
    m_items.push_back(d);
}

// 目录为空时填入内置权限
void PermMgr::ensureBuiltins()
{
    if (!m_items.empty()) {
        return;
    }
    static const Perm bits[] = {
        PermRuntimeView, PermRuntimeStartStop, PermRuntimeResetCount, PermRuntimeSwitchSolution,
        PermEnterDesign, PermEditSolution, PermRunJobDebug,
        PermEditDevice, PermEditComm,
        PermManageUsers, PermEditAppSettings,
        PermViewLogs, PermManageLogs
    };
    static const char* groups[] = {
        "runtime", "runtime", "runtime", "runtime",
        "design", "design", "design",
        "device", "device",
        "system", "system",
        "log", "log"
    };
    for (int i = 0; i < 13; ++i) {
        PermDesc d;
        d.bit = bits[i];
        d.id = permissionId(bits[i]);
        d.group = groups[i];
        add(d);
    }
}

// 该标识是否已登记
bool PermMgr::contains(const std::string& id) const
{
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) {
            return true;
        }
    }
    return false;
}

// 读角色权限掩码
bool PermMgr::loadRoleMask(UserRole role, uint32_t* mask, std::string* error)
{
    return AuthDb::instance().loadRoleMask(role, mask, error);
}

// 写角色权限掩码
bool PermMgr::saveRoleMask(UserRole role, uint32_t mask, std::string* error)
{
    return AuthDb::instance().saveRoleMask(role, mask, error);
}

// 全部权限项
const std::vector<PermDesc>& PermMgr::all() const
{
    return m_items;
}



}
