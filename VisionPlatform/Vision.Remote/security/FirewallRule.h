/**
 * @file FirewallRule.h
 * @brief 为本程序的监听端口加一条 Windows 防火墙入站规则
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_FIREWALL_RULE_H
#define VISION_REMOTE_FIREWALL_RULE_H

#include <string>

namespace Vision {

/**
 * @class FirewallRule
 * @brief 调用 netsh。没有管理员权限时失败，调用方提示用户
 */
class FirewallRule {
public:
    /**
     * @brief 允许 exe 在 port 上入站 TCP
     * @param name 规则名，重复添加会先删同名规则
     */
    bool allowTcp(const std::wstring& name, const std::wstring& exe, int port, std::string* err) const;
};

}

#endif
