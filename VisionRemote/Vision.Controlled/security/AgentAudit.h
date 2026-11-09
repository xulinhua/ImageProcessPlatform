/**
 * @file AgentAudit.h
 * @brief 被控端本地审计，JSON Lines 按天落在 exe旁\\exe名\\log\\audit
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_AGENT_AUDIT_H
#define VISION_CONTROLLED_AGENT_AUDIT_H

#include <mutex>
#include <string>

namespace Vision {

/**
 * @class AgentAudit
 * @brief 追加一行 JSON。文件按本地日期分开，目录不存在时创建
 */
class AgentAudit {
public:
    /** @brief event 用点分短名，detail 是给人看的补充，可空 */
    void add(const char* event, const std::string& detail);

private:
    std::mutex m_mu;            ///< 保护按天追加的审计文件
};

}

#endif
