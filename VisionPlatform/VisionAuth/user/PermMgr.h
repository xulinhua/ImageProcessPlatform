/**
 * @file PermMgr.h
 * @brief PermMgr头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PERMISSIONCATALOG_H
#define VISION_PERMISSIONCATALOG_H
#include "VisionAuthExport.h"

#include "Perm.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class PermDesc
 * @brief 目录里的一条权限：点分标识、分组、权限位
 */
struct PermDesc {
    std::string id;    ///< 点分标识
    std::string group; ///< 分组，如 runtime / design
    Perm bit;    ///< 对应权限位
    PermDesc() : bit(PermNone) {}
};

/**
 * @class PermMgr
 * @brief 已知权限标识目录。未知标识一律视为无权限
 */
class VISION_AUTH_API PermMgr {
public:
    /** @brief 进程内唯一目录 */
    static PermMgr& instance();
    /** @brief 登记一条；同标识则覆盖 */
    void add(const PermDesc& d);
    /** @brief 目录为空时填入内置权限 */
    void ensureBuiltins();
    /** @brief 该标识是否已登记 */
    bool contains(const std::string& id) const;
    const std::vector<PermDesc>& all() const;
    /** @brief 读某角色的权限掩码 */
    bool loadRoleMask(UserRole role, uint32_t* mask, std::string* error);
    /** @brief 写某角色的权限掩码 */
    bool saveRoleMask(UserRole role, uint32_t mask, std::string* error);

private:
    PermMgr() {}
    std::vector<PermDesc> m_items; ///< 已登记权限
};

}

#endif
