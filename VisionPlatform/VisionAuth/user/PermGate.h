/**
 * @file PermGate.h
 * @brief 工具参数节点键。权限模块不打开方案
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PERM_GATE_H
#define VISION_PERM_GATE_H
#include "VisionAuthExport.h"

#include "AppSettings.h"
#include <string>

namespace Vision {

/**
 * @class PermGate
 * @brief 拼节点键，并按角色判断。调用方传入流程和工具 id
 */
class VISION_AUTH_API PermGate {
public:
    /** @brief 全部工具的根。勾选时下属一起动 */
    static std::string toolRoot();
    /** @brief 一种工具。tool|注册名 */
    static std::string toolNode(const std::string& toolName);
    /** @brief 一种工具下的参数 */
    static std::string toolField(const std::string& toolName, const std::string& field);
    /** @brief 全部实例的根 */
    static std::string instRoot();
    /** @brief 某条流程里的一个工具实例 */
    static std::string instNode(const std::string& flowId, const std::string& toolId);
    /** @brief 实例上的一个参数 */
    static std::string instField(const std::string& flowId, const std::string& toolId, const std::string& field);
    /** @brief 节点或任一上级被关掉则拒绝。没有记录视为允许。厂家始终允许 */
    static bool allowed(const AppSettings& settings, UserRole role, const std::string& nodeId);
};

}

#endif
