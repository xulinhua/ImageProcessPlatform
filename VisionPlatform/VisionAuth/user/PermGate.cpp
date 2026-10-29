/**
 * @file PermGate.cpp
 * @brief PermGate实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PermGate.h"

namespace Vision {

std::string PermGate::toolRoot()
{
    return "tool";
}

std::string PermGate::toolNode(const std::string& toolName)
{
    return toolRoot() + "|" + toolName;
}

std::string PermGate::toolField(const std::string& toolName, const std::string& field)
{
    return toolNode(toolName) + "|" + field;
}

std::string PermGate::instRoot()
{
    return "inst";
}

std::string PermGate::instNode(const std::string& flowId, const std::string& toolId)
{
    return instRoot() + "|" + flowId + "|" + toolId;
}

std::string PermGate::instField(const std::string& flowId, const std::string& toolId, const std::string& field)
{
    return instNode(flowId, toolId) + "|" + field;
}

bool PermGate::allowed(const AppSettings& settings, UserRole role, const std::string& nodeId)
{
    if (role == RoleFactory || nodeId.empty()) {
        return true;
    }
    std::string cur = nodeId;
    while (!cur.empty()) {
        bool found = false;
        const bool on = settings.nodeGranted(role, cur, &found);
        if (found && !on) {
            return false;
        }
        const size_t bar = cur.rfind('|');
        if (bar == std::string::npos) {
            break;
        }
        cur.resize(bar);
    }
    return true;
}

}
